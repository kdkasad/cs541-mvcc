#include "cs541_mvcc/lib.hpp"

#include <iostream>

int main() {
    // This is just a dummy entrypoint.
    // The actual tests/demos are in tests/simple.cpp.
    const cs541_mvcc::Store store({1, 2, 3, 4, 5});
    auto manager = cs541_mvcc::TransactionManager::create();
    auto t1 = manager->begin();
    const auto value = t1.optimistic_read(store, 0);
    if (value) {
        std::cerr << "t1.optimistic_read(store, 0) = Some(" << *value << ")\n";
    } else {
        std::cerr << "t1.optimistic_read(store, 0) = None\n";
    }
}
