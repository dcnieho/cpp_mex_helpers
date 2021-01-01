#pragma once

template <class T, template <class...> class Template>
struct is_specialization
    : std::false_type
{};

template <template <class...> class Template, class... Args>
struct is_specialization<Template<Args...>, Template>
    : std::true_type
{};

template<class T1, template <class...> class T2>
static constexpr bool const is_specialization_v = is_specialization<std::decay_t<T1>, T2>::value;