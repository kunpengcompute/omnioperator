/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "SubscriptUtil.h"

#include <limits>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

bool IsSelectedElementNull(BaseVector *element, int64_t position)
{
    if (element == nullptr) {
        return true;
    }
    if (element->GetEncoding() == OMNI_ENCODING_CONST) {
        int32_t nullIndex = (position >= 0 && position < element->GetSize()) ? static_cast<int32_t>(position) : 0;
        return element->HasNull() && element->IsNull(nullIndex);
    }
    if (position < 0 || position >= element->GetSize()) {
        return true;
    }
    return element->IsNull(static_cast<int32_t>(position));
}

BaseVector *CreateGetResult(const DataTypePtr &outputType, int32_t rowSize)
{
    if (outputType == nullptr) {
        OMNI_THROW("element_at error:", "output type is null");
    }
    auto typeId = outputType->GetId();
    if (typeId == OMNI_ARRAY || typeId == OMNI_MAP || typeId == OMNI_ROW) {
        return VectorHelper::CreateComplexVector(outputType.get(), rowSize);
    }
    auto *result = VectorHelper::CreateFlatVector(static_cast<int32_t>(typeId), rowSize);
    if (typeId == OMNI_DECIMAL64 || typeId == OMNI_DECIMAL128) {
        VectorHelper::SetVectorDataType(result, outputType.get());
    }
    return result;
}

// Map and array element_at are defined after the shared helpers.
bool ReadStringView(BaseVector *vec, int32_t row, std::string_view &out)
{
    if (vec == nullptr || vec->IsNull(row)) {
        return false;
    }
    switch (vec->GetEncoding()) {
        case OMNI_ENCODING_CONST: {
            auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
            out = constVec->GetConstValue();
            return true;
        }
        case OMNI_FLAT:
            out = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec)->GetValue(row);
            return true;
        case OMNI_DICTIONARY:
            out = static_cast<Vector<DictionaryContainer<std::string_view>> *>(vec)->GetValue(row);
            return true;
        default:
            OMNI_THROW("element_at error:", "Unsupported string key encoding");
    }
}

template <typename Key>
BaseVector *ApplyMapKey(BaseVector *mapArg, BaseVector *keyArg, const DataTypePtr &outputType)
{
    auto *mapVector = dynamic_cast<MapVector *>(mapArg);
    if (mapVector == nullptr) {
        OMNI_THROW("element_at error:", "map argument must be a MapVector");
    }
    auto rowSize = mapVector->GetSize();
    auto *keys = mapVector->GetKeyVector().get();
    auto *values = mapVector->GetValueVector().get();
    auto *offsets = mapVector->GetOffsets();
    DataTypePtr valueType = outputType;
    if (valueType == nullptr && values != nullptr) {
        valueType = std::make_shared<DataType>(values->GetTypeId());
    }
    auto *outVec = CreateGetResult(valueType, rowSize);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (mapVector->IsNull(row)) {
            VectorHelper::SetNull(outVec, row);
            continue;
        }
        Key needle;
        if (!VectorHelper::GetValueFromVector<Key>(keyArg, row, needle)) {
            VectorHelper::SetNull(outVec, row);
            continue;
        }
        int32_t start = static_cast<int32_t>(offsets[row]);
        int32_t end = static_cast<int32_t>(offsets[row + 1]);
        int32_t found = -1;
        for (int32_t i = start; i < end; ++i) {
            Key candidate;
            if (!VectorHelper::GetValueFromVector<Key>(keys, i, candidate)) {
                continue;
            }
            if (candidate == needle) {
                found = i;
                break;
            }
        }
        if (found < 0 || IsSelectedElementNull(values, found)) {
            VectorHelper::SetNull(outVec, row);
        } else {
            VectorHelper::CopyValue(values, found, outVec, row);
        }
    }
    return outVec;
}

