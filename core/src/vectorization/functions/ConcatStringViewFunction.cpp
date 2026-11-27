/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: concat SV-in / SV-out (materialized StringView output) — implementation.
 */

#include "ConcatStringViewFunction.h"

#include <string>

#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void ConcatStringViewFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("Concat function Error", "Concat requires at least 2 arguments");
    }

    BaseVector *arg2 = args.top();
    args.pop();
    BaseVector *arg1 = args.top();
    args.pop();

    std::vector<BaseVector *> argVectors = {arg1, arg2};
    ApplyConcat(argVectors, result, context);

    delete arg1;
    delete arg2;
}

void ConcatStringViewFunction::ApplyConcat(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
    ExecutionContext *context)
{
    const int32_t size = context->GetResultRowSize();
    auto *out = new Vector<StringView>(size, context->GetPool());

    for (int32_t row = 0; row < size; ++row) {
        bool hasNull = false;
        for (auto *argVec : argVectors) {
            const int32_t nullCheckIdx = (argVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
            if (argVec->IsNull(nullCheckIdx)) {
                hasNull = true;
                break;
            }
        }

        if (hasNull) {
            out->SetNull(row);
            continue;
        }

        std::string concatenated;
        for (auto *argVec : argVectors) {
            const std::string_view str = GetStringValueFromVector(argVec, row);
            concatenated.append(str.data(), str.size());
        }
        out->SetValue(row, StringView(concatenated));
    }

    result = out;
}

std::string_view ConcatStringViewFunction::GetStringValueFromVector(BaseVector *vec, int32_t row)
{
    const Encoding encoding = vec->GetEncoding();

    if (vec->GetTypeId() == OMNI_STRING_VIEW) {
        if (encoding == OMNI_ENCODING_CONST) {
            return static_cast<ConstVector<StringView> *>(vec)->GetConstValueRef();
        }
        if (encoding == OMNI_FLAT) {
            return static_cast<Vector<StringView> *>(vec)->GetValueRef(row);
        }
        OMNI_THROW("Concat function Error", "Unsupported encoding type for StringView string");
    }

    if (encoding == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<std::string_view> *>(vec)->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        return static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        return static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec)->GetValue(row);
    }
    OMNI_THROW("Concat function Error", "Unsupported encoding type for string");
}

} // namespace omniruntime::vectorization
