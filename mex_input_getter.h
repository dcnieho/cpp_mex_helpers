#pragma once
#include <optional>
#include <string>
#include <utility>
#include <tuple>
#include <string_view>
#include <functional>
#include <type_traits>
#include <algorithm>

#include "mex_type_utils_fwd.h"
#include "is_container_trait.h"
#include "is_specialization_trait.h"
#include "replace_specialization_type.h"
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

        // forward declaration
        template <typename OutputType>
        constexpr std::string buildCorrespondingMatlabTypeString_impl();
        // end forward declaration
        template <typename OutputType, bool IsContainer>
        constexpr std::string buildCorrespondingMatlabTypeString_impl()
        {
            if constexpr (std::is_same_v<OutputType, std::string>)
            {
                if constexpr (IsContainer)
                    return "cellstring";
                else
                    return "string";
            }
            else
            {
                constexpr mxClassID mxClass = typeToMxClass_v<OutputType>;
                return mxClassToString<mxClass>();
            }
        }
        template <bool IsContainer, template <class...> class TP, class... Args, size_t... Is>
        constexpr std::string buildCorrespondingMatlabTypeString_impl(TP<Args...>&&, std::index_sequence<Is...>)
        {
            std::string outStr;
            outStr.reserve(64);
            if constexpr (IsContainer)
                outStr = "M";
            else
                outStr = "1";
            outStr += "x" + std::to_string(sizeof...(Args)) + " cell array ";
            if constexpr (IsContainer)
                outStr += "with each row containing ";
            else
                outStr += "of types ";
            outStr += "{";

            std::size_t length = sizeof...(Args);
            ((outStr += buildCorrespondingMatlabTypeString_impl<Args>() + (Is == length-1 ? "" : ", ")),...);
            outStr += "}";

            return outStr;
        }

        template <typename OutputType>
        constexpr std::string buildCorrespondingMatlabTypeString_impl()
        {
            if constexpr (is_container_v<OutputType> && !std::is_same_v<OutputType, std::string>)
            {
                if constexpr (is_specialization_v<typename OutputType::value_type, std::tuple> || is_specialization_v<typename OutputType::value_type, std::pair>)
                {
                    using theTuple = typename OutputType::value_type;
                    return buildCorrespondingMatlabTypeString_impl<true>(theTuple(), std::make_index_sequence<std::tuple_size_v<theTuple>>{});
                }
                else
                    return buildCorrespondingMatlabTypeString_impl<typename OutputType::value_type, true>();
            }
            else
            {
                if constexpr (is_specialization_v<OutputType, std::tuple> || is_specialization_v<OutputType, std::pair>)
                    return buildCorrespondingMatlabTypeString_impl<false>(OutputType(), std::make_index_sequence<std::tuple_size_v<OutputType>>{});
                else
                    return buildCorrespondingMatlabTypeString_impl<OutputType, false>();
            }
        }

        // if converter provided, for scalar output
        template <typename T, typename Arg>
        constexpr std::string buildCorrespondingMatlabTypeString(T(*conv_)(Arg))
        {
            return buildCorrespondingMatlabTypeString_impl<std::decay_t<Arg>>();
        }
        template <typename T, typename Arg>
        constexpr std::string buildCorrespondingMatlabTypeString(std::function<T(Arg)> conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<std::decay_t<Arg>>();
        }
        // if converter provided, for container output
        template <typename T, typename Arg>
        constexpr std::string buildCorrespondingMatlabTypeString(typename T::value_type(*conv_)(Arg))
        {
            return buildCorrespondingMatlabTypeString_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>();
        }
        template <typename T, typename Arg>
        constexpr std::string buildCorrespondingMatlabTypeString(std::function<typename T::value_type(Arg)> conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<replace_specialization_type_t<std::vector<T::value_type>, std::decay_t<Arg>>>();
        }
        // if no converter provided by user
        template <typename T>
        constexpr std::string buildCorrespondingMatlabTypeString(nullptr_t conv_)
        {
            return buildCorrespondingMatlabTypeString_impl<T>();
        }

        template <typename T, typename Converter>
        void buildAndThrowError(std::string_view funcID_, size_t idx_, size_t offset_, bool isOptional_, Converter conv_)
        {
            auto typeStr = buildCorrespondingMatlabTypeString<T>(conv_);
            std::string out;
            out.reserve(50);
            out += "SWAG::";
            if (!funcID_.empty())
            {
                out += funcID_;
                out += ": ";
            }
            if (isOptional_)
                out += "Optional ";
            auto ordinal = idx_ - offset_ + 1;
            out += NumberToOrdinal(ordinal) + " argument must be a";
            if (typeStr[0] == 'a' || typeStr[0] == 'e' || typeStr[0] == 'i' || typeStr[0] == 'o' || typeStr[0] == 'u')
                out += "n";
            out += " " + typeStr;

            // if simple type (e.g. int) or container of simple type (e.g. std::vector<int>),
            // automatically add "scalar" or "array" to the string
            bool special = true;
            if constexpr (std::is_arithmetic_v<T>)
                special = false;
            else if constexpr (is_container_v<T> && !std::is_same_v<T, std::string>)
            {
                if constexpr (std::is_arithmetic_v<typename T::value_type>)
                    special = false;
            }
            if (!special)
            {
                if constexpr (is_container_v<T>)
                    out += " array";
                else
                    out += " scalar";
            }
            out += ".";
            throw out;
        }


        // forward declaration
        template <typename T>
        typename std::enable_if_t<!is_container_v<T>, bool>
            checkInput_impl(const mxArray* inp_);
        template <typename Cont>
        typename std::enable_if_t<is_container_v<Cont> &&
            !is_specialization_v<typename Cont::value_type, std::pair> &&
            !is_specialization_v<typename Cont::value_type, std::tuple>,
            bool>
            checkInput_impl(const mxArray* inp_);
        template<class Cont>
        typename std::enable_if_t<
            is_container_v<Cont> && (
                is_specialization_v<typename Cont::value_type, std::pair> ||
                is_specialization_v<typename Cont::value_type, std::tuple>),
            bool>
            checkInput_impl(const mxArray* inp_);
        // end forward declaration
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

        template <template <class...> class TP, class... Args, size_t... Is>
        bool checkInput_impl(const mxArray* inp_, TP<Args...>&&, std::index_sequence<Is...>, mwIndex iRow_ = 0, mwSize nRow_ = 1)
        {
            if (!mxIsCell(inp_))
                return false;

            return (checkInput_impl<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_))&& ...);
        }
        template<class Cont>
        typename std::enable_if_t<
            is_container_v<Cont> && (
                is_specialization_v<typename Cont::value_type, std::pair> ||
                is_specialization_v<typename Cont::value_type, std::tuple>),
            bool>
            checkInput_impl(const mxArray* inp_)
        {
            using theTuple = typename Cont::value_type;

            // get info about input
            auto nRow = mxGetM(inp_);
            auto nCol = mxGetN(inp_);
            if (nCol != std::tuple_size_v<theTuple>)
                return false;

            // per row, check each cell
            for (mwIndex iRow = 0; iRow < nRow; ++iRow)
                if (!checkInput_impl(inp_, theTuple(), std::make_index_sequence<std::tuple_size_v<theTuple>>{}, iRow, nRow))
                    return false;

            return true;
        }
        template <typename Cont>
        typename std::enable_if_t<is_container_v<Cont> &&
            !is_specialization_v<typename Cont::value_type, std::pair> &&
            !is_specialization_v<typename Cont::value_type, std::tuple>,
            bool>
            checkInput_impl(const mxArray* inp_)
        {
            // early out for complex or sparse arguments, never wanted by us
            if (mxIsComplex(inp_) || mxIsSparse(inp_))
                return false;

            if constexpr (std::is_same_v<Cont, std::string>)
                return mxIsChar(inp_);
            else
            {
                if (typeNeedsMxCellStorage_v<Cont::value_type> || mxIsCell(inp_))
                    return checkInput_impl_cell<Cont::value_type>(inp_);
                else
                    return checkInput_impl<Cont::value_type>(inp_);
            }
        }
        template <typename T>
        typename std::enable_if_t<!is_container_v<T>, bool>
            checkInput_impl(const mxArray* inp_)
        {
            // early out for complex or sparse arguments, never wanted by us
            if (mxIsComplex(inp_) || mxIsSparse(inp_))
                return false;

            // NB: below checks if mxArray contains exactly the expected type,
            // it does not check whether type could be acquired losslessly through a cast
            if constexpr (is_specialization_v<T, std::pair> || is_specialization_v<T, std::tuple>)
                return checkInput_impl(inp_, T(), std::make_index_sequence<std::tuple_size_v<T>>{});
            else
                return mxGetClassID(inp_) == typeToMxClass_v<T> && mxIsScalar(inp_);
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


        // forward declarations
        template <typename Cont>
        typename std::enable_if_t<is_container_v<Cont> &&
            !is_specialization_v<typename Cont::value_type, std::pair> &&
            !is_specialization_v<typename Cont::value_type, std::tuple>,
            Cont>
            getValue_impl(const mxArray* inp_);
        template <typename T>
        typename std::enable_if_t<!is_container_v<T>, T>
            getValue_impl(const mxArray* inp_);
        // end forward declarations

        template <template <class...> class TP, class... Args, size_t... Is>
        TP<Args...> getValue_impl(const mxArray* inp_, TP<Args...>&&, std::index_sequence<Is...>, mwIndex iRow_ = 0, mwSize nRow_ = 1)
        {
            if constexpr (is_specialization_v<TP<Args...>, std::tuple>)
                return std::make_tuple(getValue_impl<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_)) ...);
            else
                return std::make_pair(getValue_impl<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_)) ...);
        }
        template<class Cont>
        typename std::enable_if_t<
                is_container_v<Cont> && (
                    is_specialization_v<typename Cont::value_type, std::pair> ||
                    is_specialization_v<typename Cont::value_type, std::tuple>),
            Cont>
            getValue_impl(const mxArray* inp_)
        {
            // get info about input (we've already checked that number of column is equal to tuple length)
            auto nRow = mxGetM(inp_);

            // per row, convert from cell
            Cont out;
            using theTuple = typename Cont::value_type;
            for (mwIndex iRow = 0; iRow < nRow; ++iRow)
                out.push_back(getValue_impl(inp_, theTuple(), std::make_index_sequence<std::tuple_size_v<theTuple>>{}, iRow, nRow));

            return out;
        }

        template <typename Cont>
        typename std::enable_if_t<is_container_v<Cont> && 
                !is_specialization_v<typename Cont::value_type, std::pair> &&
                !is_specialization_v<typename Cont::value_type, std::tuple>,
            Cont>
            getValue_impl(const mxArray* inp_)
        {
            if constexpr (std::is_same_v<Cont, std::string>)
            {
                char* str = mxArrayToString(inp_);
                Cont out = str;
                mxFree(str);
                return out;
            }
            else
            {
                if (typeNeedsMxCellStorage_v<Cont::value_type> || mxIsCell(inp_))
                {
                    const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                    Cont out;
                    for (mwIndex i = 0; i < nElem; i++)
                        out.emplace_back(getValue_impl<Cont::value_type>(mxGetCell(inp_, i)));
                    return out;
                }
                else
                {
                    auto data = static_cast<typename Cont::value_type*>(mxGetData(inp_));
                    auto numel = mxGetNumberOfElements(inp_);
                    return Cont(data, data + numel);
                }
            }
        }

        template <typename T>
        typename std::enable_if_t<!is_container_v<T>, T>
            getValue_impl(const mxArray* inp_)
        {
            if constexpr (is_specialization_v<T, std::pair> || is_specialization_v<T, std::tuple>)
                return getValue_impl(inp_, T(), std::make_index_sequence<std::tuple_size_v<T>>{});
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