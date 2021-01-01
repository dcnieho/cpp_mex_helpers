#pragma once
#include <optional>
#include <string>
#include <tuple>
#include <string_view>
#include <functional>
#include <type_traits>
#include <algorithm>
#include <sstream>

#include "mex_type_utils_fwd.h"
#include "is_container_trait.h"
#include "is_specialization_trait.h"
#include "always_false.h"


namespace mxTypes
{
    namespace detail
    {
        std::string NumberToOrdinal(size_t number)
        {
            std::string suffix = "th";
            if (number % 100 < 11 || number % 100 > 13) {
                switch (number % 10) {
                case 1:
                    suffix = "st";
                    break;
                case 2:
                    suffix = "nd";
                    break;
                case 3:
                    suffix = "rd";
                    break;
                }
            }
            return std::to_string(number) + suffix;
        }

        template <typename OutputType, bool IsContainer>
        constexpr std::tuple<const char*, bool> buildCorrespondingMatlabTypeString_impl()
        {
            if constexpr (std::is_same_v<OutputType, std::string>)
                if constexpr (IsContainer)
                    return { "cellstring",true };
                else
                    return { "string",true };
            else
            {
                constexpr mxClassID mxClass = typeToMxClass_v<OutputType>;
                return { mxClassToString<mxClass>(),false };
            }
        }

        template <typename OutputType>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString_impl(std::string_view funcID_, size_t idx_, size_t offset_)
        {
            if constexpr (is_container_v<OutputType> && !std::is_same_v<OutputType, std::string>)
            {
                auto [typeStr, special] = buildCorrespondingMatlabTypeString_impl<typename OutputType::value_type, true>();
                return { typeStr, special, true };
            }
            else
            {
                auto [typeStr, special] = buildCorrespondingMatlabTypeString_impl<OutputType, false>();
                return { typeStr, special, false };
            }
        }

        // if converter provided, for scalar output
        template <typename T, typename Arg>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString(std::string_view funcID_, size_t idx_, size_t offset_, T(*conv_)(Arg))
        {
            return buildCorrespondingMatlabTypeString_impl<std::decay_t<Arg>>(funcID_, idx_, offset_);
        }
        template <typename T, typename Arg>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString(std::string_view funcID_, size_t idx_, size_t offset_, std::function<T(Arg)> conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<std::decay_t<Arg>>(funcID_, idx_, offset_);
        }
        // if converter provided, for container output
        template <typename T, typename Arg>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString(std::string_view funcID_, size_t idx_, size_t offset_, typename T::value_type(*conv_)(Arg))
        {
            return buildCorrespondingMatlabTypeString_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>(funcID_, idx_, offset_);
        }
        template <typename T, typename Arg>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString(std::string_view funcID_, size_t idx_, size_t offset_, std::function<typename T::value_type(Arg)> conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>(funcID_, idx_, offset_);
        }
        // if no converter provided by user
        template <typename T>
        constexpr std::tuple<const char*, bool, bool> buildCorrespondingMatlabTypeString(std::string_view funcID_, size_t idx_, size_t offset_, nullptr_t conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<T>(funcID_, idx_, offset_);
        }

        template <typename T, typename Converter>
        void buildAndThrowError(std::string_view funcID_, size_t idx_, size_t offset_, bool isOptional_, Converter conv_)
        {
            auto [typeStr, special, isContainer] = buildCorrespondingMatlabTypeString<T>(funcID_, idx_, offset_, conv_);
            std::stringstream os;
            os << "SWAG::" << funcID_ << ": ";
            if (isOptional_)
                os << "Optional ";
            auto ordinal = idx_ - offset_ + 1;
            os << NumberToOrdinal(ordinal) << " argument must be a";
            if (typeStr[0] == 'a' || typeStr[0] == 'e' || typeStr[0] == 'i' || typeStr[0] == 'o' || typeStr[0] == 'u')
                os << "n";
            os << " " << typeStr;
            if (!special)
            {
                if (isContainer)
                    os << " array" << typeStr;
                else
                    os << " scalar" << typeStr;
            }
            os << ".";
            throw os.str();
        }


