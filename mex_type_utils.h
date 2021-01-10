#pragma once
#include <type_traits>

#include "mex_type_utils_fwd.h"
#include "pack_utils.h"
#include "always_false.h"


namespace mxTypes {
    //// functionality to convert C++ types to MATLAB ClassIDs and back
    template <typename T> struct typeToMxClass { static_assert(always_false<T>, "typeToMxClass not implemented for this type"); static constexpr mxClassID value = mxUNKNOWN_CLASS; };
    template <>           struct typeToMxClass<double  > { static constexpr mxClassID value = mxDOUBLE_CLASS; };
    template <>           struct typeToMxClass<float   > { static constexpr mxClassID value = mxSINGLE_CLASS; };
    template <>           struct typeToMxClass<bool    > { static constexpr mxClassID value = mxLOGICAL_CLASS; };
    template <>           struct typeToMxClass<uint64_t> { static constexpr mxClassID value = mxUINT64_CLASS; };
    template <>           struct typeToMxClass<int64_t > { static constexpr mxClassID value = mxINT64_CLASS; };
    template <>           struct typeToMxClass<uint32_t> { static constexpr mxClassID value = mxUINT32_CLASS; };
    template <>           struct typeToMxClass<int32_t > { static constexpr mxClassID value = mxINT32_CLASS; };
    template <>           struct typeToMxClass<uint16_t> { static constexpr mxClassID value = mxUINT16_CLASS; };
    template <>           struct typeToMxClass<int16_t > { static constexpr mxClassID value = mxINT16_CLASS; };
    template <>           struct typeToMxClass<uint8_t > { static constexpr mxClassID value = mxUINT8_CLASS; };
    template <>           struct typeToMxClass<int8_t  > { static constexpr mxClassID value = mxINT8_CLASS; };
    template <typename T>
    constexpr mxClassID typeToMxClass_v = typeToMxClass<T>::value;

    template <typename T>
    struct typeNeedsMxCellStorage
    {
        static constexpr bool value = !std::is_arithmetic_v<T>; // true for integrals and floating point, and bool is included in integral
    };
    template <typename T>
    constexpr bool typeNeedsMxCellStorage_v = typeNeedsMxCellStorage<T>::value;

    template <mxClassID T>
    constexpr mxClassID MxClassToType()
    {
        if      constexpr (T == mxDOUBLE_CLASS)
            using type = double;
        else if constexpr (T == mxSINGLE_CLASS)
            using type = float;
        else if constexpr (T == mxLOGICAL_CLASS)
            using type = bool;
        else if constexpr (T == mxUINT64_CLASS)
            using type = uint64_t;
        else if constexpr (T == mxINT64_CLASS)
            using type = int64_t;
        else if constexpr (T == mxUINT32_CLASS)
            using type = uint32_t;
        else if constexpr (T == mxINT32_CLASS)
            using type = int32_t;
        else if constexpr (T == mxUINT16_CLASS)
            using type = uint16_t;
        else if constexpr (T == mxINT16_CLASS)
            using type = int16_t;
        else if constexpr (T == mxUINT8_CLASS)
            using type = uint8_t;
        else if constexpr (T == mxINT8_CLASS)
            using type = int8_t;
    }

    template <mxClassID T>
    constexpr const char* mxClassToString()
    {
        if      constexpr (T == mxDOUBLE_CLASS)
            return "double";
        else if constexpr (T == mxSINGLE_CLASS)
            return "float";
        else if constexpr (T == mxLOGICAL_CLASS)
            return "logical";
        else if constexpr (T == mxUINT64_CLASS)
            return "uint64";
        else if constexpr (T == mxINT64_CLASS)
            return "int64";
        else if constexpr (T == mxUINT32_CLASS)
            return "uint32";
        else if constexpr (T == mxINT32_CLASS)
            return "int32";
        else if constexpr (T == mxUINT16_CLASS)
            return "uint16";
        else if constexpr (T == mxINT16_CLASS)
            return "int16";
        else if constexpr (T == mxUINT8_CLASS)
            return "uint8";
        else if constexpr (T == mxINT8_CLASS)
            return "int8";
        else if constexpr (T == mxCHAR_CLASS)
            return "string";
    }

