#include "cs541_mvcc/version.hpp"
#include "cs541_mvcc/transaction.hpp"

#include <iostream>
#include <stdexcept>

namespace cs541_mvcc {
Version::Version(Key key, Value value, TxnIdOrTimestamp begin, TxnIdOrTimestamp end,
                 std::shared_ptr<Version> previous)
    : key(key), value(value), begin(std::move(begin)), end(std::move(end)),
      previous(std::move(previous)) {}

bool Version::is_visible_at(Timestamp ts, TxnId reader,
                            const TransactionManager& txn_manager) const {
    // Hold the locks so we don't hit a race condition between reading the
    // fields and checking the manager for the transactions they reference
    auto begin = this->begin.lock();
    auto end = this->end.lock();
    const auto txn_status = [&](TxnId id) {
        auto metadata = txn_manager.get(id);
        if (!metadata) {
            throw std::logic_error("version references transaction that doesn't exist");
        }
        return metadata->status();
    };
    const auto visible_according_to_begin = [&] {
        if (std::holds_alternative<Infinity>(*begin)) {
            return false;
        }
        if (const auto* begin_ts = std::get_if<Timestamp>(&*begin)) {
            return *begin_ts <= ts;
        }
        const auto writer = std::get<TxnId>(*begin);
        if (writer == reader) {
            return std::holds_alternative<Infinity>(*end);
        }
        const auto status = txn_status(writer);
        if (std::holds_alternative<Active>(status) || std::holds_alternative<Aborted>(status)) {
            return false;
        }
        if (const auto* committed = std::get_if<Committed>(&status)) {
            return committed->end_ts <= ts;
        }
        std::cerr << "Making version invisible because the writer is preparing\n";
        return false;
    }();
    if (!visible_according_to_begin) {
        return false;
    }
    if (std::holds_alternative<Infinity>(*end)) {
        return true;
    }
    if (const auto* end_ts = std::get_if<Timestamp>(&*end)) {
        return ts < *end_ts;
    }
    const auto writer = std::get<TxnId>(*end);
    const auto status = txn_status(writer);
    if (std::holds_alternative<Active>(status)) {
        // See errata for this case. Larson paper is wrong.
        return writer != reader;
    }
    if (std::holds_alternative<Aborted>(status)) {
        return true;
    }
    if (const auto* committed = std::get_if<Committed>(&status)) {
        return ts < committed->end_ts;
    }
    std::cerr << "Making version invisible because the writer is preparing\n";
    return false;
}
}
