/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayInsert function implementation
 */

#include "ArrayInsert.h"
#include "type/decimal128.h"
#include <iostream>
#include <algorithm>
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void ArrayInsertImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    auto *legacyArg = args.top();
    args.pop();
    auto legacyGuard = MakeVectorArgGuard(legacyArg);
    auto *itemArg = args.top();
    args.pop();
    auto itemGuard = MakeVectorArgGuard(itemArg);
    auto *posArg = args.top();
    args.pop();
    auto posGuard = MakeVectorArgGuard(posArg);
    auto *arrayArg = args.top();
    args.pop();
    auto arrayGuard = MakeVectorArgGuard(arrayArg);

    bool legacyNegativeIndex = false;
    if (legacyArg != nullptr && legacyArg->GetTypeId() == OMNI_BOOLEAN) {
        if (legacyArg->GetEncoding() == OMNI_ENCODING_CONST) {
            auto *constBoolVec = dynamic_cast<ConstVector<bool> *>(legacyArg);
            if (constBoolVec != nullptr) {
                legacyNegativeIndex = constBoolVec->GetConstValue();
            }
        } else {
            auto *boolVec = dynamic_cast<Vector<bool> *>(legacyArg);
            if (boolVec != nullptr && boolVec->GetSize() > 0) {
                legacyNegativeIndex = boolVec->GetValue(0);
            }
        }
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        OMNI_THROW("ArrayInsert error:", "First argument must be ARRAY type");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto inputElementVector = arrayVec->GetElementVector();
    DataTypeId elementTypeId = (inputElementVector != nullptr) ? inputElementVector->GetTypeId()
                                                               : OMNI_NONE;

    std::vector<int64_t> newSizes(rowSize, 0);
    std::vector<bool> rowIsNull(rowSize, false);
    int64_t totalElements = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row) || IsVectorNullAtRow(posArg, row)) {
            rowIsNull[row] = true;
            continue;
        }

        int32_t pos = GetPosValue(posArg, row);
        if (pos == 0) {
            OMNI_THROW("ArrayInsert error:", "Array insert position should not be 0");
        }

        int64_t srcSize = arrayVec->GetSize(row);

        if (pos > 0) {
            int64_t newLen = std::max(srcSize + 1, static_cast<int64_t>(pos));
            if (newLen > kMaxNumberOfElements) {
                OMNI_THROW("ArrayInsert error:",
                    "The size of result array must be less than or equal to " +
                    std::to_string(kMaxNumberOfElements));
            }
            newSizes[row] = newLen;
        } else {
            bool extendsLeft = -(static_cast<int64_t>(pos)) > srcSize;
            if (extendsLeft) {
                int64_t newLen = -(static_cast<int64_t>(pos)) + (legacyNegativeIndex ? 1 : 0);
                if (newLen > kMaxNumberOfElements) {
                    OMNI_THROW("ArrayInsert error:",
                        "The size of result array must be less than or equal to " +
                        std::to_string(kMaxNumberOfElements));
                }
                newSizes[row] = newLen;
            } else {
                int64_t newLen = srcSize + 1;
                if (newLen > kMaxNumberOfElements) {
                    OMNI_THROW("ArrayInsert error:",
                        "The size of result array must be less than or equal to " +
                        std::to_string(kMaxNumberOfElements));
                }
                newSizes[row] = newLen;
            }
        }
        totalElements += newSizes[row];
    }

    auto *resultArray = new ArrayVector(rowSize);

    BaseVector *newElementVector = nullptr;
    if (inputElementVector != nullptr) {
        if (elementTypeId == OMNI_VARCHAR || elementTypeId == OMNI_CHAR || elementTypeId == OMNI_VARBINARY) {
            newElementVector = VectorHelper::CreateStringVector(totalElements);
        } else {
            newElementVector = VectorHelper::CreateFlatVector(elementTypeId, totalElements);
        }
    }

    int64_t currentOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        resultArray->SetOffset(row, currentOffset);

        if (rowIsNull[row]) {
            resultArray->SetNull(row);
            continue;
        }

        resultArray->SetNotNull(row);

        int32_t pos = GetPosValue(posArg, row);
        int64_t srcSize = arrayVec->GetSize(row);
        int64_t srcOffset = arrayVec->GetOffset(row);
        int64_t newSize = newSizes[row];

        if (pos > 0) {
            int32_t posIdx = pos - 1;
            for (int64_t i = 0; i < newSize; ++i) {
                if (i == posIdx) {
                    CopyItemToElement(itemArg, row, newElementVector, currentOffset);
                } else {
                    int64_t srcIdx = (i > posIdx) ? i - 1 : i;
                    if (srcIdx < srcSize) {
                        if (inputElementVector->IsNull(srcOffset + srcIdx)) {
                            newElementVector->SetNull(currentOffset);
                        } else {
                            newElementVector->SetNotNull(currentOffset);
                            VectorHelper::CopyValue(inputElementVector.get(),
                                static_cast<int32_t>(srcOffset + srcIdx),
                                newElementVector, static_cast<int32_t>(currentOffset));
                        }
                    } else {
                        newElementVector->SetNull(currentOffset);
                    }
                }
                currentOffset++;
            }
        } else {
            bool extendsLeft = -(static_cast<int64_t>(pos)) > srcSize;
            if (extendsLeft) {
                CopyItemToElement(itemArg, row, newElementVector, currentOffset);
                currentOffset++;

                int64_t nullsToFill = newSize - 1 - srcSize;
                for (int64_t n = 0; n < nullsToFill; ++n) {
                    newElementVector->SetNull(currentOffset);
                    currentOffset++;
                }

                for (int64_t i = 0; i < srcSize; ++i) {
                    if (inputElementVector->IsNull(srcOffset + i)) {
                        newElementVector->SetNull(currentOffset);
                    } else {
                        newElementVector->SetNotNull(currentOffset);
                        VectorHelper::CopyValue(inputElementVector.get(),
                            static_cast<int32_t>(srcOffset + i),
                            newElementVector, static_cast<int32_t>(currentOffset));
                    }
                    currentOffset++;
                }
            } else {
                int64_t posIdx = pos + srcSize + (legacyNegativeIndex ? 0 : 1);
                int64_t nextIdx = 0;
                for (int64_t i = 0; i < srcSize; ++i) {
                    if (nextIdx == posIdx) {
                        CopyItemToElement(itemArg, row, newElementVector, currentOffset);
                        currentOffset++;
                        nextIdx++;
                    }
                    if (inputElementVector->IsNull(srcOffset + i)) {
                        newElementVector->SetNull(currentOffset);
                    } else {
                        newElementVector->SetNotNull(currentOffset);
                        VectorHelper::CopyValue(inputElementVector.get(),
                            static_cast<int32_t>(srcOffset + i),
                            newElementVector, static_cast<int32_t>(currentOffset));
                    }
                    currentOffset++;
                    nextIdx++;
                }
                if (nextIdx < newSize) {
                    CopyItemToElement(itemArg, row, newElementVector, currentOffset);
                    currentOffset++;
                }
            }
        }
    }

    resultArray->SetOffset(rowSize, currentOffset);

    if (newElementVector != nullptr) {
        resultArray->SetElementVector(std::shared_ptr<BaseVector>(newElementVector));
    }

    result = resultArray;
}

