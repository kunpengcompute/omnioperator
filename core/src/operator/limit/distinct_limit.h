/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 */
#ifndef __DISTINCT_LIMIT_H__
#define __DISTINCT_LIMIT_H__

#include <vector>
#include <unordered_map>
#include <memory>
#include "type/data_types.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/status.h"
#include "operator/hash_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "distinct_state_func.h"

namespace omniruntime {
namespace op {
class DistinctLimitOperatorFactory : public OperatorFactory {
public:
    DistinctLimitOperatorFactory(const type::DataTypes &sourceTypes, const int32_t *distinctCols,
        int32_t distinctColsCount, int32_t hashCol, int64_t limit);

    ~DistinctLimitOperatorFactory() override;

    static DistinctLimitOperatorFactory *CreateDistinctLimitOperatorFactory(const type::DataTypes &inSourceTypes,
        const int32_t *inDistinctCols, int32_t inDistinctColsCount, int32_t inHashColumn, int64_t inLimit);

    Operator *CreateOperator() override;

private:
    type::DataTypes sourceTypes;
    std::vector<int32_t> distinctCols;
    int32_t distinctColsCount;
    int32_t hashCol;
    int64_t limit;
};

using DistinctRowInfo = struct RowInfo {
    uint64_t hashValue; // hash value of the row
    int32_t slotIndex;  // index when hash conflict
};

using DuplicateValueFunc = void (*)(ValueState &distinctSlot, BaseVector *inputVector, uint32_t rowIndex,
    ExecutionContext *context);
using GenerateHashFunc = void (*)(BaseVector *vector, const uint32_t rowCount, const int32_t *rowArray,
    uint64_t *combinedHash);
using GenerateHashFuncVect = void (*)(BaseVector *vector, const uint32_t start, const uint32_t rowCount,
    uint64_t *combinedHash);
using CheckEqualFunc = void (*)(BaseVector *vector, const uint32_t offset, const ValueState &slot, bool &isSame);
using FillOutputFunc = void (*)(VectorBatch *resultBatch, std::vector<ValueState> &rowVector, int32_t rowIndex,
    int32_t colIndex);

using DistinctLimitFuncSet = struct {
    type::DataTypeId dataTypeId;
    DuplicateValueFunc duplicateValueFunc;
    GenerateHashFunc generateHashFunc;
    GenerateHashFuncVect generateHashFuncVect;
    CheckEqualFunc checkEqualFunc;
    FillOutputFunc fillOutputFunc;
    // these functions are used to handle value from dict
    DuplicateValueFunc duplicateValueFuncFromDict;
    GenerateHashFunc generateHashFuncFromDict;
    GenerateHashFuncVect generateHashFuncVectFromDict;
    CheckEqualFunc checkEqualFuncFromDict;
    FillOutputFunc fillOutputFuncFromDict;
};

class DistinctLimitOperator : public Operator {
public:
    static constexpr int32_t INVALID_DISTINCT_COL_ID = -1;

public:
    DistinctLimitOperator(const type::DataTypes &sourceTypes, std::vector<int32_t> &distinctCols,
        int32_t distinctColsCount, int32_t hashCol, int64_t limit);

    ~DistinctLimitOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    /// DistinctLimit can reach isFinished() before noMoreInput (when remainingLimit <= 0).
    bool isEarlyFinish() const override
    {
        return true;
    }

private:
    void FillDistinctedTuple(vec::VectorBatch *vectorBatch, int32_t rowIndex, std::vector<ValueState> &tuple,
        ExecutionContext *executionContextPtr);

    void InLoop(vec::VectorBatch *vectorBatch, const int32_t rowCount, const uint64_t *combineHashVal);

    void ReleaseRowInfo(std::vector<DistinctRowInfo *> &rowInfo);

private:
    // hashValue=>record vector with distinct
    std::unordered_map<uint64_t, std::vector<std::vector<ValueState>>, HashUtil> distinctedTable;
    std::vector<DistinctRowInfo *> distinctRowInfo; // info(hash value and conflict index) of all distinct records
    type::DataTypes sourceTypes;
    std::vector<DataTypePtr> outputTypes;
    std::vector<int32_t> distinctCols;
    int32_t distinctColsCount;
    int32_t hashCol;
    std::vector<int32_t> outCols; // include distinct cols, hash cols
    int32_t outColsCount;
    int64_t remainingLimit;
    int64_t limit;
    omniruntime::vec::VectorBatch *resultBatch;
};
}
}

#endif // __DISTINCT_LIMIT_H__
