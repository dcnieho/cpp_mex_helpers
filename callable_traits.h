#pragma once

// derived and extended from https://github.com/kennytm/utils/blob/master/traits.hpp
// and https://stackoverflow.com/a/28213747

#include <tuple>
#include <type_traits>

// get at operator() of any struct/class defining it (this includes lambdas)
template <typename T>
struct callable_traits : callable_traits<decltype(&std::decay_t<T>::operator())>
{};

#define SPEC(cv,unused)                                                         \
/* handle anything with a function-like signature */                            \
template <typename R, typename... Args>                                         \
struct callable_traits<R(Args...) cv>                                           \
{                                                                               \
    using arity = std::integral_constant<std::size_t, sizeof...(Args)>;         \
                                                                                \
    using result_type = R;                                                      \
                                                                                \
    template <std::size_t i>                                                    \
    using arg = typename std::tuple_element<i, std::tuple<Args..., void>>::type;\
};                                                                              \
/* handle pointers to data members */                                           \
template <typename C, typename R>                                               \
struct callable_traits<R C::* cv>                                               \
{                                                                               \
    using arity = std::integral_constant<std::size_t, 0>;                       \
                                                                                \
    using result_type = R;                                                      \
                                                                                \
    template <std::size_t i>                                                    \
    using arg = typename std::tuple_element<i, std::tuple<void>>::type;         \
};                                                                              \
/* handle pointers to member functions, all possible iterations of reference and noexcept */    \
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv>             : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv &>           : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv &&>          : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv noexcept>    : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv & noexcept>  : public callable_traits<R(Args...)> {};\
template <typename C, typename R, typename... Args>                                             \
struct callable_traits<R(C::*)(Args...) cv && noexcept> : public callable_traits<R(Args...)> {};

// cover all const and volatile permutations
SPEC(, )
SPEC(const, )
SPEC(volatile, )
SPEC(const volatile, )

// handle pointers to free functions
template <typename R, typename... Args>
struct callable_traits<R(*)(Args...)> : public callable_traits<R(Args...)> {};
