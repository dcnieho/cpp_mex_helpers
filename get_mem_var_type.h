#pragma once

// type trait to extract member variable type from a pointer-to-member-variable
template <typename C>
struct memVarType;

template <class C, typename T>
struct memVarType<T C::*>
{
    using type = T;
};

template <class C>
using memVarType_t = typename memVarType<C>::type;