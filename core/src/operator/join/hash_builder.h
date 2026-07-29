/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: hash builder implementations
 */
#ifndef __HASH_BUILDER_H__
#define __HASH_BUILDER_H__
#define OMNI_USE_TAPER_JOIN

#include <memory>

#include "plannode/planNode.h"
#include "operator/operator_factory.h"
#include "operator/operator.h"
#include "join_hash_table_variants.h"
#include "common_join.h"
#include "operator/join/taper_join_hash_table_variants.h"

namespace omniruntime {
namespace op {
class HashBuilderOperatorFactory : public OperatorFactory {
public:
    HashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes, const int32_t *buildHashCols,
        int32_t buildHashColsCount, int32_t operatorCount);
    HashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    ~HashBuilderOperatorFactory()
    {
        delete hashTablesVariants;
    }
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
        const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(JoinType joinType, BuildSide buildSide,
        const DataTypes &buildTypes, const int32_t *buildHashCols, int32_t buildHashColsCount, int32_t operatorCount);
    static HashBuilderOperatorFactory *CreateHashBuilderOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode);
    omniruntime::op::Operator *CreateOperator() override;

    HashTableVariants *GetHashTablesVariants()
    {
        return hashTablesVariants;
    }

private:
    DataTypes buildTypes;
    std::vector<int32_t> buildHashCols;
    HashTableVariants *hashTablesVariants;
    std::atomic<int32_t> operatorIndex;

    template <bool NeedVisited>
    HashTableVariants* InitTaperVariant(int32_t buildHashColsCount, int32_t operatorCount, JoinType joinType,
                                        BuildSide buildSide = OMNI_BUILD_UNKNOWN, bool isMultiCols = false);

    template <class RowRefListType>
    HashTableVariants *InitVariant(int32_t buildHashColsCount, int32_t operatorCount, JoinType joinType,
        BuildSide buildSide = OMNI_BUILD_UNKNOWN, bool isMultiCols = false);
};

class HashBuilderOperator : public Operator {
public:
    HashBuilderOperator(const DataTypes &buildTypes, HashTableVariants *hashTables, int32_t partitionIndex);

    ~HashBuilderOperator() = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    // for test
    DataTypes &GetBuildTypes()
    {
        return buildTypes;
    }

    HashTableImplementationType GetHashTableType()
    {
        return std::visit(
            [&](auto &&arg) -> HashTableImplementationType { return arg.GetHashTableTypes(partitionIndex); },
            *hashTablesVariants);
    }

    uint32_t GetHashTableSize()
    {
        return std::visit([&](auto &&arg) -> uint32_t { return arg.GetHashTableSize(); }, *hashTablesVariants);
    }

    uint32_t GetHashTableCount()
    {
        return std::visit([&](auto &&arg) -> uint32_t { return arg.GetHashTableCount(); }, *hashTablesVariants);
    }

private:
    DataTypes buildTypes;
    int32_t partitionIndex;
    HashTableVariants *hashTablesVariants;
};

int32_t GetTypeLength(int buildHashColsCount, DataTypes& buildTypes, std::vector<int32_t>& buildHashCols);

template <bool NeedVisited>
HashTableVariants* HashBuilderOperatorFactory::InitTaperVariant(int32_t buildHashColsCount,
    int32_t operatorCount, JoinType joinType, BuildSide buildSide, bool /*isMultiCols*/) {
    if (buildHashColsCount == 1) {
        auto type = buildTypes.GetIds()[buildHashCols[0]];
        switch (type) {
            case OMNI_BOOLEAN:
            case OMNI_BYTE:
                return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int8_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
            case OMNI_SHORT:
                return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int16_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
            case OMNI_INT:
            case OMNI_DATE32:
            case OMNI_TIME32:
            case OMNI_FLOAT:
                return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int32_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
            case OMNI_DOUBLE:
            case OMNI_TIME64:
            case OMNI_DATE64:
                return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
            case OMNI_VARCHAR:
            case OMNI_CHAR:
            case OMNI_VARBINARY: {
                auto* var = new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
                std::visit([&](auto&& v) { v.SetSerMode(); }, *var);
                return var;
            }
            case OMNI_DECIMAL128: {
                auto* var = new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
                std::visit([&](auto&& v) { v.SetSerMode(); }, *var);
                return var;
            }
            case OMNI_ARRAY:
            case OMNI_MAP:
            case OMNI_ROW:
            default:
                throw omniruntime::exception::OmniException("TAPER_NOT_SUPPORTED",	 
                    "TAPER join does not support single-column key type "	 
                    + std::to_string(static_cast<int>(type)));
        }
    }
    // Multi-column: bit-pack key columns (consistent with agg packed mode).
    // Fixed path for keys ≤ 64 total bits; wider keys fall back to serialized mode.
    if (buildHashColsCount > 1) {
        int32_t totalBits = 0;
        for (int32_t i = 0; i < buildHashColsCount; ++i) {
            auto typeId = buildTypes.GetIds()[buildHashCols[i]];
            uint8_t bits = TaperJoinHashTableVariants<int32_t, NeedVisited>::PackedBitWidth(typeId);
            if (bits == 0) {
                // VARCHAR/unsupported type in multi-col → ser mode
                auto* var = new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
                    operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
                std::visit([&](auto&& v) { v.SetSerMode(); }, *var);
                return var;
            }
            totalBits += bits;
        }
        totalBits += buildHashColsCount; // +1 null-bit per column
        if (totalBits > 0 && totalBits <= 32) {
            return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int32_t, NeedVisited>>,
                operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
        } else if (totalBits <= 64) {
            return new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
                operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
        }
        // Packed > 64 bits → ser mode
        auto* var = new HashTableVariants{std::in_place_type<TaperJoinHashTableVariants<int64_t, NeedVisited>>,
            operatorCount, &buildTypes, buildHashCols, joinType, buildSide};
        std::visit([&](auto&& v) { v.SetSerMode(); }, *var);
        return var;
    }
    return nullptr;
}

} // end of op
} // end of omniruntime
#endif
