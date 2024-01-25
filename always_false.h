#pragma once
#include <type_traits>

// helper needed to be able to do static_assert(false,...), e.g. to mark some constexpr if branches as todo
template <class...> constexpr std::false_type always_false_t{};
// version for use with non-type template arguments
template <auto...> constexpr std::false_type always_false_nt{};

