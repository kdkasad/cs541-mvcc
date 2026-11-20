#pragma once

#include "workload.hpp"

#include <span>

namespace cs541_mvcc::bench {
enum class Format { Table, Csv };

void print_report(std::span<const Run> runs, Format format);
}