        template <typename T>
        bool checkInput_impl_cell(const mxArray* inp_)
        {
            if (!mxIsCell(inp_))
                return false;

            // recurse to check each contained element
            const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
            for (mwIndex i = 0; i < nElem; i++)
                if (!checkInput_impl<T>(mxGetCell(inp_, i)))
                    return false;

            return true;
        }
        template <typename T>
        bool checkInput_impl(const mxArray* inp_)
        {
            // early out for complex arguments, never wanted by us
            if (mxIsComplex(inp_))
                return false;

            // NB: below checks if mxArray contains exactly the expected type,
            // it does not check whether type could be acquired losslessly through a cast
            if constexpr (std::is_same_v<T, std::string>)
                return mxIsChar(inp_);
            else if constexpr (is_container_v<T>)
            {
                if constexpr (typeNeedsMxCellStorage_v<T::value_type>)
                {
                    return checkInput_impl_cell<T::value_type>(inp_);
                }
                else
                {
                    if (mxIsCell(inp_))
                        return checkInput_impl_cell<T::value_type>(inp_);
                    else
                        return mxGetClassID(inp_)==typeToMxClass_v<T::value_type>;
                }
            }
            else
                return mxGetClassID(inp_)==typeToMxClass_v<T> && mxIsScalar(inp_);
        }

        template <typename T, typename T2>
        bool checkInput(const mxArray*, T2)
        {
            static_assert(always_false<T>, "Invalid converter provided, make sure your converter takes the matlab-side type as input, and has an return type matching the type you requested of FromMatlab");
        }
        // if converter provided, for scalar output
        template <typename T, typename Arg>
        bool checkInput(const mxArray* inp_, T(*conv_)(Arg))
        {
            return checkInput_impl<std::decay_t<Arg>>(inp_);
        }
        template <typename T, typename Arg>
        bool checkInput(const mxArray* inp_, std::function<T(Arg)> conv_)
        {
            return checkInput_impl<std::decay_t<Arg>>(inp_);
        }
        // if converter provided, for container output
        template <typename T, typename Arg>
        bool checkInput(const mxArray* inp_, typename T::value_type(*conv_)(Arg))
        {
            return checkInput_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>(inp_);
        }
        template <typename T, typename Arg>
        bool checkInput(const mxArray* inp_, std::function<typename T::value_type(Arg)> conv_)
        {
            return checkInput_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>(inp_);
        }
        // if no converter provided by user
        template <typename T>
        bool checkInput(const mxArray* inp_, nullptr_t)
        {
            return checkInput_impl<T>(inp_);
        }



        template <typename T>
        T getValue_impl(const mxArray* inp_)
        {
            if constexpr (std::is_same_v<T, std::string>)
            {
                char* str = mxArrayToString(inp_);
                T out = str;
                mxFree(str);
                return out;
            }
            else if constexpr (is_container_v<T>)
            {
                if (typeNeedsMxCellStorage_v<T::value_type> || mxIsCell(inp_))
                {
                    const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                    T out;
                    out.reserve(nElem);
                    for (mwIndex i = 0; i < nElem; i++)
                        out.emplace_back(getValue_impl<T::value_type>(mxGetCell(inp_, i)));
                    return out;
                }
                else
                {
                    auto data = static_cast<typename T::value_type*>(mxGetData(inp_));
                    auto numel = mxGetNumberOfElements(inp_);
                    return T(data, data + numel);
                }
            }
            else
                return *static_cast<T*>(mxGetData(inp_));
        }

