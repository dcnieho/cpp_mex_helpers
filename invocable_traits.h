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
// One exception is when passing a functor with multiple operator() defined
// (NB: this includes the overload trick with lambdas): extra type arguments
// can be passed to specify the argument types of the desired overload.
// Either a static_assert() is triggered if no operator() taking the specified
// input argument types exists, or invocable_traits for the specified overload
// are made available.

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


    template <typename T, typename... OverloadArgs>
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

    // machinery to select a specific overloaded operator()
#   define MAKE_CONST(...) __VA_OPT__(const)
#   define MAKE_VOLATILE(...) __VA_OPT__(volatile)
#   define MAKE_NOEXCEPT(...) __VA_OPT__(noexcept)
#   define MAKE_VARIADIC(...) __VA_OPT__(, ...)
#   define DECLARE_CONCEPT_NAME(c,vo,e,va) HasSpecificCallOperator##c##va##e##vo
#   define DECLARE_CONCEPT(c,vo,e,va)                   \
    template <typename C, typename... OverloadArgs>     \
    concept DECLARE_CONCEPT_NAME(c,vo,e,va) =           \
        requires (std::invoke_result_t<C, OverloadArgs...>(C::* func)(OverloadArgs... MAKE_VARIADIC(va)) MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e))\
    { func = &C::operator(); };

    template <typename C, typename... OverloadArgs>
    concept HasSpecificCallOperatorNone =
        requires (std::invoke_result_t<C, OverloadArgs...>(C::* func)(OverloadArgs...))
    { func = &C::operator(); };
    DECLARE_CONCEPT(, Va, , )
    DECLARE_CONCEPT(, , E, )
    DECLARE_CONCEPT(, Va, E, )
    DECLARE_CONCEPT(, , , Vo)
    DECLARE_CONCEPT(, Va, , Vo)
    DECLARE_CONCEPT(, , E, Vo)
    DECLARE_CONCEPT(, Va, E, Vo)
    DECLARE_CONCEPT(C, , , )
    DECLARE_CONCEPT(C, Va, , )
    DECLARE_CONCEPT(C, , E, )
    DECLARE_CONCEPT(C, Va, E, )
    DECLARE_CONCEPT(C, , , Vo)
    DECLARE_CONCEPT(C, Va, , Vo)
    DECLARE_CONCEPT(C, , E, Vo)
    DECLARE_CONCEPT(C, Va, E, Vo)
#   undef DECLARE_CONCEPT

#   define MAKE_CONCEPT_SIGNATURE(c,vo,e,va) DECLARE_CONCEPT_NAME(c,vo,e,va)<C, OverloadArgs...>

    template <typename C, typename... OverloadArgs>
    concept HasSpecificCallOperator = HasCallOperator<C> &&
        (HasSpecificCallOperatorNone<C, OverloadArgs...> || MAKE_CONCEPT_SIGNATURE(, Va, , ) || MAKE_CONCEPT_SIGNATURE(, , E, ) || MAKE_CONCEPT_SIGNATURE(, Va, E, ) || MAKE_CONCEPT_SIGNATURE(, , , Vo) || MAKE_CONCEPT_SIGNATURE(, Va, , Vo) || MAKE_CONCEPT_SIGNATURE(, , E, Vo) || MAKE_CONCEPT_SIGNATURE(, Va, E, Vo) || MAKE_CONCEPT_SIGNATURE(C, , , ) || MAKE_CONCEPT_SIGNATURE(C, Va, , ) || MAKE_CONCEPT_SIGNATURE(C, , E, ) || MAKE_CONCEPT_SIGNATURE(C, Va, E, ) || MAKE_CONCEPT_SIGNATURE(C, , , Vo) || MAKE_CONCEPT_SIGNATURE(C, Va, , Vo) || MAKE_CONCEPT_SIGNATURE(C, , E, Vo) || MAKE_CONCEPT_SIGNATURE(C, Va, E, Vo));
    ;

