#include "cs541_mvcc/lib.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace cs541_mvcc;

static void __do_assert(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + std::string(expression) + " at line " +
                                 std::to_string(line));
    }
}

#define ASSERT(expression) __do_assert((expression), #expression, __LINE__)
#define ASSERT_EQ(left, right) __do_assert(((left) == (right)), #left " == " #right, __LINE__)

static void print_boundary(const TxnIdOrTimestamp& boundary) {
    if (std::holds_alternative<Infinity>(boundary)) {
        std::cout << "Infinity";
    } else if (const auto* timestamp = std::get_if<Timestamp>(&boundary)) {
        std::cout << "Timestamp(Timestamp(" << timestamp->value << "))";
    } else {
        std::cout << "TxnId(TxnId(" << std::get<TxnId>(boundary).value << "))";
    }
}

[[maybe_unused]] static void print_version_chain(std::shared_ptr<Version> head) {
    auto current = std::move(head);
    std::cout << "Start version chain\n";
    while (current) {
        auto version = current;
        std::cout << "begin = ";
        print_boundary(*version->begin.lock());
        std::cout << ", end = ";
        print_boundary(*version->end.lock());
        std::cout << ", key = " << version->key << ", value = " << version->value << '\n';
        current = *version->previous.lock();
    }
    std::cout << "End version chain" << std::endl;
}
