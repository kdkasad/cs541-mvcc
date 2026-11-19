#pragma once

#include <compare>
#include <cstdint>

namespace cs541_mvcc {
struct Timestamp {
    std::uint64_t value;
    auto operator<=>(const Timestamp&) const = default;
};
}
