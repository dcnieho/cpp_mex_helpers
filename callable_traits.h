#pragma once

// derived and extended from https://github.com/kennytm/utils/blob/master/traits.hpp
// and
// http://coliru.stacked-crooked.com/a/6a87fadcf44c6a0f

#include <tuple>

template <typename T>
struct callable_traits : callable_traits<decltype(&std::decay_t<T>::operator())>
{};

#define SPEC_IMPL(cv,unused)                                               \
template <typename R, typename... Args>                        \
struct callable_traits<R(Args...) cv>                               \
{                                                                          \
    using arity = std::integral_constant<std::size_t, sizeof...(Args) >;   \
                                                                           \
    using result_type = R;                                                 \
                                                                           \
    template <std::size_t i>                                               \
    using arg = typename std::tuple_element<i, std::tuple<Args...,void>>::type; \
};                                               \
template <typename C, typename R>                        \
struct callable_traits<R C::* cv>                               \
{                                                                          \
    using arity = std::integral_constant<std::size_t, 0 >;   \
                                                                           \
    using result_type = R;                                                 \
                                                                           \
    template <std::size_t i>                                               \
    using arg = typename std::tuple_element<i, std::tuple<void>>::type; \
};                                               \
\
template <typename C, typename R, typename... Args>\
struct callable_traits<R(C::*)(Args...) cv> : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args> \
struct callable_traits<R(C::*)(Args...) cv &> : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                        \
struct callable_traits<R(C::*)(Args...) cv &&> : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                        \
struct callable_traits<R(C::*)(Args...) cv noexcept> : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                        \
struct callable_traits<R(C::*)(Args...) cv & noexcept> : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                        \
struct callable_traits<R(C::*)(Args...) cv && noexcept> : public callable_traits<R(Args...)> {};

template <typename R, typename... Args>
struct callable_traits<R(*)(Args...)> : public callable_traits<R(Args...)> {};

#define SPEC_0()  SPEC_IMPL(,)
#define SPEC_1(A) SPEC_IMPL(A,)

#define FUNC_CHOOSER(_f1, _f2, _f3, ...) _f3
#define FUNC_RECOMPOSER(argsWithParentheses) FUNC_CHOOSER argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT(...) FUNC_RECOMPOSER((__VA_ARGS__, SPEC_2, SPEC_1, )) // NB: SPEC_2 never used so doesn't have to be defined
#define NO_ARG_EXPANDER() ,,SPEC_0
#define MACRO_CHOOSER(...) CHOOSE_FROM_ARG_COUNT(NO_ARG_EXPANDER __VA_ARGS__ ())
#define SPEC(...) MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)


SPEC()
SPEC(const)
SPEC(volatile)
SPEC(const volatile)
