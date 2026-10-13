/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayPosition function implementation for finding element position in array
 */

#include "ArrayPositionFunction.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void ArrayPositionFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayPositionFunction Error:", "Requires exactly 2 arguments");
    }

    auto searchArg = args.top();
    args.pop();
    auto arrayArg = args.top();
    args.pop();

    if (arrayArg == nullptr || searchArg == nullptr) {
        delete arrayArg;
        delete searchArg;
        OMNI_THROW("ArrayPositionFunction Error:", "Input vector is null");
    }

    if (arrayArg->GetTypeId() != OMNI_ARRAY) {
        delete arrayArg;
        delete searchArg;
        OMNI_THROW("ArrayPositionFunction Error:", "First argument is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        delete arrayArg;
        delete searchArg;
        OMNI_THROW("ArrayPositionFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();

    result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_LONG, rowSize);
    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    if (resultVec == nullptr) {
        delete arrayArg;
        delete searchArg;
        OMNI_THROW("ArrayPositionFunction Error:", "Failed to create result vector");
    }

    auto elementVec = arrayVec->GetElementVector();
    DataTypeId elementTypeId = OMNI_INT;
    if (elementVec != nullptr) {
        elementTypeId = elementVec->GetTypeId();
    } else {
        elementTypeId = searchArg->GetTypeId();
    }

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessArrayPosition<int8_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_SHORT:
            ProcessArrayPosition<int16_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_INT:
            ProcessArrayPosition<int32_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_LONG:
            ProcessArrayPosition<int64_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_FLOAT:
            ProcessArrayPosition<float>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_DOUBLE:
            ProcessArrayPosition<double>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_BOOLEAN:
            ProcessArrayPosition<bool>(arrayVec, searchArg, resultVec, rowSize);
            break;
        default:
            delete arrayArg;
            delete searchArg;
            OMNI_THROW("ArrayPositionFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    delete arrayArg;
    delete searchArg;
}

template <typename T>
void ArrayPositionFunction::ProcessArrayPosition(ArrayVector *arrayVec, BaseVector *searchVec,
    Vector<int64_t> *resultVec, int32_t rowSize) const
{
    auto elementVec = arrayVec->GetElementVector();

    if (elementVec == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            if (arrayVec->IsNull(row)) {
                resultVec->SetNull(row);
            } else {
                resultVec->SetValue(row, static_cast<int64_t>(0));
            }
        }
        return;
    }

    auto *typedElementVec = dynamic_cast<Vector<T> *>(elementVec.get());
    if (typedElementVec == nullptr) {
        OMNI_THROW("ArrayPositionFunction Error:", "Element vector type mismatch");
    }

    int64_t *offsets = arrayVec->GetOffsets();

    bool isConstSearch = (searchVec->GetEncoding() == OMNI_ENCODING_CONST);

    T constSearchValue {};
    bool constSearchIsNull = false;
    if (isConstSearch) {
        auto *constVec = dynamic_cast<ConstVector<T> *>(searchVec);
        if (constVec == nullptr) {
            OMNI_THROW("ArrayPositionFunction Error:", "Search value type mismatch");
        }
        constSearchIsNull = searchVec->IsNull(0);
        if (!constSearchIsNull) {
            constSearchValue = constVec->GetConstValue();
        }
    }

    auto *typedSearchVec = isConstSearch ? nullptr : dynamic_cast<Vector<T> *>(searchVec);
    if (!isConstSearch && typedSearchVec == nullptr) {
        OMNI_THROW("ArrayPositionFunction Error:", "Search value type mismatch");
    }

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        bool searchIsNull = isConstSearch ? constSearchIsNull : searchVec->IsNull(row);
        T searchValue {};
        if (!searchIsNull) {
            searchValue = isConstSearch ? constSearchValue : typedSearchVec->GetValue(row);
        }

        if (searchIsNull) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];

        int64_t position = 0;
        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (typedElementVec->IsNull(static_cast<int32_t>(i))) {
                continue;
            }

            T elementValue = typedElementVec->GetValue(static_cast<int32_t>(i));

            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(searchValue) && std::isnan(elementValue)) {
                    position = (i - startOffset) + 1;
                    break;
                }
            }

            if (elementValue == searchValue) {
                position = (i - startOffset) + 1;
                break;
            }
        }

        resultVec->SetValue(row, position);
    }
}

template void ArrayPositionFunction::ProcessArrayPosition<int8_t>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<int16_t>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<int32_t>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<int64_t>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<float>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<double>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;
template void ArrayPositionFunction::ProcessArrayPosition<bool>(ArrayVector *, BaseVector *,
    Vector<int64_t> *, int32_t) const;

} // namespace omniruntime::vectorization
