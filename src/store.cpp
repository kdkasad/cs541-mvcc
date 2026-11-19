#include "cs541_mvcc/store.hpp"

#include <limits>
#include <stdexcept>

namespace cs541_mvcc {
Store::Store(std::vector<Value> values) : map_(values.size()) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto key = static_cast<Key>(index);
        *map_[index].lock() =
            std::make_shared<Version>(key, values[index], Timestamp{0}, Infinity{}, nullptr);
    }
}

std::shared_ptr<Version> Store::get_latest_version(Key key) const {
    if (key > std::numeric_limits<std::size_t>::max() || key >= map_.size()) {
        return nullptr;
    }
    return *map_[static_cast<std::size_t>(key)].lock();
}

std::expected<WriteEntry, WriteError> Store::install_update(Key key, Value value, TxnId writer,
                                                            Timestamp read_ts) const {
    if (key > std::numeric_limits<std::size_t>::max() || key >= map_.size()) {
        return std::unexpected(WriteError::KeyOutOfRange);
    }
    auto head = map_[static_cast<std::size_t>(key)].lock();
    auto old = *head;
    auto new_version = std::make_shared<Version>(key, value, writer, Infinity{}, old);
    {
        auto begin = old->begin.lock();
        auto end = old->end.lock();
        if (const auto* ts = std::get_if<Timestamp>(&*begin)) {
            if (*ts > read_ts) {
                return std::unexpected(WriteError::Conflict);
            }
        } else if (std::holds_alternative<TxnId>(*begin)) {
            return std::unexpected(WriteError::Conflict);
        } else {
            throw std::logic_error("begin should not be infinity");
        }
        if (!std::holds_alternative<Infinity>(*end)) {
            return std::unexpected(WriteError::Conflict);
        }
        *end = writer;
        *head = new_version;
    }
    return WriteEntry{old, new_version};
}

void Store::remove_head(std::shared_ptr<Version> version) const {
    const auto key = version->key;
    auto head = map_.at(static_cast<std::size_t>(key)).lock();
    if (version != *head) {
        throw std::logic_error("version must be the head");
    }
    auto new_head = *(*head)->previous.lock();
    if (!new_head) {
        throw std::logic_error("head must have a previous version");
    }
    *head = std::move(new_head);
}
}
