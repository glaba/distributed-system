#pragma once

#include <mutex>
#include <utility>
#include <iostream>
#include <memory>

class locked_base {
private:
    // Helper function that allows us to support dyn_cast as well as replacing the original pointer
    virtual void replace_ptr(void *ptr) = 0;

    template <typename U>
    friend class unlocked;
};

template <typename T>
class locked;

template <typename T>
class unlocked {
public:
    struct empty {};

    unlocked() = delete;
    unlocked(empty e) {}
    unlocked(unlocked const&) = delete;
    unlocked(unlocked &&value) {
        std::swap(m, value.m);
        std::swap(data, value.data);
        std::swap(base, value.base);
    }
    ~unlocked() {
        if (m) {
            m->unlock();
        }
    }

    template <typename U>
    static auto dyn_cast(unlocked<U> &&value) -> unlocked<T> {
        unlocked<T> retval(value.m, dynamic_cast<T*>(value.data), value.base);
        value.m = nullptr;
        value.data = nullptr;
        value.base = nullptr;
        return retval;
    }

    auto operator=(unlocked const&) -> unlocked& = delete;
    inline auto operator=(unlocked &&other) -> unlocked& {
        if (m) {
            m->unlock();
        }
        m = other.m;
        data = other.data;
        base = other.base;
        other.m = nullptr;
        other.data = nullptr;
        other.base = nullptr;
        return *this;
    }

    explicit operator bool() const {
        return data != nullptr;
    }

    inline auto operator*() const -> T& {
        return *data;
    }

    inline auto operator->() const -> T* {
        return data;
    }

    void replace_data(std::unique_ptr<T> &&ptr) {
        data = ptr.release();
        base->replace_ptr(reinterpret_cast<void*>(data));
    }

    auto unsafe_get_mutex() -> std::recursive_mutex& {
        return *m;
    }

    auto unsafe_get_data() -> T& {
        return *data;
    }

private:
    unlocked(locked<T> &ref) : m(&ref.m), data(ref.data.get()), base(&ref) {
        m->lock();
    }

    unlocked(std::recursive_mutex *m_, T *data_, locked_base *base_) : m(m_), data(data_), base(base_) {}

    std::recursive_mutex *m = nullptr;
    T *data = nullptr;
    locked_base *base = nullptr;

    template <typename U>
    friend class locked;
    template <typename U>
    friend class unlocked;
};

template <typename T>
class locked : public locked_base {
public:
    struct empty {};

    locked(empty e) : data(nullptr) {}

    locked() : data(std::make_unique<T>()) {}

    template <typename ...U>
    locked(U&&... args) : data(std::make_unique<T>(std::forward<U>(args)...)) {}

    locked(std::unique_ptr<T> &&data_) : data(std::move(data_)) {}

    // Returns an object of type unlocked<T>, whose lifetime cannot exceed that of this locked<T>
    inline auto operator()() const -> unlocked<T> {
        return unlocked(*const_cast<locked<T>*>(this));
    }

    auto unsafe_get_mutex() -> std::recursive_mutex& {
        return m;
    }

    auto unsafe_get_data() -> T& {
        return *data;
    }

private:
    void replace_ptr(void *ptr) {
        data = std::unique_ptr<T>(reinterpret_cast<T*>(ptr));
    }

    std::recursive_mutex m;
    std::unique_ptr<T> data;

    template <typename U>
    friend class unlocked;
};
