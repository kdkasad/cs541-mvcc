#include "cs541_mvcc/transaction.hpp"
#include "utils.h"
#include "cs541_mvcc/lib.hpp"

#include <iostream>
#include <numeric>
#include <string_view>
#include <thread>

using namespace cs541_mvcc;

void simple_read_existing() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
}

void simple_read_missing() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT_EQ(std::nullopt, t1.optimistic_read(store, 20));
}

void read_ignores_earlier_uncommitted_write() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{2}, t1.optimistic_read(store, 0));
    ASSERT_EQ(std::optional<Value>{1}, t2.optimistic_read(store, 0));
}

// This test is different from the above because the writer transaction is
// started after the reader
void read_ignores_later_uncommitted_write() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT(t2.write(store, 0, 2).has_value());
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    ASSERT_EQ(std::optional<Value>{2}, t2.optimistic_read(store, 0));
}

void simple_scan() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT_EQ((std::optional<std::vector<Value>>{std::vector<Value>{1, 2, 3, 4, 5}}),
              t1.optimistic_scan(store));
    ASSERT(t1.write(store, 0, 2).has_value());
    ASSERT_EQ((std::optional<std::vector<Value>>{std::vector<Value>{2, 2, 3, 4, 5}}),
              t1.optimistic_scan(store));
}

void read_after_commit() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    ASSERT(t1.optimistic_commit(store).has_value());
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{2}, t2.optimistic_read(store, 0));
}

void read_before_and_after_commit() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    ASSERT(t2.write(store, 0, 2).has_value());
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    ASSERT(t2.optimistic_commit(store).has_value());
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
}

void conflicting_writes_abort_second_writer() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    // Larson et al. section 2.6:
    // > This is a write-write conflict. We follow the
    // > first-writer-wins rule and force transaction T to abort.
    ASSERT_EQ(std::unexpected(WriteError::Conflict), t2.write(store, 0, 3));
    ASSERT_EQ(std::optional<Value>{2}, t1.optimistic_read(store, 0));
    ASSERT_EQ(std::optional<Value>{1}, t2.optimistic_read(store, 0));
    ASSERT(t1.optimistic_commit(store).has_value());
    ASSERT_EQ(std::unexpected(CommitError::NotSerializable), t2.optimistic_commit(store));
    auto t3 = manager->begin();
    ASSERT_EQ(std::optional<Value>{2}, t3.optimistic_read(store, 0));
}

void aborted_write_is_ignored() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    ASSERT_EQ(std::optional<Value>{2}, t1.optimistic_read(store, 0));
    t1.abort(store);
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t2.optimistic_read(store, 0));
}

void write_after_aborted_write_succeeds() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    ASSERT_EQ(std::optional<Value>{2}, t1.optimistic_read(store, 0));
    t1.abort(store);
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t2.optimistic_read(store, 0));
    ASSERT(t2.write(store, 0, 3).has_value());
    ASSERT_EQ(std::optional<Value>{3}, t2.optimistic_read(store, 0));
    ASSERT(t2.optimistic_commit(store).has_value());
    auto t3 = manager->begin();
    ASSERT_EQ(std::optional<Value>{3}, t3.optimistic_read(store, 0));
}

void non_repeatable_read_is_aborted() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    ASSERT(t2.write(store, 0, 2).has_value());
    ASSERT(t2.optimistic_commit(store).has_value());
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    ASSERT_EQ(std::unexpected(CommitError::NotSerializable), t1.optimistic_commit(store));
}

void version_created_and_invalidated_within_read_transaction() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    // If committed, this write would invalidate t1
    ASSERT(t2.write(store, 0, 2).has_value());
    ASSERT_EQ(std::optional<Value>{2}, t2.optimistic_read(store, 0));
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    t2.abort(store);
    ASSERT_EQ(std::optional<Value>{1}, t1.optimistic_read(store, 0));
    // Because t2 aborted, t1 should commit successfully
    ASSERT(t1.optimistic_commit(store).has_value());
}

void non_conflicting_writes_both_commit() {
    const Store store({1, 2, 3, 4, 5});
    auto manager = TransactionManager::create();
    auto t1 = manager->begin();
    auto t2 = manager->begin();
    ASSERT(t1.write(store, 0, 2).has_value());
    ASSERT(t2.write(store, 1, 3).has_value());
    ASSERT(t1.optimistic_commit(store).has_value());
    ASSERT(t2.optimistic_commit(store).has_value());
    auto t3 = manager->begin();
    ASSERT_EQ(std::optional<Value>(2), t3.optimistic_read(store, 0));
    ASSERT_EQ(std::optional<Value>(3), t3.optimistic_read(store, 1));
}