int32_t ArrayInsertImpl::GetPosValue(BaseVector *posArg, int32_t row) const
{
    if (posArg->GetEncoding() == OMNI_ENCODING_CONST) {
        if (posArg->GetTypeId() == OMNI_LONG || posArg->GetTypeId() == OMNI_TIMESTAMP) {
            auto *constVec = dynamic_cast<ConstVector<int64_t> *>(posArg);
            if (constVec != nullptr) {
                return static_cast<int32_t>(constVec->GetConstValue());
            }
        }
        auto *constVec = dynamic_cast<ConstVector<int32_t> *>(posArg);
        if (constVec != nullptr) {
            return constVec->GetConstValue();
        }
    }

    if (posArg->GetTypeId() == OMNI_LONG || posArg->GetTypeId() == OMNI_TIMESTAMP) {
        auto *flatVec = dynamic_cast<Vector<int64_t> *>(posArg);
        if (flatVec != nullptr) {
            return static_cast<int32_t>(flatVec->GetValue(row));
        }
    }

    auto *flatVec = dynamic_cast<Vector<int32_t> *>(posArg);
    if (flatVec != nullptr) {
        return flatVec->GetValue(row);
    }

    OMNI_THROW("ArrayInsert error:", "Unsupported type for position argument");
    return 0;
}

bool ArrayInsertImpl::IsVectorNullAtRow(BaseVector *vec, int32_t row) const
{
    if (vec == nullptr) {
        return true;
    }
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return vec->IsNull(0);
    }
    return vec->IsNull(row);
}

void ArrayInsertImpl::CopyItemToElement(BaseVector *itemVector, int32_t row,
    BaseVector *resultElem, int64_t targetIdx) const
{
    if (IsVectorNullAtRow(itemVector, row)) {
        resultElem->SetNull(static_cast<int32_t>(targetIdx));
        return;
    }

    resultElem->SetNotNull(static_cast<int32_t>(targetIdx));

    if (itemVector->GetEncoding() == OMNI_ENCODING_CONST) {
        CopyConstItemToElement(itemVector, resultElem, targetIdx);
    } else {
        VectorHelper::CopyValue(itemVector, row, resultElem, static_cast<int32_t>(targetIdx));
    }
}

void ArrayInsertImpl::CopyConstItemToElement(BaseVector *constItem,
    BaseVector *resultElem, int64_t targetIdx) const
{
    DataTypeId typeId = constItem->GetTypeId();
    int32_t idx = static_cast<int32_t>(targetIdx);

    switch (typeId) {
        case OMNI_BYTE: {
            auto val = static_cast<ConstVector<int8_t> *>(constItem)->GetConstValue();
            static_cast<Vector<int8_t> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_SHORT: {
            auto val = static_cast<ConstVector<int16_t> *>(constItem)->GetConstValue();
            static_cast<Vector<int16_t> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            auto val = static_cast<ConstVector<int32_t> *>(constItem)->GetConstValue();
            static_cast<Vector<int32_t> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            auto val = static_cast<ConstVector<int64_t> *>(constItem)->GetConstValue();
            static_cast<Vector<int64_t> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_FLOAT: {
            auto val = static_cast<ConstVector<float> *>(constItem)->GetConstValue();
            static_cast<Vector<float> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_DOUBLE: {
            auto val = static_cast<ConstVector<double> *>(constItem)->GetConstValue();
            static_cast<Vector<double> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_BOOLEAN: {
            auto val = static_cast<ConstVector<bool> *>(constItem)->GetConstValue();
            static_cast<Vector<bool> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            auto val = static_cast<ConstVector<std::string_view> *>(constItem)->GetConstValue();
            static_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElem)->SetValue(idx, val);
            break;
        }
        case OMNI_DECIMAL128: {
            auto val = static_cast<ConstVector<Decimal128> *>(constItem)->GetConstValue();
            static_cast<Vector<Decimal128> *>(resultElem)->SetValue(idx, val);
            break;
        }
        default:
            OMNI_THROW("ArrayInsert error:",
                "Unsupported element type for const item: " + std::to_string(typeId));
    }
}

} // namespace omniruntime::vectorization
