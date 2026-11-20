#include "workload.hpp"

#include "cs541_mvcc/lib.hpp"

#include <chrono>
#include <latch>
#include <numeric>
#include <random>
#include <thread>
#include <utility>

namespace cs541_mvcc::bench {
namespace {
enum class Outcome { Committed, WriteConflict, ValidationFailed, ReadMiss };

Outcome run_one_txn(const Store& store, TransactionManager& manager, const WorkloadConfig& config,
                    std::mt19937_64& rng, std::uniform_int_distribution<Key>& key_dist) {
    auto t = manager.begin();
    Value sum = 0;
    for (std::size_t i = 0; i < config.reads_per_txn; ++i) {
        const auto value = t.optimistic_read(store, key_dist(rng));
        if (!value) {
            t.abort(store);
            return Outcome::ReadMiss;
        }
        sum += *value;
    }
    for (std::size_t i = 0; i < config.writes_per_txn; ++i) {
        const auto written = t.write(store, key_dist(rng), sum);
        // The library doesn't abort on a failed write, so we do it here.
        if (!written && written.error() == WriteError::Conflict) {
            t.abort(store);
            return Outcome::WriteConflict;
        }
        // AlreadyWritten (same random key twice) just skips the write.
    }
    if (!t.optimistic_commit(store)) {
        return Outcome::ValidationFailed;
    }
    return Outcome::Committed;
}

void run_thread(const Store& store, TransactionManager& manager, const WorkloadConfig& config,
                std::size_t thread_index, std::latch& start, WorkloadResult& result) {
    std::mt19937_64 rng(thread_index);
    std::uniform_int_distribution<Key> key_dist(0, config.num_keys - 1);
    start.wait();
    for (std::size_t i = 0; i < config.txns_per_thread; ++i) {
        const auto txn_start = std::chrono::steady_clock::now();
        switch (run_one_txn(store, manager, config, rng, key_dist)) {
        case Outcome::Committed: {
            const auto elapsed = std::chrono::steady_clock::now() - txn_start;
            ++result.commits;
            result.commit_latencies_ns.push_back(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()));
            break;
        }
        case Outcome::WriteConflict:
            ++result.write_conflict_aborts;
            break;
        case Outcome::ValidationFailed:
            ++result.validation_aborts;
            break;
        case Outcome::ReadMiss:
            ++result.read_miss_aborts;
            break;
        }
    }
}

// Unlink chains one version at a time; default shared_ptr destruction recurses and can overflow.
void free_version_chains(const Store& store) {
    for (const auto key : store.key_range()) {
        auto version = store.get_latest_version(key);
        while (version) {
            // Separate statement so the lock is released before `version` may be freed.
            auto previous = std::exchange(*version->previous.lock(), nullptr);
            version = std::move(previous);
        }
    }
}
}

WorkloadResult run_workload(const WorkloadConfig& config) {
    std::vector<Value> initial_values(config.num_keys);
    std::iota(initial_values.begin(), initial_values.end(), 0);
    const Store store(std::move(initial_values));
    auto manager = TransactionManager::create();

    std::vector<WorkloadResult> per_thread(config.threads);
    std::latch start(1);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < config.threads; ++i) {
        threads.emplace_back(run_thread, std::cref(store), std::ref(*manager), std::cref(config), i,
                             std::ref(start), std::ref(per_thread[i]));
    }
    const auto wall_start = std::chrono::steady_clock::now();
    start.count_down();
    for (auto& thread : threads) {
        thread.join();
    }
    const auto wall_elapsed = std::chrono::steady_clock::now() - wall_start;

    WorkloadResult total;
    total.elapsed_seconds = std::chrono::duration<double>(wall_elapsed).count();
    for (auto& result : per_thread) {
        total.commits += result.commits;
        total.write_conflict_aborts += result.write_conflict_aborts;
        total.validation_aborts += result.validation_aborts;
        total.read_miss_aborts += result.read_miss_aborts;
        total.commit_latencies_ns.insert(total.commit_latencies_ns.end(),
                                         result.commit_latencies_ns.begin(),
                                         result.commit_latencies_ns.end());
    }
    free_version_chains(store);
    return total;
}
}
