#include "report.hpp"
#include "workload.hpp"

#include <iostream>
#include <string_view>
#include <vector>

using namespace cs541_mvcc::bench;

namespace {
constexpr std::size_t TXNS_PER_THREAD = 100000;

std::vector<WorkloadConfig> make_sweep() {
    std::vector<WorkloadConfig> configs;
    // Thread scaling with low contention, read-only then mixed (both 6 ops per txn).
    for (const std::size_t threads : {1, 2, 4, 8}) {
        configs.push_back({"read_only", threads, 10000, 6, 0, TXNS_PER_THREAD});
    }
    for (const std::size_t threads : {1, 2, 4, 8}) {
        configs.push_back({"mixed", threads, 10000, 4, 2, TXNS_PER_THREAD});
    }
    // Contention: same threads, fewer keys.
    for (const std::size_t keys : {10000, 1000, 100, 10}) {
        configs.push_back({"contention", 8, keys, 4, 2, TXNS_PER_THREAD});
    }
    return configs;
}
}

int main(int argc, char** argv) {
    auto format = Format::Table;
    if (argc == 2 && std::string_view(argv[1]) == "--csv") {
        format = Format::Csv;
    } else if (argc != 1) {
        std::cerr << "usage: " << argv[0] << " [--csv]\n";
        return 2;
    }
    std::vector<Run> runs;
    for (const auto& config : make_sweep()) {
        runs.push_back({config, run_workload(config)});
    }
    print_report(runs, format);
}
