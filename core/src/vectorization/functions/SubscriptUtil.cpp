/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "SubscriptUtil.h"

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
            auto *outVec = VectorHelper::CreateVector(context->GetPool(), OMNI_FLAT, static_cast<int32_t>(valueTypeId), rowSize);

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
}
