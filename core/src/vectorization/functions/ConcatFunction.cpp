/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Concat function implementation
 */

#include "ConcatFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "type/string_Impl.h"
#include <algorithm>
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void ConcatFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("Concat function Error", "Concat requires at least 2 arguments");
    }

    // Pop exactly 2 arguments from stack (as Gluten's UnfoldConcatStringFunc always creates
    // nested 2-argument concat calls for multi-argument concat)
    // In ExprEval, arguments are pushed in order: first arg (left) pushed first, second arg (right) pushed last
    // So stack order: top = arg2 (right), next = arg1 (left)
    // For concat(str1, str2), we want to concatenate str1 + str2
    std::vector<BaseVector *> argVectors;

    // Pop arg2 (right argument) - top of stack
    BaseVector *arg2 = args.top();
    args.pop();

    // Pop arg1 (left argument) - next on stack
    BaseVector *arg1 = args.top();
    args.pop();

    // Order: arg1 first, then arg2 for correct concatenation order (left + right)
    argVectors.push_back(arg1);
    argVectors.push_back(arg2);

    ApplyConcat(argVectors, result, outputType, context);

    delete arg1;
    delete arg2;
}

void ConcatFunction::ApplyConcat(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
    const DataTypePtr &outputType, ExecutionContext *context) const
{
    // Get size from context (handles constant vectors correctly)
    int32_t size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);

    // Process each row
    for (int32_t row = 0; row < size; ++row) {
        bool hasNull = false;
        for (auto *argVec : argVectors) {
            int32_t nullCheckIdx = (argVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
            if (argVec->IsNull(nullCheckIdx)) {
                hasNull = true;
                break;
            }
        }

        if (hasNull) {
            result->SetNull(row);
            continue;
        }

        // Concatenate all string values
        std::string concatenated;
        for (auto *argVec : argVectors) {
            std::string_view str = GetStringValueFromVector(argVec, row);
            concatenated.append(str.data(), str.size());
        }

        // Set the concatenated result
        std::string_view resultView(concatenated);
        SetStringValueToVector(result, row, const_cast<std::string_view &>(resultView));
    }
}

std::string_view ConcatFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();

    // StringView (16B) input (SV-in / VARCHAR-out): return a view over the referenced StringView stored
    // in the vector buffer via GetValueRef / GetConstValueRef, so inline (<=12B) data does not dangle
    // (a temporary StringView's inline bytes would). The caller (ApplyConcat) copies immediately via
    // append. Type-aware, so any SV/VARCHAR mix of arguments is handled. Mirrors Like.cpp's SV reader.
    if (vec->GetTypeId() == OMNI_STRING_VIEW) {
        if (encoding == OMNI_ENCODING_CONST) {
            return static_cast<ConstVector<StringView> *>(vec)->GetConstValueRef();
        } else if (encoding == OMNI_FLAT) {
            return static_cast<Vector<StringView> *>(vec)->GetValueRef(row);
        } else {
            OMNI_THROW("Concat function Error", "Unsupported encoding type for StringView string");
        }
    }

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("Concat function Error", "Unsupported encoding type for string");
    }
}

void ConcatFunction::SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const
{
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}

void ConcatWsFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    std::vector<BaseVector *> argVectors;
    for (int i = 0; i < inputDataTypes_.size(); ++i) {
        argVectors.push_back(args.top());
        args.pop();
    }
    std::reverse(argVectors.begin(), argVectors.end());
    ApplyConcatWs(argVectors, result, outputType, context);
    for (auto *argVec : argVectors) {
        delete argVec;
    }
}

void ConcatWsFunction::ApplyConcatWs(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
    const DataTypePtr &outputType, ExecutionContext *context) const
{
    int32_t size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_VARCHAR, size);
    auto resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    for (int32_t row = 0; row < size; ++row) {
        ConcatWsRow(argVectors, resultVector, row);
    }
}

/**
 * concat_ws  one row
 * @param argVectors
 * @param resultVector
 * @param row
 */
void ConcatWsFunction::ConcatWsRow(const std::vector<BaseVector *> &argVectors, Vector<LargeStringContainer<std::string_view>> *&resultVector, int32_t row) const {
    auto separatorVector = argVectors[0];
    if (separatorVector->IsNull(row)) {
        resultVector->SetNull(row);
        return;
    }
    auto separator = VectorHelper::GetStringValueFromVector(separatorVector, row);
    std::string result = "";
    bool first = false;
    for (int i = 1; i < argVectors.size(); ++i) {
        if (argVectors[i]->GetTypeId() == OMNI_VARCHAR) {
            auto argiVec = argVectors[i];
            if (first) {
                if (!argiVec->IsNull(row)) {
                    result += separator;
                    result += VectorHelper::GetStringValueFromVector(argiVec, row);
                }
            } else {
                if (!argiVec->IsNull(row)) {
                    result = VectorHelper::GetStringValueFromVector(argiVec, row);
                    first = true;
                }
            }
        } else { // Array<String>
            auto argiVec = static_cast<ArrayVector *>(argVectors[i]);
            if (first) {
                if (!argiVec->IsNull(row) && argiVec->GetSize(row) > 0) {
                    result += separator;
                    result += argiVec->GetValueToString(row, separator);
                }
            } else {
                if (!argiVec->IsNull(row) && argiVec->GetSize(row) > 0) {
                    result = argiVec->GetValueToString(row, separator);
                    first = true;
                }
            }
        }
    }
    resultVector->SetValue(row, result);
}
} // namespace omniruntime::vectorization
