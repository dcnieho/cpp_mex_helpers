#pragma once
#include <optional>
#include <string>

#include "mex_type_utils_fwd.h"
#include "is_container_trait.h"
#include "is_specialization_trait.h"


namespace mxTypes
{
    namespace detail
    {
        template <typename T>
        bool checkInput(const mxArray* inp_)
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
                    if (!mxIsCell(inp_))
                        return false;

                    // recurse to check each contained element
                    const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                    for (mwIndex i = 0; i < nElem; i++)
                        if (!checkInput<T::value_type>(mxGetCell(inp_, i)))
                            return false;

                    return true;
                }
                else
                    return mxGetClassID(inp_)==typeToMxClass_v<T::value_type>;
            }
            else
                return mxGetClassID(inp_)==typeToMxClass_v<T            > && mxIsScalar(inp_);
        }

        template <typename T>
        T getValue(const mxArray* inp_)
        {
            if constexpr (std::is_same_v<T, std::string>)
            {
                char* str = mxArrayToString(inp_);
                T out     = str;
                mxFree(str);
                return out;
            }
            else if constexpr (is_container_v<T>)
            {
                if constexpr (typeNeedsMxCellStorage_v<T::value_type>)
                {
                    // recurse to get each contained element
                    const auto nElem = static_cast<mwIndex>(mxGetNumberOfElements(inp_));
                    T out;
                    out.reserve(nElem);
                    for (mwIndex i=0; i<nElem; i++)
                        out.emplace_back(getValue<T::value_type>(mxGetCell(inp_, i)));
                    return out;
                }
                else
                {
                    auto data  = static_cast<typename T::value_type*>(mxGetData(inp_));
                    auto numel = mxGetNumberOfElements(inp_);
                    return T(data, data+numel);
                }
            }
            else
                return *static_cast<T*>(mxGetData(inp_));
        }
    }

    // for optional input arguments, use std::optional<T> as return type
    template <typename OutputType>
    typename std::enable_if_t<is_specialization_v<OutputType, std::optional>, OutputType>
        FromMatlab(int nrhs, const mxArray* prhs[], size_t idx_, const char* errorStr_)
    {
        using ValueType = typename OutputType::value_type;

        // check element exists and is not empty
        if (idx_>=nrhs || mxIsEmpty(prhs[idx_]))
            return std::nullopt;

        // see if element passes checks. If not, thats an error for an optional value
        auto inp = prhs[idx_];
        if (!detail::checkInput<ValueType>(inp))
            throw errorStr_;

        return detail::getValue<ValueType>(inp);
    }

    // for required arguments
    template <typename OutputType>
    typename std::enable_if_t<!is_specialization_v<OutputType, std::optional>, OutputType>
        FromMatlab(int nrhs, const mxArray* prhs[], size_t idx_, const char* errorStr_)
    {
        // check element exists, is not empty and passes checks
        if (idx_>=nrhs || mxIsEmpty(prhs[idx_]) || !detail::checkInput<OutputType>(prhs[idx_]))
            throw errorStr_;

        return detail::getValue<OutputType>(prhs[idx_]);
    }
}