    //// converters of generic data types to MATLAB variables
    //// to simple variables
    mxArray* ToMatlab(std::string str_)
    {
        return mxCreateString(str_.c_str());
    }

    template<class T>
    typename std::enable_if_t<std::is_arithmetic_v<T>, mxArray*>
    ToMatlab(T val_)
    {
        mxArray* temp;
        auto storage = static_cast<T*>(mxGetData(temp = mxCreateUninitNumericMatrix(1, 1, typeToMxClass_v<T>, mxREAL)));
        *storage = val_;
        return temp;
    }

    template<class Cont, typename... Extras>
    typename std::enable_if_t<is_container_v<Cont>, mxArray*>
    ToMatlab(Cont data_, Extras&&... extras_)
    {
        mxArray* temp = nullptr;
        using V = typename Cont::value_type;
        if constexpr (typeNeedsMxCellStorage_v<V>)
        {
            // output cell array
            temp = mxCreateCellMatrix(static_cast<mwSize>(data_.size()), 1);
            mwIndex i = 0;
            for (auto&& item : data_)
                mxSetCell(temp, i++, ToMatlab(item,std::forward<Extras>(extras_)...));
        }
        else if constexpr (typeToMxClass_v<V> != mxSTRUCT_CLASS)
        {
            // output array
            auto storage = static_cast<V*>(mxGetData(temp = mxCreateUninitNumericMatrix(static_cast<mwSize>(data_.size()), 1, typeToMxClass_v<V>, mxREAL)));

            if constexpr (is_guaranteed_contiguous_v<Cont>)
            {
                // contiguous storage, can memcopy
                if (data_.size())
                    memcpy(storage, &data_[0], data_.size() * sizeof(data_[0]));
            }
            else
            {
                // non-contiguous storage, copy one at a time
                for (auto&& item : data_)
                    (*storage++) = item;
            }
        }
        else // NB: if constexpr (typeToMxClass_v<V> == mxSTRUCT_CLASS)
        {
            // output array of structs
            mwIndex i = 0;
            if (!data_.size())
                if constexpr (std::is_default_constructible_v<V>)   // try hard to produce struct with empty fields
                    temp = ToMatlab(V{}, i++, 0, temp, std::forward<Extras>(extras_)...);
                else    // fall back to just empty
                    temp = mxCreateDoubleMatrix(0, 0, mxREAL);
            else
                for (auto&& item : data_)
                    temp = ToMatlab(item, i++, static_cast<mwSize>(data_.size()), temp, std::forward<Extras>(extras_)...);
        }
        return temp;
    }

    mxArray* ToMatlab(std::monostate)
    {
        return mxCreateDoubleMatrix(0, 0, mxREAL);
    }

    template <class... Types>
    mxArray* ToMatlab(std::variant<Types...> val_)
    {
        return std::visit([](auto& a)->mxArray* {return ToMatlab(a); }, val_);
    }

    template <class T>
    mxArray* ToMatlab(std::optional<T> val_)
    {
        if (!val_)
            return mxCreateDoubleMatrix(0, 0, mxREAL);
        else
            return ToMatlab(*val_);
    }

    template <class T>
    mxArray* ToMatlab(std::shared_ptr<T> val_)
    {
        if (!val_)
            return mxCreateDoubleMatrix(0, 0, mxREAL);
        else
            return ToMatlab(*val_);
    }

