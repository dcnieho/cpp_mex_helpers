#pragma once
#include <type_traits>

// needed helper to be able to do static_assert(false,...) in some constexpr if branches below, e.g. to mark them as todo TODO
template <class...> constexpr std::false_type always_false{};

