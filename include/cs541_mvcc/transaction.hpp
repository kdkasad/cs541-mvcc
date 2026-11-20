#pragma once

#include "cs541_mvcc/version.hpp"

#include <expected>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cs541_mvcc {
class Store;
struct Active {};
struct Aborted {};
struct Preparing {
    Timestamp end_ts;
};
struct Committed {
    Timestamp end_ts;
};
using TxnStatus = std::variant<Active, Aborted, Preparing, Committed>;

class TxnMetadata {
    mutex_wrapper::Mutex<TxnStatus> status_;
    friend class Transaction;

public:
    const TxnId id;
    const Timestamp begin_ts;
    TxnMetadata(TxnId id, Timestamp begin_ts);
    [[nodiscard]] TxnStatus status() const;
};

struct WriteEntry {
    std::shared_ptr<Version> old;
    std::shared_ptr<Version> new_version;
};

enum class WriteError {
    /// The given key is not in the range covered by the store.
    KeyOutOfRange,
    /// A write-write conflict happened.
    /// Another transaction updated the same key during our transaction.
    Conflict,
};
enum class CommitError { NotSerializable };

class Transaction;
class TransactionManager : public std::enable_shared_from_this<TransactionManager> {
    struct TransactionManagerInner {
        std::uint64_t next_id = 1;
        std::uint64_t next_timestamp = 1;
        std::unordered_map<std::uint64_t, std::shared_ptr<TxnMetadata>> transactions;
    };
    mutex_wrapper::Mutex<TransactionManagerInner> inner_;
    TransactionManager();
    void remove(TxnId id);
    friend class Transaction;

public:
    static std::shared_ptr<TransactionManager> create();
    Transaction begin();
    std::shared_ptr<TxnMetadata> get(TxnId id) const;
};

class Transaction {
    std::shared_ptr<TxnMetadata> metadata_;
    std::vector<std::shared_ptr<Version>> read_set_;
    std::unordered_map<Key, WriteEntry> write_set_;
    std::shared_ptr<TransactionManager> manager_;
    Transaction(std::shared_ptr<TxnMetadata> metadata, std::shared_ptr<TransactionManager> manager);
    friend class TransactionManager;
    /// Panic if called on an aborted/committed transaction
    void panic_if_not_active() const;
    std::optional<Value> optimistic_read_impl(const Store& store, Key key);

public:
    // allow move but not copy
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) noexcept = default;
    Transaction& operator=(Transaction&&) noexcept = default;

    std::optional<Value> optimistic_read(const Store& store, Key key);
    std::optional<std::vector<Value>> optimistic_scan(const Store& store);
    std::expected<void, WriteError> write(const Store& store, Key key, Value value);
    void abort(const Store& store);
    std::expected<void, CommitError> optimistic_commit(const Store& store);
};
}