#   define DEFINE_RESOLVE_OVERLOAD(c,vo,e,va)           \
    template <typename C, typename... OverloadArgs>     \
    requires MAKE_CONCEPT_SIGNATURE(c,vo,e,va)          \
    auto invocable_traits_resolve_overload()            \
    {                                                   \
        return static_cast<std::invoke_result_t<C, OverloadArgs...>(C::*)(OverloadArgs... MAKE_VARIADIC(va)) MAKE_CONST(c) MAKE_VOLATILE(vo) MAKE_NOEXCEPT(e)>(&C::operator());\
    };

    template <typename C, typename... OverloadArgs>
    requires HasSpecificCallOperatorNone<C, OverloadArgs...>
    auto invocable_traits_resolve_overload()
    {
        return static_cast<std::invoke_result_t<C, OverloadArgs...>(C::*)(OverloadArgs...)>(&C::operator());
    };
    DEFINE_RESOLVE_OVERLOAD( ,Va, , )
    DEFINE_RESOLVE_OVERLOAD( , ,E, )
    DEFINE_RESOLVE_OVERLOAD( ,Va,E, )
    DEFINE_RESOLVE_OVERLOAD( , , ,Vo)
    DEFINE_RESOLVE_OVERLOAD( ,Va, ,Vo)
    DEFINE_RESOLVE_OVERLOAD( , ,E,Vo)
    DEFINE_RESOLVE_OVERLOAD( ,Va,E,Vo)
    DEFINE_RESOLVE_OVERLOAD(C, , , )
    DEFINE_RESOLVE_OVERLOAD(C,Va, , )
    DEFINE_RESOLVE_OVERLOAD(C, ,E, )
    DEFINE_RESOLVE_OVERLOAD(C,Va,E, )
    DEFINE_RESOLVE_OVERLOAD(C, , ,Vo)
    DEFINE_RESOLVE_OVERLOAD(C,Va, ,Vo)
    DEFINE_RESOLVE_OVERLOAD(C, ,E,Vo)
    DEFINE_RESOLVE_OVERLOAD(C,Va,E,Vo)

#   undef MAKE_CONST
#   undef MAKE_VOLATILE
#   undef MAKE_NOEXCEPT
#   undef MAKE_VARIADIC
#   undef DECLARE_CONCEPT_NAME
#   undef MAKE_CONCEPT_SIGNATURE
#   undef DEFINE_RESOLVE_OVERLOAD

    // specific overloaded operator() is available, use it for analysis
    template <typename T, bool, typename... OverloadArgs>
    struct invocable_traits_extract : invocable_traits_impl<std::decay_t<decltype(invocable_traits_resolve_overload<T, OverloadArgs...>())>> {};

    // unambiguous operator() is available, use it for analysis
    template <typename T, bool B>
    struct invocable_traits_extract<T, B> : invocable_traits_impl<decltype(&T::operator())> {};

    // to reduce excessive compiler error output
    struct invocable_traits_error
    {
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
    struct invocable_traits_extract<T, false> : invocable_traits_error
    {
        static_assert(std::is_class_v<T>, "passed type is not a class, and thus cannot have an operator()");
        static_assert(!std::is_class_v<T> || HasCallOperator<T>, "passed type is a class that doesn't have an operator()");
        static_assert(!(!std::is_class_v<T> || HasCallOperator<T>) || CanGetCallOperator<T>, "passed type is a class that has an overloaded operator(), specify argument types in invocable_traits invocation to disambiguate which overload you wish to use");
    };

    template <typename T, typename... OverloadArgs>
    struct invocable_traits_extract<T, false, OverloadArgs...> : invocable_traits_error
    {
        static_assert(std::is_class_v<T>, "passed type is not a class, and thus cannot have an operator()");
        static_assert(!std::is_class_v<T> || HasCallOperator<T>, "passed type is a class that doesn't have an operator()");
        static_assert(!(!std::is_class_v<T> || HasCallOperator<T>) || HasSpecificCallOperator<T, OverloadArgs...>, "passed type is a class that doesn't have an operator() that declares the specified argument types");
    };

    // catch all that doesn't match the various function signatures above
    // If T has an operator(), we go with that. Else, issue error message.
    template <typename T, typename... OverloadArgs>
    struct invocable_traits_impl : invocable_traits_extract<T, HasSpecificCallOperator<T, OverloadArgs...>, OverloadArgs...> {};
    template <typename T>
    struct invocable_traits_impl<T> : invocable_traits_extract<T, HasCallOperator<T> && CanGetCallOperator<T>> {};
}

template <typename T, typename... OverloadArgs>
struct invocable_traits : detail::invocable_traits_impl<std::remove_reference_t<T>, OverloadArgs...> {};
template <typename T>
struct invocable_traits<T> : detail::invocable_traits_impl<std::decay_t<T>> {};
