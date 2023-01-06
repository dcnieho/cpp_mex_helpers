#pragma once
#include <type_traits>
#include <tuple>

#include "pack_utils.h"


namespace nested_field {
    //// struct of arrays
    // machinery to turn a container of objects into a single struct with an array per object field
    // get field indicated by list of pointers-to-member-variable in fields
    template <typename O, typename T, typename... Os, typename... Ts>
    constexpr auto get(const O& obj, T O::* field1, Ts Os::*... fields)
    {
        if constexpr (!sizeof...(fields))
            return obj.*field1;
        else
            return get(obj.*field1, fields...);
    }

    // get field indicated by list of pointers-to-member-variable in fields, process return value by either:
    // 1. transform by applying callable; or
    // 2. cast return value to user specified type
    template <typename Obj, typename OutOrFun, typename... Fs, typename... Ts>
    constexpr auto get(const Obj& obj, OutOrFun o, Ts Fs::*... fields)
    {
        if constexpr (std::is_invocable_v<OutOrFun, last<0, Obj, Ts...>>)
            return std::invoke(o, get(obj, fields...));
        else
            return static_cast<OutOrFun>(get(obj, fields...));
    }

    template <typename Obj, typename... Fs>
    constexpr auto getWrapper(const Obj& obj, Fs... fields)
    {
        // if last is pointer-to-member-variable, but previous is not (this would be a type tag then), swap the last two to put the type tag last
        if      constexpr (sizeof...(Fs) > 1 && std::is_member_object_pointer_v<last<0, Obj, Fs...>> && !std::is_member_object_pointer_v<last<1, Obj, Fs...>>)
            return rotate_right_except_last(
                [&obj](auto... elems) constexpr
                {
                    return get(obj, elems...);
                }, fields...);
        // if last is pointer-to-member-variable, no casting of return value requested through type tag, call getField
        else if constexpr (std::is_member_object_pointer_v<last<0, Obj, Fs...>>)
            return get(obj, fields...);
        // if last is an enum, compare the value of the field to it
        // this turns enum fields into a boolean given reference enum value for which true should be returned
        else if constexpr (std::is_enum_v<last<0, Obj, Fs...>>)
        {
            auto tuple = std::make_tuple(fields...);
            return drop_last(
            [&obj](auto... elems) constexpr
            {
                return get(obj, elems...);
            }, fields...) == std::get<sizeof...(Fs) - 1>(tuple);
        }
        else
            // if last is not pointer-to-member-variable, call getField with correct order of arguments
            // last is type to cast return value to, or lambda to apply to return value
            return rotate_right(
            [&obj](auto... elems) constexpr
            {
                return get(obj, elems...);
            }, fields...);
    }
}