#pragma once
#if defined(__clang__) || defined(_MSC_VER)
#include <experimental/coroutine>
namespace std
{
    template <typename T = void>
    using coroutine_handle = std::experimental::coroutine_handle<T>;
}
#elif defined(__GNUC__)
#include <coroutine>
#endif
using default_handle = std::coroutine_handle<>;