// Spawn N_THREADS threads. Each one reads key i 100 times then commits.
void parallel_reads_partitioned_keys() {
    const std::uint16_t N_THREADS = 256;
    std::vector<Value> initial_values(N_THREADS);
    std::iota(initial_values.begin(), initial_values.end(), 1);
    const Store store(initial_values);
    auto manager = TransactionManager::create();
    std::vector<std::thread> threads(N_THREADS);
    for (std::uint16_t i = 0; i < N_THREADS; i++) {
        threads[i] = std::thread([&manager, &store, i]() {
            Key key = i;
            auto t = manager->begin();
            for (unsigned char j = 0; j < 100; j++) {
                ASSERT_EQ(std::optional<Value>(i + 1), t.optimistic_read(store, key));
            }
            ASSERT(t.optimistic_commit(store).has_value());
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
}

// Spawn N_THREADS threads. Each one opens a transaction, writes key i, and commits 100 times.
void parallel_writes_partitioned_keys() {
    const std::uint16_t N_THREADS = 256;
    std::vector<Value> initial_values(N_THREADS);
    std::iota(initial_values.begin(), initial_values.end(), 1);
    const Store store(initial_values);
    auto manager = TransactionManager::create();
    std::vector<std::thread> threads(N_THREADS);
    for (std::uint16_t i = 0; i < N_THREADS; i++) {
        threads[i] = std::thread([&manager, &store, i]() {
            Key key = i;
            for (unsigned char j = 0; j < 100; j++) {
                auto t = manager->begin();
                ASSERT(t.write(store, key, j).has_value());
                ASSERT(t.optimistic_commit(store).has_value());
            }
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    // Ensure all values are 99
    auto t = manager->begin();
    for (std::uint16_t i = 0; i < N_THREADS; i++) {
        ASSERT_EQ(std::optional<Value>(99), t.optimistic_read(store, i));
    }
    ASSERT(t.optimistic_commit(store).has_value());
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        if (std::string_view(argv[1]) == "simple_read_existing") {
            simple_read_existing();
            return 0;
        }
        if (std::string_view(argv[1]) == "simple_read_missing") {
            simple_read_missing();
            return 0;
        }
        if (std::string_view(argv[1]) == "read_ignores_earlier_uncommitted_write") {
            read_ignores_earlier_uncommitted_write();
            return 0;
        }
        if (std::string_view(argv[1]) == "read_ignores_later_uncommitted_write") {
            read_ignores_later_uncommitted_write();
            return 0;
        }
        if (std::string_view(argv[1]) == "simple_scan") {
            simple_scan();
            return 0;
        }
        if (std::string_view(argv[1]) == "read_after_commit") {
            read_after_commit();
            return 0;
        }
        if (std::string_view(argv[1]) == "read_before_and_after_commit") {
            read_before_and_after_commit();
            return 0;
        }
        if (std::string_view(argv[1]) == "conflicting_writes_abort_second_writer") {
            conflicting_writes_abort_second_writer();
            return 0;
        }
        if (std::string_view(argv[1]) == "aborted_write_is_ignored") {
            aborted_write_is_ignored();
            return 0;
        }
        if (std::string_view(argv[1]) == "write_after_aborted_write_succeeds") {
            write_after_aborted_write_succeeds();
            return 0;
        }
        if (std::string_view(argv[1]) == "non_repeatable_read_is_aborted") {
            non_repeatable_read_is_aborted();
            return 0;
        }
        if (std::string_view(argv[1]) ==
            "version_created_and_invalidated_within_read_transaction") {
            version_created_and_invalidated_within_read_transaction();
            return 0;
        }
        if (std::string_view(argv[1]) == "non_conflicting_writes_both_commit") {
            non_conflicting_writes_both_commit();
            return 0;
        }
        if (std::string_view(argv[1]) == "parallel_reads_partitioned_keys") {
            parallel_reads_partitioned_keys();
            return 0;
        }
        if (std::string_view(argv[1]) == "parallel_writes_partitioned_keys") {
            parallel_writes_partitioned_keys();
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 2;
}
