#pragma once

template <typename C, class R>
struct replace_specialization_type;

template <template <class...> class Container, typename... Args, class R>
struct replace_specialization_type<Container<Args...>, R>
{
    using type = Container<R>;
};

template <class C, class R>
using replace_specialization_type_t = typename replace_specialization_type<C,R>::type;