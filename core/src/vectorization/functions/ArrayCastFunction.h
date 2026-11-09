/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: cast array elements when the container type id stays OMNI_ARRAY
 */

#pragma once

#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"

#include <string>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

class ArrayCastFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        if (args.empty()) {
            OMNI_THROW("ArrayCast Error:", "CAST array requires 1 argument");
        }
        BaseVector *input = args.top();
        args.pop();
        auto *src = dynamic_cast<ArrayVector *>(input);
        auto *outType = dynamic_cast<ArrayType *>(outputType.get());
        if (src == nullptr || outType == nullptr) {
            delete input;
            OMNI_THROW("ArrayCast Error:", "CAST array requires array input and array return type");
        }

        const int32_t nRows = src->GetSize();
        auto dstElemsOwner = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(outType->ElementType()->GetId(), src->GetElementVector() == nullptr
                    ? 0
                    : src->GetElementVector()->GetSize()));
        if (src->GetElementVector() != nullptr) {
            CastElements(src->GetElementVector().get(), dstElemsOwner.get(), outType->ElementType()->GetId());
        }

        auto *dst = new ArrayVector(nRows, dstElemsOwner);
        for (int32_t row = 0; row < nRows; ++row) {
            dst->SetOffset(row, static_cast<int32_t>(src->GetOffset(row)));
            if (src->IsNull(row)) {
                dst->SetNull(row);
            }
        }
        dst->SetOffset(nRows, static_cast<int32_t>(src->GetOffset(nRows)));
        result = dst;
        delete input;
    }

private:
    using StringVector = Vector<LargeStringContainer<std::string_view>>;

    void CastElements(BaseVector *srcElems, BaseVector *dstElems, DataTypeId dstTypeId) const
    {
        if (!TypeUtil::IsStringType(dstTypeId)) {
            OMNI_THROW("ArrayCast Error:", "only array element cast to string is supported");
        }
        auto *dst = static_cast<StringVector *>(dstElems);
        const int32_t n = srcElems->GetSize();
        switch (srcElems->GetTypeId()) {
            case OMNI_BYTE:
                CastNumeric<int8_t>(srcElems, dst, n);
                break;
            case OMNI_SHORT:
                CastNumeric<int16_t>(srcElems, dst, n);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                CastNumeric<int32_t>(srcElems, dst, n);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
                CastNumeric<int64_t>(srcElems, dst, n);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                CopyStrings(srcElems, dst, n);
                break;
            default:
                OMNI_THROW("ArrayCast Error:", "unsupported array element type");
        }
    }

    template <typename T>
    void CastNumeric(BaseVector *srcElems, StringVector *dst, int32_t n) const
    {
        auto *src = static_cast<Vector<T> *>(srcElems);
        for (int32_t i = 0; i < n; ++i) {
            if (src->IsNull(i)) {
                dst->SetNull(i);
                continue;
            }
            std::string text = std::to_string(static_cast<int64_t>(src->GetValue(i)));
            dst->SetValue(i, std::string_view(text));
        }
    }

    void CopyStrings(BaseVector *srcElems, StringVector *dst, int32_t n) const
    {
        auto *src = static_cast<StringVector *>(srcElems);
        for (int32_t i = 0; i < n; ++i) {
            if (src->IsNull(i)) {
                dst->SetNull(i);
                continue;
            }
            auto value = src->GetValue(i);
            dst->SetValue(i, value);
        }
    }
};
}

