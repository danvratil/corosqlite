// SPDX-FileCopyrightText: 2021 Daniel Vrátil <me@dvratil.cz>
//
// SPDX-License-Identifier: MIT

#if defined(__clang__)
#include <experimental/coroutine>
namespace corosqlite::coro {
    using suspend_never = std::experimental::suspend_never;
    using suspend_always = std::experimental::suspend_always;
    template<typename T>
    using coroutine_handle = std::experimental::coroutine_handle<T>;
} // namespace corosqlite::coro
#elif defined(__GNUC__) || defined(_MSC_VER)
#include <coroutine>
namespace corosqlite::coro {
    using suspend_never = std::suspend_never;
    using suspend_always = std::suspend_always;
    template<typename T>
    using coroutine_handle = std::coroutine_handle<T>;
} // namespace corosqlite::coro
#else
#pragma error "Current compiler is not supported (yet)."
#endif
#include <cassert>
#include <utility>
#include <stdexcept>

namespace corosqlite {

template<typename T>
class ResultGenerator {
public:
    struct promise_type {
        T result_;

        using coro_handle = coro::coroutine_handle<promise_type>;
        auto get_return_object() {
            return coro_handle::from_promise(*this);
        }

        constexpr coro::suspend_never initial_suspend() noexcept { return {}; };
        constexpr coro::suspend_always final_suspend() noexcept { return {}; };
        constexpr void return_void() noexcept {};

        auto yield_value(T &&result) {
            result_ = std::move(result);
            return coro::suspend_always();
        }

        auto unhandled_exception() {
            std::terminate();
        }
    };

    using coro_handle = coro::coroutine_handle<promise_type>;

    ResultGenerator(coro_handle handle)
        : handle_(handle) {};

    ResultGenerator(ResultGenerator &) = delete;
    ResultGenerator(ResultGenerator &&) = delete;
    ~ResultGenerator() {
        handle_.destroy();
    }

    auto done() -> bool {
        if (!handle_.done()) {
            handle_.resume();
        }
        return handle_.done();
    }

    auto next() const -> T {
        return handle_.promise().result_;
    }
private:
    coro_handle handle_;
};

} // namespace corosqlite

