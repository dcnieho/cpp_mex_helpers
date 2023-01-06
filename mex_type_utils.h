#pragma once
#include <type_traits>
#include <functional>

#include "mex_type_utils_fwd.h"
#include "pack_utils.h"
#include "always_false.h"
#include "get_field_nested.h"


namespace mxTypes {
    //// functionality to convert C++ types to MATLAB ClassIDs and back
    template <typename T> struct typeToMxClass { static_assert(always_false<T>, "typeToMxClass not implemented for this type"); static constexpr mxClassID value = mxUNKNOWN_CLASS; };
    template <>           struct typeToMxClass<double  > { static constexpr mxClassID value = mxDOUBLE_CLASS; };
    template <>           struct typeToMxClass<float   > { static constexpr mxClassID value = mxSINGLE_CLASS; };
    template <>           struct typeToMxClass<bool    > { static constexpr mxClassID value = mxLOGICAL_CLASS; };
    template <>           struct typeToMxClass<char    > { static constexpr mxClassID value = mxCHAR_CLASS; };
#ifndef IS_OCTAVE
    template <>           struct typeToMxClass<mxChar  > { static constexpr mxClassID value = mxCHAR_CLASS; };
#endif
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
        static constexpr bool value = !std::is_arithmetic_v<T>; // std::is_arithmetic_v is true for integrals and floating point, and bool is included in integral
    };
    template <typename T>
    constexpr bool typeNeedsMxCellStorage_v = typeNeedsMxCellStorage<T>::value;

    template <typename T>
    struct typeDumpVectorOneAtATime
    {
        static constexpr bool value = false;
    };
    template <typename T>
    constexpr bool typeDumpVectorOneAtATime_v = typeDumpVectorOneAtATime<T>::value;

    template <mxClassID T>
    constexpr mxClassID MxClassToType()
    {
        if      constexpr (T == mxDOUBLE_CLASS)
            using type = double;
        else if constexpr (T == mxSINGLE_CLASS)
            using type = float;
        else if constexpr (T == mxLOGICAL_CLASS)
            using type = bool;
        else if constexpr (T == mxCHAR_CLASS)
            using type = mxChar;
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

        return mxUNKNOWN_CLASS; // to make some compilers happy, return value is not to be used.
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
        else if constexpr (T == mxCELL_CLASS)
            return "cell";
        else if constexpr (T == mxFUNCTION_CLASS)
            return "function";
#ifndef IS_OCTAVE
        else if constexpr (T == mxOPAQUE_CLASS)
            return "opaque";
        else if constexpr (T == mxOBJECT_CLASS)
            return "object";
#endif
        else if constexpr (T == mxSTRUCT_CLASS)
            return "struct";
        else
            return "unknown";
    }

    //// converters of generic data types to MATLAB variables
    //// to simple variables
    mxArray* ToMatlab(std::string str_)
    {
        return mxCreateString(str_.c_str());
    }

    template<class T>
    requires std::is_arithmetic_v<T>
    mxArray* ToMatlab(T val_)
    {
        mxArray* temp;
        auto storage = static_cast<T*>(mxGetData(temp = mxCreateUninitNumericMatrix(1, 1, typeToMxClass_v<T>, mxREAL)));
        *storage = val_;
        return temp;
    }

    template<class Cont, typename... Extras>
    requires Container<Cont>
    mxArray* ToMatlab(Cont data_, Extras&&... extras_)
    {
        mxArray* temp = nullptr;
        using V = typename Cont::value_type;
        if constexpr (typeNeedsMxCellStorage_v<V>)
        {
            // output cell array
            temp = mxCreateCellMatrix(static_cast<mwSize>(data_.size()), 1);
            mwIndex i = 0;
            if constexpr (!typeDumpVectorOneAtATime_v<V>)
            {
                for (auto&& item : data_)
                    mxSetCell(temp, i++, ToMatlab(item, std::forward<Extras>(extras_)...));
            }
            else
            {
                // iterate backward, remove item that was just converted to matlab
                i = static_cast<mwSize>(data_.size());
                for (auto rit = std::rbegin(data_); rit != std::rend(data_); )
                {
                    mxSetCell(temp, --i, ToMatlab(*rit, std::forward<Extras>(extras_)...));
                    rit = decltype(rit)(data_.erase(std::next(rit).base()));
                }
            }
        }
        else if constexpr (typeToMxClass_v<V> != mxSTRUCT_CLASS)
        {
            // output array
            static_assert(sizeof...(Extras) < 2, "Only 0 (normal case) or 1 (type tag dispatch) extra arguments to ToMatlab() are supported for this branch.");
            using outputType = typename std::tuple_element<0, std::tuple<Extras..., V>>::type;  // if Extras... is an empty pack, V is output, else first type in Extras...
            auto storage = static_cast<outputType*>(mxGetData(temp = mxCreateUninitNumericMatrix(static_cast<mwSize>(data_.size()), 1, typeToMxClass_v<outputType>, mxREAL)));

            if (!data_.empty())
            {
                if constexpr (ContiguousStorage<Cont> && !typeDumpVectorOneAtATime_v<outputType> && sizeof...(Extras)==0)
                {
                    // contiguous storage, can memcopy, unless want to remove or convert each element after its copied
                    memcpy(storage, &data_[0], data_.size() * sizeof(data_[0]));
                }
                else
                {
                    // non-contiguous storage or one at a time explicitly requested: copy one at a time
                    if constexpr (!typeDumpVectorOneAtATime_v<V>)
                    {
                        for (auto&& item : data_)
                            (*storage++) = static_cast<outputType>(item);
                    }
                    else
                    {
                        // iterate backward, remove item that was just converted to matlab
                        storage += data_.size();
                        for (auto rit = std::rbegin(data_); rit != std::rend(data_); )
                        {
                            (*--storage) = static_cast<outputType>(*rit);
                            rit = decltype(rit)(data_.erase(std::next(rit).base()));
                        }
                    }
                }
            }
        }
        else // NB: if constexpr (typeToMxClass_v<V> == mxSTRUCT_CLASS)
        {
            // output array of structs
            mwIndex i = 0;
            if (data_.empty())
                if constexpr (std::is_default_constructible_v<V>)   // try hard to produce struct with empty fields
                    temp = ToMatlab(V{}, i++, 0, temp, std::forward<Extras>(extras_)...);
                else    // fall back to just empty
                    temp = mxCreateDoubleMatrix(0, 0, mxREAL);
            else
                if constexpr (!typeDumpVectorOneAtATime_v<V>)
                {
                    for (auto&& item : data_)
                        temp = ToMatlab(item, i++, static_cast<mwSize>(data_.size()), temp, std::forward<Extras>(extras_)...);
                }
                else
                {
                    // iterate backward, remove item that was just converted to matlab
                    i = static_cast<mwSize>(data_.size());
                    for (auto rit = std::rbegin(data_); rit != std::rend(data_); )
                    {
                        temp = ToMatlab(*rit, --i, static_cast<mwSize>(data_.size()), temp, std::forward<Extras>(extras_)...);
                        rit = decltype(rit)(data_.erase(std::next(rit).base()));
                    }
                }
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
        return std::visit([](auto& a) {return ToMatlab(a); }, val_);
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
    requires
        (
            is_specialization_v<Cont<Args...>, std::map> ||
            is_specialization_v<Cont<Args...>, std::unordered_map>
            )
        &&
            std::is_convertible_v<typename Cont<Args...>::key_type, std::string>
    mxArray* ToMatlab(Cont<Args...> data_)
    {
        // get all keys
        std::vector<std::string> keys;
        for (auto&& [key, val]: data_)
            keys.push_back(std::move(key));
        // get a vector of pointers to beginning of strings, so we can pass it to the C API of mxCreateStructMatrix
        std::vector<const char*> fields;
        for (const auto& k: keys)
            fields.push_back(k.c_str());

        // create the struct
        auto storage = mxCreateStructMatrix(1, 1, static_cast<int>(fields.size()), fields.data());

        // copy data into it
        for (int i=0; auto&& [key, val] : data_)
            mxSetFieldByNumber(storage, 0, i++, ToMatlab(val));

        return storage;
    }
    template <template <class...> class Cont, class... Args>
    requires
        is_specialization_v<Cont<Args...>, std::set> ||
        is_specialization_v<Cont<Args...>, std::unordered_set> ||
        is_specialization_v<Cont<Args...>, std::multiset> ||
        is_specialization_v<Cont<Args...>, std::unordered_multiset>
    mxArray* ToMatlab(Cont<Args...> data_)
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
    requires
        is_specialization_v<T<Args...>, std::pair> ||
        is_specialization_v<T<Args...>, std::tuple>
    mxArray* ToMatlab(T<Args...> val_)
    {
        return ToMatlab(std::array{ val_ });      // forward to container version, to not duplicate logic
    }
    template<class Cont>
    requires
        Container<Cont> && (
            is_specialization_v<typename Cont::value_type, std::pair> ||
            is_specialization_v<typename Cont::value_type, std::tuple>)
    mxArray* ToMatlab(Cont data_)
    {
        static constexpr size_t N = std::tuple_size_v<typename Cont::value_type>;
        size_t nRow = data_.size();
        mxArray* storage = mxCreateCellMatrix(static_cast<mwSize>(nRow), static_cast<mwSize>(N));
        for (mwIndex i = 0; auto&& item: data_)
        {
            mwIndex j = 0; // column index
            std::apply([&](auto&&... args) {(mxSetCell(storage, i + (j++)*nRow, ToMatlab(args)), ...); }, item);
            ++i; // next row
        }

        return storage;
    }

    // generic ToMatlab that converts provided data through type tag dispatch
    template <class T, class U>
    requires (!Container<T>)
    mxArray* ToMatlab(T val_, U)
    {
        return ToMatlab(static_cast<U>(val_));
    }


    //// struct of arrays
    // machinery to turn a container of objects into a single struct with an array per object field
    // default output is storage type corresponding to the type of the member variable accessed through this function, but it can be overridden through type tag dispatch (see getFieldWrapper implementation)
    template<typename Cont, typename... Fs>
    requires Container<Cont>
    mxArray* FieldToMatlab(const Cont& data_, bool rowVector_, Fs... fields)
    {
        mxArray* temp;
        using V = typename Cont::value_type;
        using U = decltype(nested_field::getWrapper(std::declval<V>(), fields...));
        mwSize rCount = static_cast<mwSize>(data_.size());
        mwSize cCount = 1;
        if (rowVector_)
            std::swap(rCount, cCount);

        if constexpr (typeNeedsMxCellStorage_v<U>)
        {
            // output cell array
            temp = mxCreateCellMatrix(rCount, cCount);
            mwIndex i = 0;
            if constexpr (!typeDumpVectorOneAtATime_v<V>)
            {
                for (auto&& item : data_)
                    mxSetCell(temp, i++, ToMatlab(nested_field::getWrapper(item, fields...)));
            }
            else
            {
                // iterate backward, remove item that was just converted to matlab
                i = static_cast<mwSize>(data_.size());
                for (auto rit = std::rbegin(data_); rit != std::rend(data_); )
                {
                    mxSetCell(temp, --i, ToMatlab(nested_field::getWrapper(*rit, fields...)));
                    rit = decltype(rit)(data_.erase(std::next(rit).base()));
                }
            }

        }
        else if constexpr (typeToMxClass_v<U> != mxSTRUCT_CLASS)
        {
            // output array
            auto storage = static_cast<U*>(mxGetData(temp = mxCreateUninitNumericMatrix(rCount, cCount, typeToMxClass_v<U>, mxREAL)));

            if (data_.size())
            {
                if constexpr (!typeDumpVectorOneAtATime_v<V>)
                {
                    for (auto&& item : data_)
                        (*storage++) = nested_field::getWrapper(item, fields...);
                }
                else
                {
                    // iterate backward, remove item that was just converted to matlab
                    storage += data_.size();
                    for (auto rit = std::rbegin(data_); rit != std::rend(data_); )
                    {
                        (*--storage) = nested_field::getWrapper(*rit, fields...);
                        rit = decltype(rit)(data_.erase(std::next(rit).base()));
                    }
                }
            }
        }
        else // NB: if constexpr (typeToMxClass_v<U> == mxSTRUCT_CLASS)
        {
            static_assert(always_false<Cont>, "Shouldn't happen, check you didn't override typeToMxClass for this type");   // or is this a TODO implement? Analyze situation when i encounter it
        }

        return temp;
    }
}