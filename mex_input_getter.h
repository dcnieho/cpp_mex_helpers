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
#include "invocable_traits.h"


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
            if constexpr (Container<OutputType> && !std::is_same_v<OutputType, std::string>)
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

        template <typename OutputType, typename Converter>
        constexpr std::string buildCorrespondingMatlabTypeString()
        {
            if constexpr (!std::is_same_v<Converter, std::nullptr_t>)
            {
                using ConverterInputType = std::decay_t<invocable_traits::get<Converter>::template arg_t<0>>;
                if constexpr (Container<OutputType>)
                {
                    using InputContainerType = replace_specialization_type_t<OutputType, ConverterInputType>;
                    return buildCorrespondingMatlabTypeString_impl<InputContainerType>();
                }
                else
                    return buildCorrespondingMatlabTypeString_impl<ConverterInputType>();
            }
            else
                return buildCorrespondingMatlabTypeString_impl<OutputType>();
        }

        template <typename OutputType, typename Converter>
        void buildAndThrowError(std::string_view funcID_, size_t idx_, size_t offset_, int nrhs_, const mxArray* prhs_[], bool isOptional_, Converter conv_)
        {
            auto typeStr = buildCorrespondingMatlabTypeString<OutputType, Converter>();
            std::string out;
            out.reserve(100);
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
            if constexpr (std::is_arithmetic_v<OutputType>)
                special = false;
            else if constexpr (Container<OutputType> && !std::is_same_v<OutputType, std::string>)
            {
                if constexpr (std::is_arithmetic_v<typename OutputType::value_type>)
                    special = false;
            }
            if (!special)
            {
                if constexpr (Container<OutputType>)
                    out += " array";
                else
                    out += " scalar";
            }
            out += ". ";

            // now say what the argument instead contained (and some special cases like
            // not enough arguments provided or empty argument)
            if (idx_ >= nrhs_)
                out += "Not enough input arguments. Only " + std::to_string(nrhs_ - offset_) + " were provided.";
            else if(mxIsEmpty(prhs_[idx_]))
                out += "The provided input argument was empty.";
            else
            {
                out += "The provided input argument was a ";
                if (mxIsSparse(prhs_[idx_]))
                    out += "sparse ";
                if (mxIsComplex(prhs_[idx_]))
                    out += "complex ";
                auto numDim = mxGetNumberOfDimensions(prhs_[idx_]);
                auto dims   = mxGetDimensions(prhs_[idx_]);
                for (mwSize i = 0; i < numDim; ++i)
                {
                    out += std::to_string(dims[i]);
                    if (i < numDim - 1)
                        out += "x";
                    else
                        out += " ";
                }

                out += mxGetClassName(prhs_[idx_]);
                out += ".";
            }
            throw out;
        }


        // forward declaration
        template <typename OutputType, typename Converter>
        bool checkInput(const mxArray* inp_, Converter conv_);
        // end forward declarations


        template <template <class...> class TP, class... Args, size_t... Is>
        bool checkInput_tuple(const mxArray* inp_, TP<Args...>&&, std::index_sequence<Is...>, mwIndex iRow_ = 0, mwSize nRow_ = 1)
        {
            return (checkInput<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_), nullptr) && ...);
        }

        template <typename OutputType>
        bool checkInput_impl_cell(const mxArray* inp_)
        {
            if (!mxIsCell(inp_))
                return false;

            // recurse to check each contained element
            const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
            for (mwIndex i = 0; i < nElem; i++)
                if (!checkInput<OutputType>(mxGetCell(inp_, i), nullptr))
                    return false;

            return true;
        }

        template <typename OutputType, typename Converter>
        bool checkInput(const mxArray* inp_, Converter conv_)
        {
            if constexpr (!std::is_same_v<Converter, std::nullptr_t>)
            {
                // check for input data type of converter
                using ConverterInputType = std::decay_t<invocable_traits::get<Converter>::template arg_t<0>>;
                if constexpr (Container<OutputType>)
                {
                    using InputContainerType = replace_specialization_type_t<OutputType, ConverterInputType>;
                    return checkInput<InputContainerType>(inp_, nullptr);
                }
                else
                    return checkInput<ConverterInputType>(inp_, nullptr);
            }
            else
            {
                // early out for complex or sparse arguments, never wanted by us
                if (mxIsComplex(inp_) || mxIsSparse(inp_))
                    return false;

                if constexpr (Container<OutputType>)
                {
                    if constexpr (
                        is_specialization_v<typename OutputType::value_type, std::pair> ||
                        is_specialization_v<typename OutputType::value_type, std::tuple>)
                    {
                        using theTuple = typename OutputType::value_type;

                        if (!mxIsCell(inp_))
                            return false;

                        // get info about input
                        auto nRow = mxGetM(inp_);
                        auto nCol = mxGetN(inp_);
                        if (nCol != std::tuple_size_v<theTuple>)
                            return false;

                        // per row, check each cell
                        for (mwIndex iRow = 0; iRow < nRow; ++iRow)
                            if (!checkInput_tuple(inp_, theTuple(), std::make_index_sequence<std::tuple_size_v<theTuple>>{}, iRow, nRow))
                                return false;

                        return true;
                    }
                    else if constexpr (std::is_same_v<OutputType, std::string>)
                        return mxIsChar(inp_);
                    else
                    {
                        if constexpr (typeNeedsMxCellStorage_v<OutputType::value_type>)
                            return checkInput_impl_cell<OutputType::value_type>(inp_);
                        else
                        {
                            if (mxIsCell(inp_))
                                return checkInput_impl_cell<OutputType::value_type>(inp_);
                            else
                                return mxGetClassID(inp_) == typeToMxClass_v<OutputType::value_type>;
                        }
                    }
                }
                else
                {
                    // NB: below checks if mxArray contains exactly the expected type,
                    // it does not check whether type could be acquired losslessly through a cast
                    if constexpr (is_specialization_v<OutputType, std::pair> || is_specialization_v<OutputType, std::tuple>)
                        return checkInput_tuple(inp_, OutputType(), std::make_index_sequence<std::tuple_size_v<OutputType>>{});
                    else
                        return mxGetClassID(inp_) == typeToMxClass_v<OutputType> && mxIsScalar(inp_);
                }
            }
        }



        // forward declarations
        template <typename OutputType, typename Converter>
        OutputType getValue(const mxArray* inp_, Converter conv_);
        // end forward declarations

        template <template <class...> class TP, class... Args, size_t... Is>
        TP<Args...> getValue_tuple(const mxArray* inp_, TP<Args...>&&, std::index_sequence<Is...>, mwIndex iRow_ = 0, mwSize nRow_ = 1)
        {
            if constexpr (is_specialization_v<TP<Args...>, std::tuple>)
                return std::make_tuple(getValue<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_), nullptr) ...);
            else
                return std::make_pair(getValue<Args>(mxGetCell(inp_, iRow_ + (Is)*nRow_), nullptr) ...);
        }

        template <typename OutputType, typename Converter>
        OutputType getValue(const mxArray* inp_, Converter conv_)
        {
            if constexpr (!std::is_same_v<Converter, std::nullptr_t>)
            {
                // apply converter function
                using ConverterInputType = std::decay_t<invocable_traits::get<Converter>::template arg_t<0>>;
                if constexpr (is_specialization_v<ConverterInputType, std::basic_string_view>)
                    // if a string_view is the input to the converter function, get a
                    // temporary std::string instead, else we have a lifetime issue.
                    return std::invoke(conv_, getValue<std::string>(inp_, nullptr));
                else if constexpr (Container<OutputType>)
                {
                    OutputType out;
                    if (mxIsCell(inp_))
                    {
                        // recurse to get each contained element
                        const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                        out.reserve(nElem);
                        for (mwIndex i = 0; i < nElem; i++)
                            // get each element using non-converter getValue, then invoke converter on it
                            out.emplace_back(std::invoke(conv_, getValue<ConverterInputType>(mxGetCell(inp_, i)), nullptr));
                    }
                    else
                    {
                        auto data = static_cast<ConverterInputType*>(mxGetData(inp_));
                        auto numel = mxGetNumberOfElements(inp_);
                        std::transform(data, data + numel, std::back_inserter(out), conv_);
                    }
                    return out;
                }
                else
                    // single value, use non-converter getValue to get it, then invoke converter on it
                    return std::invoke(conv_, getValue<ConverterInputType>(inp_, nullptr));
            }
            else
            {
                // copy over data without converter function
                if constexpr (Container<OutputType>)
                {
                    if constexpr (
                        is_specialization_v<typename OutputType::value_type, std::pair> ||
                        is_specialization_v<typename OutputType::value_type, std::tuple>)
                    {
                        // get info about input (we've already checked that number of column is equal to tuple length)
                        auto nRow = mxGetM(inp_);

                        // per row, convert from cell
                        OutputType out;
                        using theTuple = typename OutputType::value_type;
                        for (mwIndex iRow = 0; iRow < nRow; ++iRow)
                            out.push_back(getValue_tuple(inp_, theTuple(), std::make_index_sequence<std::tuple_size_v<theTuple>>{}, iRow, nRow));

                        return out;
                    }
                    else
                    {
                        static_assert(!is_specialization_v<OutputType, std::basic_string_view>, "Can't return a string view, would be dangling");
                        if constexpr (std::is_same_v<OutputType, std::string>)
                        {
                            char* str = mxArrayToString(inp_);
                            OutputType out = str;
                            mxFree(str);
                            return out;
                        }
                        else
                        {
                            if (mxIsCell(inp_))
                            {
                                const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                                OutputType out;
                                for (mwIndex i = 0; i < nElem; i++)
                                    out.emplace_back(getValue<OutputType::value_type>(mxGetCell(inp_, i), nullptr));
                                return out;
                            }
                            else
                            {
                                auto data = static_cast<typename OutputType::value_type*>(mxGetData(inp_));
                                auto numel = mxGetNumberOfElements(inp_);
                                return OutputType(data, data + numel);
                            }
                        }
                    }
                }
                else
                {
                    if constexpr (is_specialization_v<OutputType, std::pair> || is_specialization_v<OutputType, std::tuple>)
                        return getValue_tuple(inp_, OutputType(), std::make_index_sequence<std::tuple_size_v<OutputType>>{});
                    else
                        return *static_cast<OutputType*>(mxGetData(inp_));
                }
            }
        }
    }

    // returns T of std::optional<T> if std::optional, else just returns provided type
    template <typename T>
    struct unwrapOptional
    {
        using type = T;
    };

    template <typename T>
    requires is_specialization_v<T, std::optional>
    struct unwrapOptional<T>
    {
        using type = T::value_type;
    };

    // for optional input arguments, use std::optional<T> as return type,
    // for required arguments just use any other T
    template <typename OutputType, typename Converter = std::nullptr_t>
    OutputType FromMatlab(int nrhs, const mxArray* prhs[], size_t idx_, std::string_view funcID_, size_t offset_, Converter conv_ = nullptr)
    {
        // unwrap std::optional to get at desired type
        bool constexpr outputIsOptional = is_specialization_v<OutputType, std::optional>;
        using UnwrappedOutputType = unwrapOptional<OutputType>::type;

        // check converter, if provided
        if constexpr (!std::is_same_v<Converter, std::nullptr_t>)
        {
            using traits = invocable_traits::get<Converter>;
            static_assert(traits::arity == 1, "A conversion function, if provided, must be unary.");
            static_assert(std::is_convertible_v<traits::invoke_result_t, UnwrappedOutputType>, "The conversion function's result type cannot be converted to the requested output type.");
        }

        // check element exists and is not empty
        bool haveElement = idx_ < nrhs && !mxIsEmpty(prhs[idx_]);
        if constexpr (outputIsOptional)
            if (!haveElement)
                return std::nullopt;

        // see if element passes checks. If not, thats an error for an optional value
        auto inp = prhs[idx_];
        if (!haveElement || !detail::checkInput<UnwrappedOutputType>(inp, conv_))
            detail::buildAndThrowError<UnwrappedOutputType>(funcID_, idx_, offset_, nrhs, prhs, outputIsOptional, conv_);

        return detail::getValue<UnwrappedOutputType>(inp, conv_);
    }
}