#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

// inspired by https://github.com/kennytm/utils/blob/master/traits.hpp
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

    template <
        typename Rd, typename Ri, typename C,
        bool IsConst, bool isVolatile, bool isNoexcept, bool IsVariadic,
        typename... Args>
    struct invocable_traits_class
    {
        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr auto is_const    = IsConst;
        static constexpr auto is_volatile = isVolatile;
        static constexpr auto is_noexcept = isNoexcept;
        static constexpr auto is_variadic = IsVariadic;

        using declared_result_t = Rd;   // return type as declared in function
        using invoke_result_t   = Ri;   // return type of std::invoke() expression
        using class_t           = C;

        template <std::size_t i>
        using arg_t = typename invocable_traits_arg_impl<i, Args...>::type;
    };

    template <
        typename Rd, typename Ri,
        bool IsConst, bool isVolatile, bool isNoexcept, bool IsVariadic,
        typename... Args>
    struct invocable_traits_free : public invocable_traits_class<Rd, Ri, void, IsConst, isVolatile, isNoexcept, IsVariadic, Args...> {};


    template <typename T>
    struct invocable_traits_impl;

    #define IS_NONEMPTY(...) 0 __VA_OPT__(+1)

    #define INVOCABLE_TRAITS_SPEC(c,v,...)                                              \
    /* functions, including noexcept versions */                                        \
    template <typename R, typename... Args>                                             \
    struct invocable_traits_impl<R(Args... __VA_OPT__(,) __VA_ARGS__) c v>              \
        : public invocable_traits_free<                                                 \
            R,                                                                          \
            std::invoke_result_t<R(Args... __VA_OPT__(,) __VA_ARGS__) c v,Args...>,     \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            false,                                                                      \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};                                                                \
    template <typename R, typename... Args>                                             \
    struct invocable_traits_impl<R(Args...__VA_OPT__(,) __VA_ARGS__) c v noexcept>      \
        : public invocable_traits_free<                                                 \
            R,                                                                          \
            std::invoke_result_t<R(Args... __VA_OPT__(,) __VA_ARGS__) c v noexcept,Args...>,\
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            true,                                                                       \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};                                                                \
    /* pointers to member functions, including noexcept versions) */                    \
    template <typename C, typename R, typename... Args>                                 \
    struct invocable_traits_impl<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v>         \
        : public invocable_traits_class<                                                \
            R,                                                                          \
            std::invoke_result_t<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v,C,Args...>,  \
            C,                                                                          \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            false,                                                                      \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};                                                                \
    template <typename C, typename R, typename... Args>                                 \
    struct invocable_traits_impl<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v noexcept>\
        : public invocable_traits_class<                                                \
            R,                                                                          \
            std::invoke_result_t<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v noexcept,C,Args...>,  \
            C,                                                                          \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            true,                                                                       \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};

    // cover all const and volatile permutations
    INVOCABLE_TRAITS_SPEC(,, )
    INVOCABLE_TRAITS_SPEC(const,, )
    INVOCABLE_TRAITS_SPEC(,volatile, )
    INVOCABLE_TRAITS_SPEC(const, volatile, )
    // and also variadic function versions
    INVOCABLE_TRAITS_SPEC(,, ...)
    INVOCABLE_TRAITS_SPEC(const,, ...)
    INVOCABLE_TRAITS_SPEC(,volatile, ...)
    INVOCABLE_TRAITS_SPEC(const, volatile, ...)
    #undef INVOCABLE_TRAITS_SPEC

    /* pointers to data members */
    template <typename C, typename R>
    struct invocable_traits_impl<R C::*>
        : public invocable_traits_class<R,
                                        std::invoke_result_t<R C::*,C>,
                                        C,
                                        false,
                                        false,
                                        false,
                                        false> {};

    // pointers to functions
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...)>                 : public invocable_traits_impl<R(Args...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...) noexcept>        : public invocable_traits_impl<R(Args...) noexcept> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...)>            : public invocable_traits_impl<R(Args..., ...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...) noexcept>   : public invocable_traits_impl<R(Args..., ...) noexcept> {};


    // get at operator() of any struct/class defining it (this includes lambdas)
    // bit of machinery for better error messages
    template <typename T>
    concept HasCallOperator = requires(T t)
    {
        t.operator();
    };

    template <typename T, bool isCallable>
    struct invocable_traits_extract : invocable_traits_impl<decltype(&T::operator())> {};

    template <typename T>
    struct invocable_traits_extract<T, false>
    {
        static_assert(std::is_class_v<T>, "passed type is not a class, and thus cannot have an operator()");
        static_assert(!std::is_class_v<T> || HasCallOperator<T>, "passed type is a class that doesn't have an operator()");

        // to reduce excessive compiler error output
        static constexpr std::size_t arity = 0;
        static constexpr auto is_const    = false;
        static constexpr auto is_volatile = false;
        static constexpr auto is_noexcept = false;
        static constexpr auto is_variadic = false;
        using declared_result_t = void;
        using invoke_result_t   = void;
        using class_t           = void;
        template <size_t i> struct arg_t { using type = void; };
    };

    template <typename T>
    struct invocable_traits_impl : invocable_traits_extract<T, HasCallOperator<T>> {};
}

template <typename T>
struct invocable_traits : detail::invocable_traits_impl<std::decay_t<T>> {};