BaseVector *ApplyMapString(BaseVector *mapArg, BaseVector *keyArg, const DataTypePtr &outputType)
{
    auto *mapVector = dynamic_cast<MapVector *>(mapArg);
    if (mapVector == nullptr) {
        OMNI_THROW("element_at error:", "map argument must be a MapVector");
    }
    auto rowSize = mapVector->GetSize();
    auto *keys = mapVector->GetKeyVector().get();
    auto *values = mapVector->GetValueVector().get();
    auto *offsets = mapVector->GetOffsets();
    DataTypePtr valueType = outputType;
    if (valueType == nullptr && values != nullptr) {
        valueType = std::make_shared<DataType>(values->GetTypeId());
    }
    auto *outVec = CreateGetResult(valueType, rowSize);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (mapVector->IsNull(row)) {
            VectorHelper::SetNull(outVec, row);
            continue;
        }
        std::string_view needle;
        if (!ReadStringView(keyArg, row, needle)) {
            VectorHelper::SetNull(outVec, row);
            continue;
        }
        int32_t start = static_cast<int32_t>(offsets[row]);
        int32_t end = static_cast<int32_t>(offsets[row + 1]);
        int32_t found = -1;
        for (int32_t i = start; i < end; ++i) {
            std::string_view candidate;
            if (!ReadStringView(keys, i, candidate)) {
                continue;
            }
            if (candidate == needle) {
                found = i;
                break;
            }
        }
        if (found < 0 || IsSelectedElementNull(values, found)) {
            VectorHelper::SetNull(outVec, row);
        } else {
            VectorHelper::CopyValue(values, found, outVec, row);
        }
    }
    return outVec;
}

namespace {

template <typename I>
BaseVector *ApplyGetArrayItemTyped(BaseVector *arrayArg, BaseVector *indexArg, const DataTypePtr &outputType)
{
    auto *arrayVector = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVector == nullptr) {
        OMNI_THROW("get_array_item error:", "array argument must be an ArrayVector");
    }
    auto rowSize = arrayVector->GetSize();
    auto *result = CreateGetResult(outputType, rowSize);
    auto *offsets = arrayVector->GetOffsets();
    BaseVector *elements = arrayVector->GetElementVector().get();

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVector->IsNull(row)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        I rawIndex;
        if (!VectorHelper::GetValueFromVector<I>(indexArg, row, rawIndex)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        // Spark GetArrayItem codegen: final int index = (int) indexExpr;
        int32_t index = static_cast<int32_t>(rawIndex);
        int64_t rowArraySize = offsets[row + 1] - offsets[row];
        if (index < 0 || static_cast<int64_t>(index) >= rowArraySize ||
            offsets[row] > std::numeric_limits<int32_t>::max() - index) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        int64_t elementPosition = offsets[row] + index;
        if (IsSelectedElementNull(elements, elementPosition)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        VectorHelper::CopyValue(elements, static_cast<int32_t>(elementPosition), result, row);
    }
    return result;
}

}  // namespace

void GetArrayItemFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    (void)context;
    if (args.size() < 2) {
        OMNI_THROW("get_array_item error:", "Expected 2 arguments (array, index)");
    }
    auto *indexArg = args.top();
    args.pop();
    auto *arrayArg = args.top();
    args.pop();
    if (arrayArg == nullptr || indexArg == nullptr || outputType == nullptr) {
        delete arrayArg;
        delete indexArg;
        OMNI_THROW("get_array_item error:", "arguments or output type is null");
    }

    switch (indexArg->GetTypeId()) {
        case OMNI_BYTE:
            result = ApplyGetArrayItemTyped<int8_t>(arrayArg, indexArg, outputType);
            break;
        case OMNI_SHORT:
            result = ApplyGetArrayItemTyped<int16_t>(arrayArg, indexArg, outputType);
            break;
        case OMNI_INT:
            result = ApplyGetArrayItemTyped<int32_t>(arrayArg, indexArg, outputType);
            break;
        case OMNI_LONG:
            result = ApplyGetArrayItemTyped<int64_t>(arrayArg, indexArg, outputType);
            break;
        default:
            delete arrayArg;
            delete indexArg;
            OMNI_THROW("get_array_item error:", "Unsupported index type");
    }
    delete arrayArg;
    delete indexArg;
}

