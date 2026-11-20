#include "report.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace cs541_mvcc::bench {
namespace {
using Row = std::vector<std::string>;

const Row HEADER = {"name",          "threads",         "keys",           "reads",
                    "writes",        "commits",         "aborts",         "abort_rate_pct",
                    "commits_per_s", "aborts_per_s",    "write_conflict", "validation",
                    "read_miss",     "mean_us",         "p50_us",         "p99_us"};

std::string fixed(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

// Latency at percentile p (0-100) in microseconds; `sorted_ns` must be sorted.
double percentile_us(const std::vector<std::uint64_t>& sorted_ns, double p) {
    if (sorted_ns.empty()) {
        return 0;
    }
    const auto index = static_cast<std::size_t>(p / 100.0 * static_cast<double>(sorted_ns.size() - 1));
    return static_cast<double>(sorted_ns[index]) / 1000.0;
}

Row make_row(const Run& run) {
    const auto& [config, result] = run;
    auto latencies = result.commit_latencies_ns;
    std::ranges::sort(latencies);
    const auto attempts = result.commits + result.aborts();
    const double abort_rate =
        attempts == 0 ? 0 : 100.0 * static_cast<double>(result.aborts()) / static_cast<double>(attempts);
    const double mean_us =
        latencies.empty() ? 0
                          : static_cast<double>(std::accumulate(latencies.begin(), latencies.end(),
                                                                std::uint64_t{0})) /
                                static_cast<double>(latencies.size()) / 1000.0;
    const auto per_second = [&](std::uint64_t count) {
        return static_cast<double>(count) / result.elapsed_seconds;
    };
    return {config.name,
            std::to_string(config.threads),
            std::to_string(config.num_keys),
            std::to_string(config.reads_per_txn),
            std::to_string(config.writes_per_txn),
            std::to_string(result.commits),
            std::to_string(result.aborts()),
            fixed(abort_rate, 2),
            fixed(per_second(result.commits), 0),
            fixed(per_second(result.aborts()), 0),
            std::to_string(result.write_conflict_aborts),
            std::to_string(result.validation_aborts),
            std::to_string(result.read_miss_aborts),
            fixed(mean_us, 2),
            fixed(percentile_us(latencies, 50), 2),
            fixed(percentile_us(latencies, 99), 2)};
}

void print_csv(const std::vector<Row>& rows) {
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            std::cout << (i == 0 ? "" : ",") << row[i];
        }
        std::cout << '\n';
    }
}

void print_table(const std::vector<Row>& rows) {
    std::vector<std::size_t> widths(HEADER.size(), 0);
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            // Left-align the name column, right-align the numbers.
            std::cout << (i == 0 ? std::left : std::right) << std::setw(static_cast<int>(widths[i]))
                      << row[i] << "  ";
        }
        std::cout << '\n';
    }
}
}

void print_report(std::span<const Run> runs, Format format) {
    std::vector<Row> rows = {HEADER};
    for (const auto& run : runs) {
        rows.push_back(make_row(run));
    }
    if (format == Format::Csv) {
        print_csv(rows);
    } else {
        print_table(rows);
    }
}
}
