#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs541_mvcc::bench {
struct WorkloadConfig {
    std::string name;
    std::size_t threads;
    /// Fewer keys means more contention.
    std::size_t num_keys;
    std::size_t reads_per_txn;
    std::size_t writes_per_txn;
    /// Fixed count instead of a duration so memory stays bounded (no GC).
    std::size_t txns_per_thread;
};

struct WorkloadResult {
    std::uint64_t commits = 0;
    /// write() returned WriteError::Conflict.
    std::uint64_t write_conflict_aborts = 0;
    /// optimistic_commit() returned CommitError::NotSerializable.
    std::uint64_t validation_aborts = 0;
    /// A read found no visible version (writer was preparing).
    std::uint64_t read_miss_aborts = 0;
    double elapsed_seconds = 0;
    std::vector<std::uint64_t> commit_latencies_ns;

    [[nodiscard]] std::uint64_t aborts() const {
        return write_conflict_aborts + validation_aborts + read_miss_aborts;
    }
};

struct Run {
    WorkloadConfig config;
    WorkloadResult result;
};

WorkloadResult run_workload(const WorkloadConfig& config);
}
