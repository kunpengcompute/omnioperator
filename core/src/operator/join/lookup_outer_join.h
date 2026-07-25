/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 * @Description: lookup outer join implementations
 */
#ifndef __LOOKUP_OUTER_JOIN_H__
#define __LOOKUP_OUTER_JOIN_H__

#include <memory>
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "operator/util/operator_util.h"
#include "hash_builder.h"
#include "common_join.h"

namespace omniruntime {
namespace op {
class LookupOuterJoinOperatorFactory : public OperatorFactory {
public:
    LookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes, int32_t *probeOutputCols,
        int32_t probeOutputColsCount, int32_t *buildOutputCols, const type::DataTypes &buildOutputTypes,
        HashTableVariants *hashTables);
    LookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes, int32_t *probeOutputCols,
        int32_t probeOutputColsCount, int32_t *buildOutputCols, const type::DataTypes &buildOutputTypes,
        HashTableVariants *hashTables, BuildSide buildSide);
    ~LookupOuterJoinOperatorFactory() override;
    static LookupOuterJoinOperatorFactory *CreateLookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes,
        int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
        const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr);
    static LookupOuterJoinOperatorFactory *CreateLookupOuterJoinOperatorFactory(const type::DataTypes &probeTypes,
        int32_t *probeOutputCols, int32_t probeOutputColsCount, int32_t *buildOutputCols,
        const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr, BuildSide buildSide);
    static LookupOuterJoinOperatorFactory *CreateLookupOuterJoinOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode,
        HashBuilderOperatorFactory* hashBuilderOperatorFactory, const config::QueryConfig& queryConfig);
    Operator *CreateOperator() override;

private:
    DataTypes buildOutputTypes;
    std::vector<int32_t> buildOutputCols;
    DataTypes probeTypes;                 // all types for probe
    std::vector<int32_t> probeOutputCols; // output columns for probe
    HashTableVariants *hashTables;
    BuildSide buildSide = OMNI_BUILD_RIGHT;
};

class LookupOuterPositionIterator {
public:
    explicit LookupOuterPositionIterator(HashTableVariants *hashTables);

    void GetAllUnVisitedAddressFromSingleTable(std::vector<uint32_t> &hashTableIndexes,
        std::vector<std::pair<uint32_t, uint32_t>> &addresses);
    void GetAllUnVisitedAddressFromMultipleTables(std::vector<uint32_t> &hashTableIndexes,
        std::vector<std::pair<uint32_t, uint32_t>> &addresses);
    void Reset();

private:
    uint32_t currentHashTable;
    uint32_t currentPosition;
    HashTableVariants *hashTables;
};

class LookupOuterJoinOperator : public Operator {
public:
    LookupOuterJoinOperator(DataTypes &probeOutputTypes, std::vector<int32_t> &probeOutputCols,
        std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables);
    LookupOuterJoinOperator(DataTypes &probeOutputTypes, std::vector<int32_t> &probeOutputCols,
        std::vector<int32_t> &buildOutputCols, const type::DataTypes &buildOutputTypes, HashTableVariants *hashTables, BuildSide buildSide);
    ~LookupOuterJoinOperator() override;
    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;
    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;
    void AppendAllUnvisitedRows(VectorBatch *vectorBatch, const int32_t *buildOutputIds, int32_t buildOutputColsCount,
        int32_t probeOutputColsCount, int rowCount);
    void PrepareAllUnvisitedRows();
    void PrepareTotalVisitedCounts();

private:
    void BuildVecBatch(VectorBatch *vectorBatch);

    DataTypes probeOutputTypes;
    std::vector<int32_t> probeOutputCols;
    std::vector<int32_t> buildOutputCols;
    DataTypes buildOutputTypes;
    HashTableVariants *hashTables;
    LookupOuterPositionIterator *iterator;
    int32_t outputColsCount = 0;
    int32_t maxRowCount = 0;
    int32_t totalRowCount = 0;
    int32_t outputtedRowCount = 0;
    bool visited = false;
    bool isPrepareTotalVisitedCounts = false;
    std::vector<uint32_t> hashTableIndexes;
    std::vector<std::pair<uint32_t, uint32_t>> addresses;
#ifdef OMNI_USE_TAPER_JOIN
    std::vector<char*> taperUnvisitedRowPtrs_;
#endif
    BuildSide buildSide = OMNI_BUILD_RIGHT;

    bool HasNext()
    {
        return outputtedRowCount < totalRowCount;
    }
};
} // end of op
} // end of omniruntime
#endif