BaseVector *SubscriptImpl::applyArray(const SelectivityVector &rows, std::vector<BaseVector *> &args,
    ExecutionContext *context, const DataTypePtr &outputType) const
{
    (void)rows;
    (void)context;
    auto *arrayArg = args[1];
    auto *indexArg = args[0];
    switch (indexArg->GetTypeId()) {
        case OMNI_INT:
            return applyArrayTyped<int32_t>(rows, arrayArg, indexArg, context, outputType);
        case OMNI_LONG:
            return applyArrayTyped<int64_t>(rows, arrayArg, indexArg, context, outputType);
        default:
            OMNI_THROW("element_at error:", "Unsupported index type");
    }
}

template <typename I>
BaseVector *SubscriptImpl::applyArrayTyped(const SelectivityVector &rows, BaseVector *arrayArg, BaseVector *indexArg,
    ExecutionContext *context, const DataTypePtr &outputType) const
{
    (void)rows;
    (void)context;
    auto *arrayVector = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVector == nullptr) {
        OMNI_THROW("element_at error:", "array argument must be an ArrayVector");
    }
    auto rowSize = arrayVector->GetSize();
    auto *result = CreateGetResult(outputType, rowSize);
    auto *offsets = arrayVector->GetOffsets();
    BaseVector *elements = arrayVector->GetElementVector().get();
    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVector->IsNull(row)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        I rawIndex;
        if (!VectorHelper::GetValueFromVector<I>(indexArg, row, rawIndex)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        // Spark element_at: index 0 always throws. Positive indexes are 1-based.
        // Negative indexes count from the end. |index| past the length is NULL.
        if (rawIndex == 0) {
            delete result;
            OMNI_THROW("Runtime Error:",
                "The index 0 is invalid!. An index shall be either < 0 or > 0 (the first element has index 1)");
        }
        int64_t rowArraySize = offsets[row + 1] - offsets[row];
        int64_t zeroBased = rawIndex > 0 ? static_cast<int64_t>(rawIndex) - 1
                                         : rowArraySize + static_cast<int64_t>(rawIndex);
        if (zeroBased < 0 || zeroBased >= rowArraySize ||
            offsets[row] > std::numeric_limits<int32_t>::max() - zeroBased) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        int64_t elementPosition = offsets[row] + zeroBased;
        if (IsSelectedElementNull(elements, elementPosition)) {
            VectorHelper::SetNull(result, row);
            continue;
        }
        VectorHelper::CopyValue(elements, static_cast<int32_t>(elementPosition), result, row);
    }
    return result;
}

template BaseVector *SubscriptImpl::applyArrayTyped<int32_t>(const SelectivityVector &, BaseVector *, BaseVector *,
    ExecutionContext *, const DataTypePtr &) const;
template BaseVector *SubscriptImpl::applyArrayTyped<int64_t>(const SelectivityVector &, BaseVector *, BaseVector *,
    ExecutionContext *, const DataTypePtr &) const;

BaseVector *SubscriptImpl::applyMap(const SelectivityVector &rows, std::vector<BaseVector *> &args,
    ExecutionContext *context, const DataTypePtr &outputType) const
{
    (void)rows;
    (void)context;
    auto *mapArg = args[1];
    auto *keyArg = args[0];
    switch (keyArg->GetTypeId()) {
        case OMNI_BOOLEAN:
            return ApplyMapKey<bool>(mapArg, keyArg, outputType);
        case OMNI_BYTE:
            return ApplyMapKey<int8_t>(mapArg, keyArg, outputType);
        case OMNI_SHORT:
            return ApplyMapKey<int16_t>(mapArg, keyArg, outputType);
        case OMNI_INT:
            return ApplyMapKey<int32_t>(mapArg, keyArg, outputType);
        case OMNI_LONG:
            return ApplyMapKey<int64_t>(mapArg, keyArg, outputType);
        case OMNI_FLOAT:
            return ApplyMapKey<float>(mapArg, keyArg, outputType);
        case OMNI_DOUBLE:
            return ApplyMapKey<double>(mapArg, keyArg, outputType);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            return ApplyMapString(mapArg, keyArg, outputType);
        default:
            OMNI_THROW("element_at error:", "Unsupported map key type");
    }
}
}
