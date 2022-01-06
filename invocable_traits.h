#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

// derived and extended from https://github.com/kennytm/utils/blob/master/traits.hpp
// and https://stackoverflow.com/a/28213747
// This does not handle overloaded functions (which includes functors with
// overloaded operator()), because the compiler would not be able to resolve
// the overload without knowing the argument types and the cv- and noexcept-
// qualifications. If you do know those already and can thus specify the
// overload to the compiler, you do not need this class. The only remaining
// piece of information is the result type, which you can get with
// std::invoke_result.

namespace detail
{
    template <std::size_t i, typename... Args>
    struct invocable_traits_arg_impl
    {
        static_assert(i < sizeof...(Args), "Requested argument type out of bounds (function does not declare this many arguments)");

        using type = std::tuple_element_t<i, std::tuple<Args...>>;
    };

    template <typename R, typename C, bool IsVariadic, typename... Args>
    struct invocable_traits_class
    {
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr auto is_variadic = IsVariadic;

        using result_type = R;
        using class_type = C;

        template <std::size_t i>
        using arg = typename invocable_traits_arg_impl<i, Args...>::type;
    };

    template <typename R, bool IsVariadic, typename... Args>
    struct invocable_traits_free : public invocable_traits_class<R, void, IsVariadic, Args...> {};
}

template <typename T>
struct invocable_traits;

#define INVOCABLE_TRAITS_SPEC(cv,...)                                               \
/* handle functions, with all possible iterations of reference and noexcept */      \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args... __VA_OPT__(,) __VA_ARGS__) cv>                    \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args...__VA_OPT__(,) __VA_ARGS__) cv &>                   \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args...__VA_OPT__(,) __VA_ARGS__) cv &&>                  \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args...__VA_OPT__(,) __VA_ARGS__) cv noexcept>            \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args...__VA_OPT__(,) __VA_ARGS__) cv & noexcept>          \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
template <typename R, typename... Args>                                             \
struct invocable_traits<R(Args...__VA_OPT__(,) __VA_ARGS__) cv && noexcept>         \
    : public detail::invocable_traits_free<R,0 __VA_OPT__(+1),Args...> {};          \
/* handle pointers to member functions (incl. iterations of reference and noexcept) */\
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv>               \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv &>             \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv &&>            \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv noexcept>      \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv & noexcept>    \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
template <typename C, typename R, typename... Args>                                 \
struct invocable_traits<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) cv && noexcept>   \
    : public detail::invocable_traits_class<R,C,0 __VA_OPT__(+1),Args...> {};       \
/* handle pointers to data members */                                               \
__VA_OPT__( /* no variadic function version for data members, (inverted) skip */    \
template <typename C, typename R>                                                   \
struct invocable_traits<R C::* cv>                                                  \
    : public detail::invocable_traits_class<R,C,false> {};                          \
) /* end __VA_OPT___*/

// cover all const and volatile permutations
INVOCABLE_TRAITS_SPEC(, )
INVOCABLE_TRAITS_SPEC(const, )
INVOCABLE_TRAITS_SPEC(volatile, )
INVOCABLE_TRAITS_SPEC(const volatile, )
// and also variadic function versions
INVOCABLE_TRAITS_SPEC(, ...)
INVOCABLE_TRAITS_SPEC(const, ...)
INVOCABLE_TRAITS_SPEC(volatile, ...)
INVOCABLE_TRAITS_SPEC(const volatile, ...)

// pointers to functions
template <typename R, typename... Args>
struct invocable_traits<R(*)(Args...)>                  : public invocable_traits<R(Args...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(*)(Args...) noexcept>         : public invocable_traits<R(Args...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(*)(Args..., ...)>             : public invocable_traits<R(Args..., ...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(*)(Args..., ...) noexcept>    : public invocable_traits<R(Args..., ...)> {};
// references to functions
template <typename R, typename... Args>
struct invocable_traits<R(&)(Args...)>                  : public invocable_traits<R(Args...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(&)(Args...) noexcept>         : public invocable_traits<R(Args...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(&)(Args..., ...)>             : public invocable_traits<R(Args..., ...)> {};
template <typename R, typename... Args>
struct invocable_traits<R(&)(Args..., ...) noexcept>    : public invocable_traits<R(Args..., ...)> {};

// get at operator() of any struct/class defining it (this includes lambdas)
// bit of machinery for better error messages
namespace detail {
    template <typename T>
    concept HasCallOperator = requires(T)
    {
        std::declval<T>().operator();
    };

    template <typename T, bool isCallable>
    struct invocable_traits_extract : invocable_traits<decltype(&T::operator())> {};

    template <typename T>
    struct invocable_traits_extract<T, false>
    {
        static_assert(std::is_class_v<T>, "passed type is not a class, and thus cannot have an operator()");
        static_assert(!std::is_class_v<T> || HasCallOperator<T>, "passed type is a class that doesn't have an operator()");

        // to reduce excessive compiler error output
        static constexpr std::size_t arity = 0;
        static constexpr auto is_variadic = false;
        using result_type = void;
        using class_type = void;
        template <size_t i> struct arg { using type = void; };
    };
}

template <typename T>
struct invocable_traits : detail::invocable_traits_extract<std::decay_t<T>, detail::HasCallOperator<T>> {};
