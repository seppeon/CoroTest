#include <exception>
#include <utility>
#include <cassert>
#include <iostream>

#if defined(__clang__) || defined(_MSC_VER)
#include <experimental/coroutine>
namespace std
{
    template <typename T = void>
    using coroutine_handle = std::experimental::coroutine_handle<T>;
    using std::experimental::suspend_always;
}
#elif defined(__GNUC__)
#include <coroutine>
#endif


struct task {
    struct promise_type
    {
        template <typename ... Args>
        promise_type(Args &...)
        {
            std::cout << "arg count = " << sizeof...(Args) << "\n";
        }

        task get_return_object() noexcept
        {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        
        struct yield_awaiter {
            bool await_ready() noexcept { return false; }
            auto await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                return std::exchange(h.promise().continuation, {});
            }
            void await_resume() noexcept {
            }
        };

        yield_awaiter yield_value(int x) noexcept {
            value = x;
            return {};
        }

        void return_value(int x) noexcept {
            value = x;
        }

        yield_awaiter final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() noexcept {
            std::terminate();
        }

        int value;
        std::coroutine_handle<> continuation;
    };

    explicit task(std::coroutine_handle<promise_type> coro) noexcept
    : coro(coro)
    {}

    task(task&& t) noexcept
    : coro(std::exchange(t.coro, {}))
    {}

    ~task() {
        if (coro) coro.destroy();
    }

    bool await_ready() {
        return false;
    }

    auto await_suspend(std::coroutine_handle<> h) noexcept {
        coro.promise().continuation = h;
        return coro;
    }

    int await_resume() {
        return coro.promise().value;
    }

    std::coroutine_handle<promise_type> coro;
};

task args_passing(int a, int b) {
    co_yield a;
    co_return b;
}

int main() {
    task t = args_passing(1, 2);
    return 0;
}
