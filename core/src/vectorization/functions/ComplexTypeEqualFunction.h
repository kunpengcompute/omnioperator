/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Complex type equal comparison function for ARRAY and ROW types
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {

class ComplexTypeEqualFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        auto left = args.top();
        args.pop();
        auto leftGuard = MakeVectorArgGuard(left);
        auto right = args.top();
        args.pop();
        auto rightGuard = MakeVectorArgGuard(right);

        auto size = context->GetResultRowSize();
        result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);

        auto inputTypeId = left->GetTypeId();

        if (inputTypeId == OMNI_ARRAY) {
            ApplyArray(left, right, result, size);
        } else if (inputTypeId == OMNI_ROW) {
            ApplyRow(left, right, result, size);
        } else {
            OMNI_THROW("ComplexTypeEqualFunction", "Unsupported type for complex type equal");
        }
    }

private:
    void ApplyArray(BaseVector *left, BaseVector *right, BaseVector *result, int32_t size) const
    {
        auto *leftArray = dynamic_cast<ArrayVector *>(left);
        auto *rightArray = dynamic_cast<ArrayVector *>(right);

        if (!leftArray || !rightArray) {
            OMNI_THROW("ComplexTypeEqualFunction", "Invalid ArrayVector cast");
        }

        for (int32_t i = 0; i < size; i++) {
            if (leftArray->IsNull(i) && rightArray->IsNull(i)) {
                static_cast<Vector<bool> *>(result)->SetValue(i, true);
            } else if (leftArray->IsNull(i) || rightArray->IsNull(i)) {
                static_cast<Vector<bool> *>(result)->SetValue(i, false);
            } else {
                int32_t leftSize = leftArray->GetSize(i);
                int32_t rightSize = rightArray->GetSize(i);

                if (leftSize != rightSize) {
                    static_cast<Vector<bool> *>(result)->SetValue(i, false);
                } else {
                    bool equal = true;
                    int32_t leftOffset = leftArray->GetOffset(i);
                    int32_t rightOffset = rightArray->GetOffset(i);

                    auto *leftElements = leftArray->GetElementVector().get();
                    auto *rightElements = rightArray->GetElementVector().get();

                    for (int32_t j = 0; j < leftSize && equal; j++) {
                        if (leftElements->IsNull(leftOffset + j) != rightElements->IsNull(rightOffset + j)) {
                            equal = false;
                        } else if (!leftElements->IsNull(leftOffset + j)) {
                            equal = CompareElements(leftElements, rightElements, leftOffset + j, rightOffset + j);
                        }
                    }
                    static_cast<Vector<bool> *>(result)->SetValue(i, equal);
                }
            }
        }
    }

    void ApplyRow(BaseVector *left, BaseVector *right, BaseVector *result, int32_t size) const
    {
        auto *leftRow = dynamic_cast<RowVector *>(left);
        auto *rightRow = dynamic_cast<RowVector *>(right);

        if (!leftRow || !rightRow) {
            OMNI_THROW("ComplexTypeEqualFunction", "Invalid RowVector cast");
        }

        int32_t childCount = leftRow->ChildSize();
        if (childCount != rightRow->ChildSize()) {
            OMNI_THROW("ComplexTypeEqualFunction", "RowVector child count mismatch");
        }

        for (int32_t i = 0; i < size; i++) {
            if (leftRow->IsNull(i) && rightRow->IsNull(i)) {
                static_cast<Vector<bool> *>(result)->SetValue(i, true);
            } else if (leftRow->IsNull(i) || rightRow->IsNull(i)) {
                static_cast<Vector<bool> *>(result)->SetValue(i, false);
            } else {
                bool equal = true;
                for (int32_t j = 0; j < childCount && equal; j++) {
                    auto *leftChild = leftRow->ChildAt(j).get();
                    auto *rightChild = rightRow->ChildAt(j).get();

                    if (leftChild->IsNull(i) != rightChild->IsNull(i)) {
                        equal = false;
                    } else if (!leftChild->IsNull(i)) {
                        equal = CompareElements(leftChild, rightChild, i, i);
                    }
                }
                static_cast<Vector<bool> *>(result)->SetValue(i, equal);
            }
        }
    }

    bool CompareElements(BaseVector *left, BaseVector *right, int32_t leftIndex, int32_t rightIndex) const
    {
        auto typeId = left->GetTypeId();

        switch (typeId) {
            case OMNI_BOOLEAN:
                return VectorHelper::GetValueFromVector<bool>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<bool>(right, rightIndex);
            case OMNI_BYTE:
                return VectorHelper::GetValueFromVector<int8_t>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<int8_t>(right, rightIndex);
            case OMNI_SHORT:
                return VectorHelper::GetValueFromVector<int16_t>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<int16_t>(right, rightIndex);
            case OMNI_INT:
            case OMNI_DATE32:
                return VectorHelper::GetValueFromVector<int32_t>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<int32_t>(right, rightIndex);
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                return VectorHelper::GetValueFromVector<int64_t>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<int64_t>(right, rightIndex);
            case OMNI_FLOAT:
                return VectorHelper::GetValueFromVector<float>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<float>(right, rightIndex);
            case OMNI_DOUBLE:
                return VectorHelper::GetValueFromVector<double>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<double>(right, rightIndex);
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            case OMNI_VARBINARY:
                return VectorHelper::GetStringValueFromVector(left, leftIndex) ==
                       VectorHelper::GetStringValueFromVector(right, rightIndex);
            case OMNI_DECIMAL128:
                return VectorHelper::GetValueFromVector<Decimal128>(left, leftIndex) ==
                       VectorHelper::GetValueFromVector<Decimal128>(right, rightIndex);
            case OMNI_ARRAY:
            case OMNI_ROW:
                return CompareComplexTypes(left, right, leftIndex, rightIndex, typeId);
            default:
                OMNI_THROW("ComplexTypeEqualFunction", "Unsupported element type for comparison");
        }
    }

    bool CompareComplexTypes(BaseVector *left, BaseVector *right, int32_t leftIndex, int32_t rightIndex,
                             DataTypeId typeId) const
    {
        if (typeId == OMNI_ARRAY) {
            auto *leftArray = dynamic_cast<ArrayVector *>(left);
            auto *rightArray = dynamic_cast<ArrayVector *>(right);

            int32_t leftSize = leftArray->GetSize(leftIndex);
            int32_t rightSize = rightArray->GetSize(rightIndex);

            if (leftSize != rightSize) {
                return false;
            }

            int32_t leftOffset = leftArray->GetOffset(leftIndex);
            int32_t rightOffset = rightArray->GetOffset(rightIndex);

            auto *leftElements = leftArray->GetElementVector().get();
            auto *rightElements = rightArray->GetElementVector().get();

            for (int32_t j = 0; j < leftSize; j++) {
                if (leftElements->IsNull(leftOffset + j) != rightElements->IsNull(rightOffset + j)) {
                    return false;
                }
                if (!leftElements->IsNull(leftOffset + j)) {
                    if (!CompareElements(leftElements, rightElements, leftOffset + j, rightOffset + j)) {
                        return false;
                    }
                }
            }
            return true;
        } else if (typeId == OMNI_ROW) {
            auto *leftRow = dynamic_cast<RowVector *>(left);
            auto *rightRow = dynamic_cast<RowVector *>(right);

            int32_t childCount = leftRow->ChildSize();
            if (childCount != rightRow->ChildSize()) {
                return false;
            }

            for (int32_t j = 0; j < childCount; j++) {
                auto *leftChild = leftRow->ChildAt(j).get();
                auto *rightChild = rightRow->ChildAt(j).get();

                if (leftChild->IsNull(leftIndex) != rightChild->IsNull(rightIndex)) {
                    return false;
                }
                if (!leftChild->IsNull(leftIndex)) {
                    if (!CompareElements(leftChild, rightChild, leftIndex, rightIndex)) {
                        return false;
                    }
                }
            }
            return true;
        }
        return false;
    }
};

}