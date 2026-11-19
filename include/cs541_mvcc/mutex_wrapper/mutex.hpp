#pragma once

#include <mutex>
#include <utility>

namespace cs541_mvcc::mutex_wrapper {

// RAII mutex. Wraps the protected data.
// Call m.lock() to lock the mutex and get access to the contained data.
// Unlocks when the Guard returned from m.lock() is destructed.
template <class T> class Mutex {
    mutable std::mutex mutex_;
    mutable T value_;

public:
    Mutex() : value_{} {}
    explicit Mutex(T value) : value_(std::move(value)) {}
    class Guard {
        std::unique_lock<std::mutex> lock_;
        T& value_;

    public:
        Guard(std::mutex& mutex, T& value) : lock_(mutex), value_(value) {}
        T& operator*() const { return value_; }
        T* operator->() const { return &value_; }
    };
    Guard lock() const { return Guard(mutex_, value_); }
};
}
