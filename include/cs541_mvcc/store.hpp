#pragma once

#include "cs541_mvcc/transaction.hpp"

#include <vector>
#include <ranges>

namespace cs541_mvcc {
class Store {
    /// Key is the index. Value is the version chain for that key.
    std::vector<mutex_wrapper::Mutex<std::shared_ptr<Version>>> map_;
    std::expected<WriteEntry, WriteError> install_update(Key key, Value value, TxnId writer,
                                                         Timestamp read_ts) const;
    friend class Transaction;

public:
    explicit Store(std::vector<Value> values);
    auto key_range() const { return std::views::iota(Key{0}, static_cast<Key>(map_.size())); }
    std::shared_ptr<Version> get_latest_version(Key key) const;
    void remove_head(std::shared_ptr<Version> version) const;
};
}