        template <typename OutType, typename MatType, typename Converter>
        OutType getValue_impl(const mxArray* inp_, Converter conv_)
        {
            if constexpr (is_container_v<OutType>)
            {
                OutType out;
                if constexpr (typeNeedsMxCellStorage_v<OutType::value_type>)
                {
                    // recurse to get each contained element
                    const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                    out.reserve(nElem);
                    for (mwIndex i = 0; i < nElem; i++)
                        // get each element using non-converter getValue_impl, then invoke converter on it
                        out.emplace_back(std::invoke(conv_,getValue_impl<MatType>(mxGetCell(inp_, i))));
                }
                else
                {
                    auto data = static_cast<MatType*>(mxGetData(inp_));
                    auto numel = mxGetNumberOfElements(inp_);
                    std::transform(data, data + numel, std::back_inserter(out), conv_);
                }
                return out;
            }
            else
                // single value, use non-converter getValue_impl to get it, then invoke converter on it
                return std::invoke(conv_,getValue_impl<MatType>(inp_));
        }

        template <typename T, typename T2>
        T getValue(const mxArray*, T2)
        {
            static_assert(always_false<T>, "Invalid converter provided, make sure your converter takes the matlab-side type as input, and has an return type matching the type you requested of FromMatlab");
        }
        // if converter provided, for scalar output
        template <typename T, typename Arg>
        T getValue(const mxArray* inp_, T(*conv_)(Arg))
        {
            return getValue_impl<T, std::decay_t<Arg>>(inp_, conv_);
        }
        template <typename T, typename Arg>
        T getValue(const mxArray* inp_, std::function<T(Arg)> conv_)
        {
            return getValue_impl<T, std::decay_t<Arg>>(inp_, conv_);
        }
        // if converter provided, for container output
        template <typename T, typename Arg>
        T getValue(const mxArray* inp_, typename T::value_type(*conv_)(Arg))
        {
            return getValue_impl<T, std::decay_t<Arg>>(inp_, conv_);
        }
        template <typename T, typename Arg>
        T getValue(const mxArray* inp_, std::function<typename T::value_type(Arg)> conv_)
        {
            return getValue_impl<T, std::decay_t<Arg>>(inp_, conv_);
        }
        // if no converter provided by user
        template <typename T>
        T getValue(const mxArray* inp_, nullptr_t conv_)
        {
            return getValue_impl<T>(inp_);
        }
    }

    // for optional input arguments, use std::optional<T> as return type
    template <typename OutputType, typename Converter = nullptr_t>
    typename std::enable_if_t<is_specialization_v<OutputType, std::optional>, OutputType>
        FromMatlab(int nrhs, const mxArray* prhs[], size_t idx_, std::string_view funcID_, size_t offset_, Converter conv_ = nullptr)
    {
        using ValueType = typename OutputType::value_type;

        // check element exists and is not empty
        if (idx_>=nrhs || mxIsEmpty(prhs[idx_]))
            return std::nullopt;

        // see if element passes checks. If not, thats an error for an optional value
        auto inp = prhs[idx_];
        if (!detail::checkInput<ValueType>(inp, conv_))
            detail::buildAndThrowError<ValueType>(funcID_, idx_, offset_, true, conv_);

        return detail::getValue<ValueType>(inp, conv_);
    }

    // for required arguments
    template <typename OutputType, typename Converter = nullptr_t>
    typename std::enable_if_t<!is_specialization_v<OutputType, std::optional>, OutputType>
        FromMatlab(int nrhs, const mxArray* prhs[], size_t idx_, std::string_view funcID_, size_t offset_, Converter conv_ = nullptr)
    {
        // check element exists, is not empty and passes checks
        if (idx_>=nrhs || mxIsEmpty(prhs[idx_]) || !detail::checkInput<OutputType>(prhs[idx_], conv_))
            detail::buildAndThrowError<OutputType>(funcID_, idx_, offset_, false, conv_);

        return detail::getValue<OutputType>(prhs[idx_], conv_);
    }
}