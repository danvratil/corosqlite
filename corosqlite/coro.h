#include <coroutine>
#include <cassert>
#include <utility>
#include <stdexcept>

namespace corosqlite {

template<typename T>
class ResultGenerator {
public:
    struct promise_type {
        T result_;

        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() {
            return coro_handle::from_promise(*this);
        }

        constexpr std::suspend_never initial_suspend() { return {}; };
        constexpr std::suspend_always final_suspend() { return {}; };
        constexpr void return_void() {};

        auto yield_value(T &&result) {
            result_ = std::move(result);
            return std::suspend_always();
        }

        auto unhandled_exception() {
            std::terminate();
        }
    };

    using coro_handle = std::coroutine_handle<promise_type>;

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

