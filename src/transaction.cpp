#include "cs541_mvcc/transaction.hpp"
#include "cs541_mvcc/store.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace cs541_mvcc {
TxnMetadata::TxnMetadata(TxnId id, Timestamp begin_ts)
    : status_(Active{}), id(id), begin_ts(begin_ts) {}

TxnStatus TxnMetadata::status() const { return *status_.lock(); }

TransactionManager::TransactionManager() : inner_(TransactionManagerInner{}) {}

std::shared_ptr<TransactionManager> TransactionManager::create() {
    return std::shared_ptr<TransactionManager>(new TransactionManager());
}

Transaction TransactionManager::begin() {
    auto inner = inner_.lock();
    const TxnId id{inner->next_id++};
    const Timestamp begin_ts{inner->next_timestamp++};
    auto metadata = std::make_shared<TxnMetadata>(id, begin_ts);
    inner->transactions.emplace(id.value, metadata);
    return Transaction(std::move(metadata), shared_from_this());
}

std::shared_ptr<TxnMetadata> TransactionManager::get(TxnId id) const {
    auto inner = inner_.lock();
    const auto found = inner->transactions.find(id.value);
    return found == inner->transactions.end() ? nullptr : found->second;
}

void TransactionManager::remove(TxnId id) { inner_.lock()->transactions.erase(id.value); }

Transaction::Transaction(std::shared_ptr<TxnMetadata> metadata,
                         std::shared_ptr<TransactionManager> manager)
    : metadata_(std::move(metadata)), manager_(std::move(manager)) {}

void Transaction::panic_if_not_active() const {
    if (!std::holds_alternative<Active>(metadata_->status())) {
        throw std::logic_error("transaction must be active");
    }
}

std::optional<Value> Transaction::optimistic_read(const Store& store, Key key) {
    panic_if_not_active();
    return optimistic_read_impl(store, key);
}

std::optional<Value> Transaction::optimistic_read_impl(const Store& store, Key key) {
    auto current = store.get_latest_version(key);
    while (current) {
        auto version = current;
        if (version->is_visible_at(metadata_->begin_ts, metadata_->id, *manager_)) {
            const auto value = version->value;
            read_set_.push_back(version);
            return value;
        }
        current = *version->previous.lock();
    }
    return std::nullopt;
}

std::optional<std::vector<Value>> Transaction::optimistic_scan(const Store& store) {
    panic_if_not_active();
    std::vector<Value> values;
    for (const auto key : store.key_range()) {
        const auto value = optimistic_read_impl(store, key);
        if (!value) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

std::expected<void, WriteError> Transaction::write(const Store& store, Key key, Value value) {
    panic_if_not_active();
    if (std::ranges::any_of(write_set_,
                            [key](const auto& entry) { return entry.new_version->key == key; })) {
        return std::unexpected(WriteError::AlreadyWritten);
    }
    auto entry = store.install_update(key, value, metadata_->id, metadata_->begin_ts);
    if (!entry) {
        return std::unexpected(entry.error());
    }
    write_set_.push_back(std::move(*entry));
    return {};
}

void Transaction::abort(const Store& store) {
    const auto status = metadata_->status();
    if (!std::holds_alternative<Active>(status) && !std::holds_alternative<Preparing>(status)) {
        throw std::logic_error("transaction must be active or preparing");
    }
    *metadata_->status_.lock() = Aborted{};
    for (auto& [old, new_version] : std::exchange(write_set_, {})) {
        // Larson et al. sets the begin timestamp to infinity. Instead, we
        // just remove the version from the list.
        store.remove_head(new_version);
        auto end = old->end.lock();
        if (const auto* writer = std::get_if<TxnId>(&*end); writer && *writer == metadata_->id) {
            *end = Infinity{};
        }
    }
    // All references to this transaction are now gone, so we can remove it from the manager
    manager_->remove(metadata_->id);
}

std::expected<void, CommitError> Transaction::optimistic_commit(const Store& store) {
    panic_if_not_active();
    const auto end_ts = [&] {
        // We need to update our transaction's status while still holding the manager lock
        auto manager_inner = manager_->inner_.lock();
        const Timestamp end_ts{manager_inner->next_timestamp++};
        *metadata_->status_.lock() = Preparing{end_ts};
        return end_ts;
    }();

    // Validate read set
    const auto is_us = [&](const TxnIdOrTimestamp& tt) {
        const auto* writer = std::get_if<TxnId>(&tt);
        return writer && *writer == metadata_->id;
    };
    bool should_abort = false;
    for (const auto& version : read_set_) {
        auto begin = version->begin.lock();
        auto end = version->end.lock();
        if (is_us(*begin)) {
            // Valid because we created the version so nobody else could've updated it
            continue;
        }
        if (is_us(*end)) {
            // Valid because we updated the version, so nobody else could've at the same time
            continue;
        }
        // We only need to check the end, because the fact we read this
        // means the begin timestamp was valid
        const bool passes = [&] {
            if (std::holds_alternative<Infinity>(*end)) {
                return true;
            }
            if (const auto* ts = std::get_if<Timestamp>(&*end)) {
                return end_ts < *ts;
            }
            const auto writer = std::get<TxnId>(*end);
            auto metadata = manager_->get(writer);
            if (!metadata) {
                throw std::logic_error("version references transaction that doesn't exist");
            }
            auto status = metadata->status_.lock();
            if (std::holds_alternative<Aborted>(*status) ||
                std::holds_alternative<Active>(*status)) {
                return true;
            }
            if (const auto* committed = std::get_if<Committed>(&*status)) {
                return end_ts < committed->end_ts;
            }
            const auto ts = std::get<Preparing>(*status).end_ts;
            if (end_ts < ts) {
                // Version being prepared ends after our end
                // time anyways, so it doesn't matter if it
                // commits or aborts
                return true;
            }
            std::cerr
                << "Aborting transaction because commit dependencies are not implemented yet\n";
            return false;
        }();
        if (!passes) {
            should_abort = true;
            break;
        }
    }
    if (should_abort) {
        abort(store);
        return std::unexpected(CommitError::NotSerializable);
    }

    // Validation successful
    *metadata_->status_.lock() = Committed{end_ts};

    // Update written versions
    for (auto& [old, new_version] : std::exchange(write_set_, {})) {
        *new_version->begin.lock() = end_ts;
        auto end = old->end.lock();
        if (const auto* writer = std::get_if<TxnId>(&*end); writer && *writer == metadata_->id) {
            *end = end_ts;
        }
    }
    manager_->remove(metadata_->id);
    return {};
}
}
