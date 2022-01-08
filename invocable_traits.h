#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

// inspired by https://github.com/kennytm/utils/blob/master/traits.hpp
// and https://stackoverflow.com/a/28213747. Further thanks to G. Sliepen
// on StackExchange CodeReview for numerous helpful suggestions.
// 
// Note that this machinery does not handle overloaded or templated functions.
// It could not possibly do so, since these do not have a unique address.
// If passed references or pointers to specific overloads or template 
// instantiations, all works as expected.

namespace detail
{
    template <bool, std::size_t i, typename... Args>
    struct invocable_traits_arg_impl
    {
        using type = std::tuple_element_t<i, std::tuple<Args...>>;
    };
    template <std::size_t i, typename... Args>
    struct invocable_traits_arg_impl<false, i, Args...>
    {
        static_assert(i < sizeof...(Args), "Argument index out of bounds (queried callable does not declare this many arguments)");

        // to reduce excessive compiler error output
        using type = void;
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
        using arg_t = typename invocable_traits_arg_impl<i < sizeof...(Args), i, Args...>::type;
    };

    template <
        typename Rd, typename Ri,
        bool IsConst, bool isVolatile, bool isNoexcept, bool IsVariadic,
        typename... Args>
    struct invocable_traits_free : public invocable_traits_class<Rd, Ri, void, IsConst, isVolatile, isNoexcept, IsVariadic, Args...> {};


    template <typename T>
    struct invocable_traits_impl;

    #define IS_NONEMPTY(...) 0 __VA_OPT__(+1)

    #define INVOCABLE_TRAITS_SPEC(c,v,e,...)                                            \
    /* functions, including noexcept versions */                                        \
    template <typename R, typename... Args>                                             \
    struct invocable_traits_impl<R(Args... __VA_OPT__(,) __VA_ARGS__) c v e>            \
        : public invocable_traits_free<                                                 \
            R,                                                                          \
            std::invoke_result_t<R(Args... __VA_OPT__(,) __VA_ARGS__) c v e,Args...>,   \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            IS_NONEMPTY(e),                                                             \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};                                                                \
    /* pointers to member functions, including noexcept versions) */                    \
    template <typename C, typename R, typename... Args>                                 \
    struct invocable_traits_impl<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v e>       \
        : public invocable_traits_class<                                                \
            R,                                                                          \
            std::invoke_result_t<R(C::*)(Args...__VA_OPT__(,) __VA_ARGS__) c v e,C,Args...>,  \
            C,                                                                          \
            IS_NONEMPTY(c),                                                             \
            IS_NONEMPTY(v),                                                             \
            IS_NONEMPTY(e),                                                             \
            IS_NONEMPTY(__VA_ARGS__),                                                   \
            Args...> {};

    // cover all const, volatile and noexcept permutations
    INVOCABLE_TRAITS_SPEC(,,, )
    INVOCABLE_TRAITS_SPEC(const,,, )
    INVOCABLE_TRAITS_SPEC(, volatile,, )
    INVOCABLE_TRAITS_SPEC(const, volatile,, )
    INVOCABLE_TRAITS_SPEC(,, noexcept, )
    INVOCABLE_TRAITS_SPEC(const, , noexcept, )
    INVOCABLE_TRAITS_SPEC(, volatile, noexcept, )
    INVOCABLE_TRAITS_SPEC(const, volatile, noexcept )
    // and also variadic function versions
    INVOCABLE_TRAITS_SPEC(,,, ...)
    INVOCABLE_TRAITS_SPEC(const,,, ...)
    INVOCABLE_TRAITS_SPEC(,volatile,, ...)
    INVOCABLE_TRAITS_SPEC(const, volatile,, ...)
    INVOCABLE_TRAITS_SPEC(,, noexcept, ...)
    INVOCABLE_TRAITS_SPEC(const,, noexcept, ...)
    INVOCABLE_TRAITS_SPEC(,volatile, noexcept, ...)
    INVOCABLE_TRAITS_SPEC(const, volatile, noexcept, ...)
    // clean up
    #undef INVOCABLE_TRAITS_SPEC
    #undef IS_NONEMPTY

    // pointers to data members
    template <typename C, typename R>
    struct invocable_traits_impl<R C::*>
        : public invocable_traits_class<R,
                                        std::invoke_result_t<R C::*,C>,
                                        C,
                                        false, false, false, false
                                       > {};

    // pointers to functions
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...)>                 : public invocable_traits_impl<R(Args...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args...) noexcept>        : public invocable_traits_impl<R(Args...) noexcept> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...)>            : public invocable_traits_impl<R(Args..., ...)> {};
    template <typename R, typename... Args>
    struct invocable_traits_impl<R(*)(Args..., ...) noexcept>   : public invocable_traits_impl<R(Args..., ...) noexcept> {};


    // check if passed type has an operator(), can be true for struct/class, includes lambdas
    template <typename T>
    concept HasCallOperator = requires(T t)
    {
        t.operator();
    };
    // check if we can get operator(), will fail if overloaded (assuming above HasCallOperator does pass)
    template <typename T>
    concept CanGetCallOperator = requires(T)
    {
        invocable_traits_impl<decltype(&T::operator())>();
    };

    template <typename T, bool isCallable>
    struct invocable_traits_extract : invocable_traits_impl<decltype(&T::operator())> {};

    template <typename T>
    struct invocable_traits_extract<T, false>
    {
        static_assert(std::is_class_v<T>, "passed type is not a class, and thus cannot have an operator()");
        static_assert(!std::is_class_v<T> || HasCallOperator<T>, "passed type is a class that doesn't have an operator()");
        static_assert(!(!std::is_class_v<T> || HasCallOperator<T>) || CanGetCallOperator<T>, "passed type is a class that has an overloaded operator(), specify argument types in invocable_traits invocation to disambiguate which overload you wish to use");

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

    // catch all that doesn't match the various function signatures above
    // If T has an operator(), we go with that. Else, issue error message.
    template <typename T>
    struct invocable_traits_impl : invocable_traits_extract<T, HasCallOperator<T> && CanGetCallOperator<T>> {};
}

template <typename T>
struct invocable_traits : detail::invocable_traits_impl<std::decay_t<T>> {};