    template <template <class...> class Cont, class... Args>
    typename std::enable_if_t<
        (
            is_specialization_v<Cont<Args...>, std::map> ||
            is_specialization_v<Cont<Args...>, std::unordered_map>
            )
        &&
        (
            std::is_same_v<typename Cont<Args...>::key_type, std::string> ||
            std::is_convertible_v<typename Cont<Args...>::key_type, std::string>
            ),
        mxArray*>
        ToMatlab(Cont<Args...> data_)
    {
        // get all keys
        std::vector<std::string> keys;
        for (auto&& [key, val]: data_)
            keys.push_back(std::move(key));
        // get a vector of pointers to beginning of strings, so we can pass it to the C API of mxCreateStructMatrix
        std::vector<const char*> fields;
        for (auto&& k: keys)
            fields.push_back(k.c_str());

        // create the struct
        auto storage = mxCreateStructMatrix(1, 1, static_cast<int>(fields.size()), fields.data());

        // copy data into it
        for (int i=0; auto&& [key, val] : data_)
            mxSetFieldByNumber(storage, 0, i++, ToMatlab(val));

        return storage;
    }
    template <template <class...> class Cont, class... Args>
    typename std::enable_if_t<
            is_specialization_v<Cont<Args...>, std::set> ||
            is_specialization_v<Cont<Args...>, std::unordered_set> ||
            is_specialization_v<Cont<Args...>, std::multiset> ||
            is_specialization_v<Cont<Args...>, std::unordered_multiset>,
        mxArray*>
        ToMatlab(Cont<Args...> data_)
    {
        mxArray* storage = mxCreateCellMatrix(1, static_cast<mwSize>(data_.size()));
        
        for (mwIndex i = 0; auto && item: data_)
        {
            mxSetCell(storage, i, ToMatlab(item));
            ++i;
        }

        return storage;
    }

    template <template <class...> class T, class... Args>
    typename std::enable_if_t<
            is_specialization_v<T<Args...>, std::pair> ||
            is_specialization_v<T<Args...>, std::tuple>,
        mxArray*>
        ToMatlab(T<Args...> val_)
    {
        mxArray* storage = mxCreateCellMatrix(static_cast<mwSize>(sizeof...(Args)), 1);
        mwIndex i = 0;
        std::apply([&](auto&&... args) {(mxSetCell(storage, i++, ToMatlab(args)), ...); }, val_);

        return storage;
    }
    template<class Cont>
    typename std::enable_if_t<
            is_container_v<Cont> && (
                is_specialization_v<typename Cont::value_type, std::pair> ||
                is_specialization_v<typename Cont::value_type, std::tuple>),
        mxArray*>
        ToMatlab(Cont data_)
    {
        static constexpr size_t N = std::tuple_size_v<typename Cont::value_type>;
        size_t nElem = data_.size();
        mxArray* storage = mxCreateCellMatrix(static_cast<mwSize>(nElem), static_cast<mwSize>(N));
        for (mwIndex i = 0; auto&& item: data_)
        {
            mwIndex j = 0;
            std::apply([&](auto&&... args) {(mxSetCell(storage, (j++)*nElem+i, ToMatlab(args)), ...); }, item);
            ++i;
        }

        return storage;
    }

    // generic ToMatlab that converts provided data through type tag dispatch
    template <class T, class U>
    typename std::enable_if_t<!is_container_v<T>, mxArray*>
    ToMatlab(T val_, U)
    {
        return ToMatlab(static_cast<U>(val_));
    }


    //// struct of arrays
    // machinery to turn a container of objects into a single struct with an array per object field
    // get field indicated by list of pointers-to-member-variable in fields
    template <typename O, typename T, typename... Os, typename... Ts>
    constexpr auto getField(const O& obj, T O::* field1, Ts Os::*... fields)
    {
        if constexpr (!sizeof...(fields))
            return obj.*field1;
        else
            return getField(obj.*field1, std::forward<Ts Os::*>(fields)...);
    }

