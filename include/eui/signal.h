#pragma once

#include <utility>

namespace eui {

template <typename T>
class Signal {
public:
    Signal() = default;
    explicit Signal(T value) : value_(std::move(value)) {}

    const T& get() const { return value_; }
    T& get() { return value_; }

    void set(const T& value) { value_ = value; }
    void set(T&& value) { value_ = std::move(value); }

    const T& value() const { return value_; }
    T& value() { return value_; }

    Signal& operator=(const T& value) {
        set(value);
        return *this;
    }

    Signal& operator=(T&& value) {
        set(std::move(value));
        return *this;
    }

private:
    T value_{};
};

template <typename T>
Signal<T> signal(T value) {
    return Signal<T>(std::move(value));
}

} // namespace eui
