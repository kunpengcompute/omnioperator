/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#ifndef OMNI_RUNTIME_TOPN_SORT_H
#define OMNI_RUNTIME_TOPN_SORT_H

#include "operator/operator_factory.h"
#include "operator/topn/topn.h"
#include "operator/hashmap/base_hash_map.h"
#include "type/string_ref.h"
#include "operator/hashmap/vector_marshaller.h"

namespace omniruntime::op {
using GetValueFunc = void *(*)(vec::BaseVector *inputVec, int32_t inputPos, int32_t &length);
using CompareOptimizeFunc = int32_t (*)(void *valuePtr, int32_t length, vec::BaseVector *right, int32_t rightPosition);
using CompareFunc = int32_t (*)(vec::BaseVector *left, int32_t leftPosition, vec::BaseVector *right,
    int32_t rightPosition);
using EqualFunc = bool (*)(vec::BaseVector *left, int32_t leftPosition, vec::BaseVector *right, int32_t rightPosition);
using CreateVectorFunc = BaseVector *(*)(BaseVector *inputVec, int32_t inputPos);
using SetValueFunc = void (*)(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos);

class PartitionValue {
public:
    explicit PartitionValue(int32_t vecBatchCount) : nextIndex(0), maxCapacity(2 * vecBatchCount)
    {
        vecBatches.resize(maxCapacity);
        rowIndexes.resize(maxCapacity);
    }
    void Enlarge()
    {
        if (nextIndex >= maxCapacity) {
            maxCapacity += maxCapacity;
            vecBatches.resize(maxCapacity);
            rowIndexes.resize(maxCapacity);
        }
    }
    ~PartitionValue() = default;
    int32_t nextIndex;
    int32_t maxCapacity;
    std::vector<VectorBatch *> vecBatches;
    std::vector<int32_t> rowIndexes;
};

class PartitionHash {
public:
    std::size_t operator () (const StringRef &key) const
    {
        // calculate hash
        return omniruntime::op::HashUtil::HashValue((int8_t *)key.data, key.size);
    }
};

class TopNSortOperator : public Operator {
public:
    TopNSortOperator(const type::DataTypes &sourceTypes, int32_t n, bool isStrictTopN,
        const std::vector<int32_t> &partitionCols, const std::vector<int32_t> &sortCols,
        const std::vector<int32_t> &sortAscendings, const std::vector<int32_t> &sortNullFirsts,
        const config::QueryConfig &queryConfig);

    ~TopNSortOperator() override = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:

    void Prepare(vec::BaseVector **inputVectors);

    void InsertNewValueOptimize(PartitionValue &value, vec::VectorBatch *inputVecBatch, vec::BaseVector **sortVectors,
        int32_t inputRowIdx);

    void InsertNewPartition(StringRef &key, vec::VectorBatch *inputVecBatch, int32_t inputRowIdx);

    void InsertNewValue(PartitionValue &value, vec::VectorBatch *inputVecBatch, vec::BaseVector **sortVectors,
        int32_t inputRowIdx);

    void UpdatePartitionValueOptimize(PartitionValue &value, vec::VectorBatch *inputVecBatch,
        vec::BaseVector **sortVectors, int32_t inputRowIdx);

    void UpdatePartitionValue(PartitionValue &value, vec::VectorBatch *inputVecBatch, vec::BaseVector **sortVectors,
        int32_t inputRowIdx);

    int32_t CompareForSortColsOptimize(void *valuePtr, int32_t length, VectorBatch *vecBatch, int32_t rowIndex)
    {
        auto rightVec = vecBatch->Get(sortCols[0]);
        auto leftNull = valuePtr == nullptr;
        auto rightNull = rightVec->IsNull(rowIndex);
        auto sortNullFirst = sortNullFirsts[0];

        if (leftNull && rightNull) {
            // both left and right are null
            return 0;
        } else if (leftNull) {
            // left is null, but right is not null
            auto result = sortNullFirst ? -1 : 1;
            return result;
        } else if (rightNull) {
            // left is not null, but right is null
            auto result = sortNullFirst ? 1 : -1;
            return result;
        } else {
            // both left and right are not null
            auto result = sortCompareOptimizeFuncs[0](valuePtr, length, rightVec, rowIndex);
            return sortAscendings[0] ? result : -result;
        }
    }

