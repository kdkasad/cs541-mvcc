#pragma once

#include "cs541_mvcc/mutex_wrapper/mutex.hpp"
#include "cs541_mvcc/timestamp.hpp"

#include <memory>
#include <variant>

namespace cs541_mvcc {
struct TxnId {
    std::uint64_t value;
    auto operator<=>(const TxnId&) const = default;
};
struct Infinity {
    bool operator==(const Infinity&) const = default;
};
using TxnIdOrTimestamp = std::variant<Infinity, Timestamp, TxnId>;
using Key = std::uint64_t;
using Value = std::uint64_t;
class TransactionManager;

struct Version {
    Key key;
    Value value;
    mutex_wrapper::Mutex<TxnIdOrTimestamp> begin;
    mutex_wrapper::Mutex<TxnIdOrTimestamp> end;
    mutex_wrapper::Mutex<std::shared_ptr<Version>> previous;

    Version(Key key, Value value, TxnIdOrTimestamp begin, TxnIdOrTimestamp end,
            std::shared_ptr<Version> previous);
    bool is_visible_at(Timestamp ts, TxnId reader, const TransactionManager& txn_manager) const;
};
}
