/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#include "hash_builder.h"
#include <vector>
#include <memory>

namespace omniruntime {
namespace op {
HashBuilderOperatorFactory::HashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
    const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
    : buildTypes(buildTypes),
      buildHashCols(std::vector<int32_t>(buildHashCols, buildHashCols + buildHashColsCount)),
      operatorIndex(0)
{
    if (operatorCount <= 0 || operatorCount > 10000) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "operatorCount is not in the acceptable range [1, 10000].");
    }
    if (joinType != OMNI_JOIN_TYPE_FULL) {
        hashTablesVariants = InitVariant<RowRefList>(buildHashColsCount, operatorCount, joinType);
    } else {
        hashTablesVariants = InitVariant<RowRefListWithFlags>(buildHashColsCount, operatorCount, joinType);
    }
}

HashBuilderOperatorFactory::HashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide,
    const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
    : buildTypes(buildTypes),
      buildHashCols(std::vector<int32_t>(buildHashCols, buildHashCols + buildHashColsCount)),
      operatorIndex(0)
{
    if (operatorCount <= 0 || operatorCount > 10000) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "operatorCount is not in the acceptable range [1, 10000].");
    }
    if (joinType == OMNI_JOIN_TYPE_FULL || (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT)
        || (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT)) {
        hashTablesVariants = InitVariant<RowRefListWithFlags>(buildHashColsCount, operatorCount, joinType, buildSide);
    } else {
        hashTablesVariants = InitVariant<RowRefList>(buildHashColsCount, operatorCount, joinType, buildSide);
    }
}

template <class RowRefListType>
HashTableVariants *HashBuilderOperatorFactory::InitVariant(int32_t buildHashColsCount, int32_t operatorCount,
    JoinType joinType, BuildSide buildSide, bool isMultiCols)
{
    if (buildHashColsCount == 1) {
        auto type = buildTypes.GetIds()[buildHashCols[0]];
        switch (type) {
            case OMNI_BOOLEAN:
            case OMNI_BYTE:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int8_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_SHORT:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int16_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_FLOAT:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int32_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DOUBLE:
            case OMNI_DATE64:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<int64_t, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_DECIMAL128:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<Decimal128, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
            case OMNI_ARRAY:
            default:
                return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<StringRef, RowRefListType>>,
                    operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
        }
    } else {
        int32_t lengthCount = GetTypeLength(buildHashColsCount, buildTypes, buildHashCols);
        if (0 < lengthCount && lengthCount <= BITS_OF_INT) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int32_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        } else if (BITS_OF_INT < lengthCount && lengthCount <= BITS_OF_LONG) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int64_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        } else if (BITS_OF_LONG < lengthCount && lengthCount <= BITS_OF_LONGLONG) {
            return new HashTableVariants{std::in_place_type<JoinHashTableVariants<int128_t, RowRefListType>>,
                operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide, true };
        }
        return new HashTableVariants{ std::in_place_type<JoinHashTableVariants<StringRef, RowRefListType>>,
            operatorCount, &(this->buildTypes), this->buildHashCols, joinType, buildSide };
    }
}

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols)
{
    int32_t lengthCount = 0;
    for (int i = 0; i < buildHashColsCount; i++) {
        switch (buildTypes.GetIds()[buildHashCols[i]]) {
            case OMNI_BOOLEAN:
            case OMNI_BYTE:
                lengthCount += BITS_OF_BYTE;
                break;
            case OMNI_SHORT:
                lengthCount += BITS_OF_SHORT;
                break;
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_FLOAT:
                lengthCount += BITS_OF_INT;
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DOUBLE:
            case OMNI_DATE64:
                lengthCount += BITS_OF_LONG;
                break;
            default:
                return NOT_EXPECTED_TYPE;
        }
    }
    return lengthCount;
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(JoinType joinType,
    const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount)
{
    return new HashBuilderOperatorFactory(joinType, buildTypes, buildHashCols, buildHashColsCount, operatorCount);
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(JoinType joinType,
    BuildSide buildSide, const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount,
    int32_t operatorCount)
{
    return new HashBuilderOperatorFactory(joinType, buildSide, buildTypes, buildHashCols, buildHashColsCount,
        operatorCount);
}

HashBuilderOperatorFactory *HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode)
{
    // Extract necessary information from planNode
    auto joinType = planNode->GetJoinType();
    auto buildSide = planNode->GetBuildSide();
    auto buildTypes = planNode->RightOutputType();

    auto buildKeysSize = planNode->RightKeys().size();
    std::vector<int32_t> buildHashCols;
    for (size_t index = 0; index < buildKeysSize; index++) {
        auto key = dynamic_cast<FieldExpr *>(planNode->RightKeys()[index]);
        buildHashCols.emplace_back(key->colVal);
    }

    auto buildHashColsCount = (int32_t) buildHashCols.size();
    return new HashBuilderOperatorFactory(joinType, buildSide, *buildTypes, buildHashCols.data(), buildHashColsCount, 1);
}

Operator *HashBuilderOperatorFactory::CreateOperator()
{
    int32_t partitionIndex =
        operatorIndex++ % std::visit([&](auto &&arg) { return arg.GetHashTableCount(); }, *hashTablesVariants);
    return new HashBuilderOperator(this->buildTypes, hashTablesVariants, partitionIndex);
}

HashBuilderOperator::HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables,
    int32_t partitionIndex)
    : buildTypes(buildTypes), partitionIndex(partitionIndex), hashTablesVariants(hashTables)
{
    SetOperatorName(opNameForHashBuilder);
}

int32_t HashBuilderOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    auto rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    UpdateAddInputInfo(rowCount);
    std::visit([&](auto &&arg) { arg.AddVecBatch(partitionIndex, vecBatch); }, *hashTablesVariants);
    return 0;
}

int32_t HashBuilderOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (this->isFinished()) {
        return 0;
    }
    std::visit(
        [&](auto &&arg) {
            arg.Prepare(partitionIndex);
            arg.BuildHashTable(partitionIndex);
        },
        *hashTablesVariants);
    if (UNLIKELY(IsDebugEnable())) {
        int32_t hashTableSize = 0;
        auto hasgTableType =
            std::visit([&](auto &&arg) { return arg.GetHashTableTypes(partitionIndex); }, *hashTablesVariants);
        if (hasgTableType == HashTableImplementationType::NORMAL_HASH_TABLE) {
            hashTableSize = std::visit([&](auto &&arg) { return arg.GetHashTable(partitionIndex)->GetElementsSize(); },
                *hashTablesVariants);
        } else if (hasgTableType == HashTableImplementationType::ARRAY_HASH_TABLE) {
            hashTableSize = std::visit([&](auto &&arg) { return arg.GetArrayTable(partitionIndex)->GetElementsSize(); },
                *hashTablesVariants);
#ifdef SVEHT
        } else if (hasgTableType == HashTableImplementationType::SVE_HASH_TABLE32) {
            hashTableSize = std::visit(
                [&](auto &&arg) {
                    auto &sveHashTable = arg.GetSve32HashTable(partitionIndex);
                    return sveHashTable == nullptr ? 0 : sveHashTable->GetElementsSize();
                },
                *hashTablesVariants);
#endif
        }
        UpdateGetOutputInfo(hashTableSize);
    }
    SetStatus(OMNI_STATUS_FINISHED);
    std::visit([&](auto &&arg) { arg.SetStatus(OMNI_STATUS_FINISHED); }, *hashTablesVariants);
    return 0;
}

OmniStatus HashBuilderOperator::Close()
{
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // end of op
} // end of omniruntime