    int32_t CompareForSortCols(vec::BaseVector **insertSortVectors, int32_t insertRowIdx, VectorBatch *vecBatch,
        int32_t rowIndex)
    {
        int32_t result;
        for (int32_t sortColIdx = 0; sortColIdx < sortColNum; sortColIdx++) {
            auto leftVec = insertSortVectors[sortColIdx];
            auto rightVec = vecBatch->Get(sortCols[sortColIdx]);
            auto leftNull = leftVec->IsNull(insertRowIdx);
            auto rightNull = rightVec->IsNull(rowIndex);
            auto sortNullFirst = sortNullFirsts[sortColIdx];

            if (leftNull && rightNull) {
                // both left and right are null
                result = 0;
            } else if (leftNull) {
                // left is null, but right is not null
                result = sortNullFirst ? -1 : 1;
                break;
            } else if (rightNull) {
                // left is not null, but right is null
                result = sortNullFirst ? 1 : -1;
                break;
            } else {
                // both left and right are not null
                result = sortCompareFuncs[sortColIdx](leftVec, insertRowIdx, rightVec, rowIndex);
                if (result != 0) {
                    result = sortAscendings[sortColIdx] ? result : -result;
                    break;
                }
            }
        }
        return result;
    }

    bool CheckDistinctForLast(vec::VectorBatch *lastVecBatch, int32_t lastRowIndex,
        vec::VectorBatch *frontOfLastVecBatch, int32_t frontOfLastRowIdx)
    {
        for (int32_t i = 0; i < sortColNum; i++) {
            auto sortCol = sortCols[i];
            auto lastSortVec = lastVecBatch->Get(sortCol);
            auto frontOfLastSortVec = frontOfLastVecBatch->Get(sortCol);
            auto result = equalFuncs[i](lastSortVec, lastRowIndex, frontOfLastSortVec, frontOfLastRowIdx);
            if (!result) {
                return true;
            }
        }
        return false;
    }

    StringRef GeneratePartitionKey(BaseVector **partitionVectors, int32_t partitionColNum, int32_t rowIdx,
        mem::SimpleArenaAllocator &arenaAllocator);

    int32_t FindInsertPositionOptimize(void *ptr, int32_t length, std::vector<VectorBatch *> &vecBatches,
        std::vector<int32_t> &rowIndexes, int32_t position);

    int32_t FindInsertPosition(BaseVector **insertSortVectors, int32_t insertRowIdx,
        std::vector<VectorBatch *> &vecBatches, std::vector<int32_t> &rowIndexes, int32_t position);

    type::DataTypes sourceTypes;
    int32_t n;
    bool isStrictTopN;
    std::vector<int32_t> partitionCols;
    int32_t partitionColNum;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    std::vector<int32_t> sortColTypes;
    std::vector<GetValueFunc> sortGetValueFuncs;
    std::vector<CompareOptimizeFunc> sortCompareOptimizeFuncs;
    std::vector<CompareFunc> sortCompareFuncs;
    int32_t sortColNum;
    std::unordered_map<type::StringRef, PartitionValue *, PartitionHash> partitionedMap;
    std::vector<VectorSerializer> serializers;
    std::vector<EqualFunc> equalFuncs;
    std::vector<CreateVectorFunc> createVectorFuncs;
    std::vector<SetValueFunc> setOutputValueFuncs;          // this is for construct output from partitionMap
    int32_t maxRowCount = 0;
    std::unordered_map<type::StringRef, PartitionValue *, PartitionHash>::iterator currentIter;
    std::vector<vec::VectorBatch *> inputs;
};

class TopNSortOperatorFactory : public OperatorFactory {
public:
    TopNSortOperatorFactory(const type::DataTypes &sourceTypes, int32_t n, bool isStrictTopN,
        const std::vector<int32_t> &partitionCols, const std::vector<int32_t> &sortCols,
        const std::vector<int32_t> &sortAscendings, const std::vector<int32_t> &sortNullFirsts,
        const config::QueryConfig &queryConfig = config::QueryConfig{})
        : sourceTypes(sourceTypes),
          n(n),
          isStrictTopN(isStrictTopN),
          partitionCols(partitionCols),
          sortCols(sortCols),
          sortAscendings(sortAscendings),
          sortNullFirsts(sortNullFirsts)
    {
        this->queryConfig_ = queryConfig;
    }

    ~TopNSortOperatorFactory() override = default;

    Operator *CreateOperator() override
    {
        return new TopNSortOperator(sourceTypes, n, isStrictTopN, partitionCols, sortCols, sortAscendings,
            sortNullFirsts, queryConfig_);
    }

private:
    type::DataTypes sourceTypes;
    int32_t n;
    bool isStrictTopN;
    std::vector<int32_t> partitionCols;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
};
}
#endif // OMNI_RUNTIME_TOPN_SORT_H