    // get field indicated by list of pointers-to-member-variable in fields, process return value by either:
    // 1. transform by applying callable; or
    // 2. cast return value to user specified type
    template <typename Obj, typename OutOrFun, typename... Fs, typename... Ts>
    constexpr auto getField(const Obj& obj, OutOrFun o, Ts Fs::*... fields)
    {
        if constexpr (std::is_invocable_v<OutOrFun, last<0, Obj, Ts...>>)
            return std::invoke(o, getField(obj, std::forward<Ts Fs::*>(fields)...));
        else
            return static_cast<OutOrFun>(getField(obj, std::forward<Ts Fs::*>(fields)...));
    }

    template <typename Obj, typename... Fs>
    constexpr auto getFieldWrapper(const Obj& obj, Fs... fields)
    {
        // if last is pointer-to-member-variable, but previous is not (this would be a type tag then), swap the last two to put the type tag last
        if      constexpr (sizeof...(Fs) > 1 && std::is_member_object_pointer_v<last<0, Obj, Fs...>> && !std::is_member_object_pointer_v<last<1, Obj, Fs...>>)
            return rotate_right_except_last(
                [&](auto... elems) constexpr
                {
                    return getField(obj, elems...);
                }, std::forward<Fs>(fields)...);
        // if last is pointer-to-member-variable, no casting of return value requested through type tag, call getField
        else if constexpr (std::is_member_object_pointer_v<last<0, Obj, Fs...>>)
            return getField(obj, std::forward<Fs>(fields)...);
        // if last is an enum, compare the value of the field to it
        // this turns enum fields into a boolean given reference enum value for which true should be returned
        else if constexpr (std::is_enum_v<last<0, Obj, Fs...>>)
        {
            auto tuple = std::make_tuple(std::forward<Fs>(fields)...);
            return drop_last(
            [&](auto... elems) constexpr
            {
                return getField(obj, elems...);
            }, std::forward<Fs>(fields)...) == std::get<sizeof...(Fs) - 1>(tuple);
        }
        else
            // if last is not pointer-to-member-variable, call getField with correct order of arguments
            // last is type to cast return value to, or lambda to apply to return value
            return rotate_right(
            [&](auto... elems) constexpr
            {
                return getField(obj, elems...);
            }, std::forward<Fs>(fields)...);
    }

    // default output is storage type corresponding to the type of the member variable accessed through this function, but it can be overridden through type tag dispatch (see getFieldWrapper implementation)
    template<typename Cont, typename... Fs>
    typename std::enable_if_t<is_container_v<Cont>, mxArray*>
    FieldToMatlab(const Cont& data_, Fs... fields)
    {
        mxArray* temp;
        using V = typename Cont::value_type;
        using U = decltype(getFieldWrapper(std::declval<V>(), std::forward<Fs>(fields)...));

        if constexpr (typeNeedsMxCellStorage_v<U>)
        {
            // output cell array
            temp = mxCreateCellMatrix(static_cast<mwSize>(data_.size()), 1);
            mwIndex i = 0;
            for (auto&& item : data_)
                mxSetCell(temp, i++, ToMatlab(getFieldWrapper(item, std::forward<Fs>(fields)...)));
        }
        else if constexpr (typeToMxClass_v<U> != mxSTRUCT_CLASS)
        {
            // output array
            auto storage = static_cast<U*>(mxGetData(temp = mxCreateUninitNumericMatrix(static_cast<mwSize>(data_.size()), 1, typeToMxClass_v<U>, mxREAL)));

            if (data_.size())
            {
                for (auto&& item : data_)
                    (*storage++) = getFieldWrapper(item, std::forward<Fs>(fields)...);
            }
        }
        else // NB: if constexpr (typeToMxClass_v<U> == mxSTRUCT_CLASS)
        {
            static_assert(always_false<Cont>, "Shouldn't happen, check you didn't override typeToMxClass for this type");   // or is this a TODO implement? Analyze situation when i encounter it
        }

        return temp;
    }
}