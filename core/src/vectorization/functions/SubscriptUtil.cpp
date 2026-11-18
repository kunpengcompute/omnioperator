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

/// Decode arguments and transform result into a dictionaryVector where the
/// dictionary maintains a mapping from a given row to the index of the input
/// map value vector. This allows us to ensure that element_at is zero-copy.
template <DataTypeId kind>
BaseVector *ApplyMapTyped(const SelectivityVector &rows, BaseVector *mapArg, BaseVector *indexArg,
    ExecutionContext *context)
{
    using KeyType = typename NativeType<kind>::type;
    auto mapVector = reinterpret_cast<MapVector *>(mapArg);
    auto rowSize = mapArg->GetSize();

    // Current element_at(map, key) registration only uses string keys. For
    // safety and simplicity we materialize a new flat vector instead of a
    // dictionary; correctness is preferred over zero-copy here.
    if (indexArg->GetEncoding() == OMNI_ENCODING_CONST) {
        if constexpr (kind == OMNI_VARCHAR || kind == OMNI_CHAR) {
            auto constKeyVector = dynamic_cast<ConstVector<std::string_view> *>(indexArg);
            auto key = constKeyVector->GetConstValue();
            auto keyVector = dynamic_cast<Vector<LargeStringContainer<KeyType>> *>(mapVector->GetKeyVector().get());
            auto valueVector = mapVector->GetValueVector().get();
            auto valueTypeId = valueVector->GetTypeId();
            auto offsets = mapVector->GetOffsets();

            // Create output flat vector of map value type.
            auto *outVec = VectorHelper::CreateVector(OMNI_FLAT, static_cast<int32_t>(valueTypeId), rowSize);

            for (int32_t row = 0; row < rowSize; ++row) {
                int32_t start = offsets[row];
                int32_t end = offsets[row + 1];
                int32_t foundIndex = -1;
                for (int32_t i = start; i < end; ++i) {
                    if (keyVector->GetValue(i) == key) {
                        foundIndex = i;
                        break;
                    }
                }
                if (foundIndex >= 0) {
                    VectorHelper::CopyValue(valueVector, foundIndex, outVec, row);
                } else {
                    outVec->SetNull(row);
                }
            }
            return outVec;
        } else {
            OMNI_THROW("ApplyMapTyped Error:", "Not support Key type!");
        }
    }

    OMNI_THROW("ApplyMapTyped Error:", "Not support Key type!");
}

BaseVector *SubscriptImpl::applyMap(const SelectivityVector &rows, std::vector<BaseVector *> &args,
    ExecutionContext *context) const
{
    auto mapArg = args[1];
    auto indexArg = args[0];

    // Ensure map key type and second argument are the same.
    if (indexArg->GetTypeId() != OMNI_ARRAY) {
        return DYNAMIC_TYPE_DISPATCH(ApplyMapTyped, indexArg->GetTypeId(), rows, mapArg, indexArg, context);
    } else {
        // We use applyMapComplexType when the key type is complex, but also when it
        // provides custom comparison operators because the main difference between
        // applyMapComplexType and applyTyped is that applyMapComplexType calls the
        // Vector's equalValueAt method, which calls the Types custom comparison
        // operator internally.
        OMNI_THROW("ApplyMapTyped Error:", "Not support Key type!");
    }
}

namespace {

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
}
