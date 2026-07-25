/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 */

#ifndef OMNI_RUNTIME_TOPN_H
#define OMNI_RUNTIME_TOPN_H

#include <queue>
#include <memory>
#include <vector>
#include "unordered_set"
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "vector/vector_batch.h"
#include "type/data_types.h"

namespace omniruntime {
namespace op {
class RowComparator {
public:
    RowComparator(const int32_t *sourceTypes, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount, omniruntime::vec::VectorBatch *vectorBatch)
        : sourceTypes(sourceTypes),
          sortCols(sortCols),
          sortAscendings(sortAscendings),
          sortNullFirsts(sortNullFirsts),
          sortColCount(sortColCount),
          vectorBatch(vectorBatch)
    {}

    RowComparator(const int32_t *sourceTypes, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount)
        : RowComparator(sourceTypes, sortCols, sortAscendings, sortNullFirsts, sortColCount, nullptr)
    {}

    ~RowComparator();

    // input column data type
    const int32_t *GetSourceTypes() const;

    // get sort type, ascend or descend
    int32_t *GetSortAscendings() const;

    // is null row put in first when sort
    int32_t *GetSortNullFirsts() const;

    // get sort column num
    int32_t GetSortColCount() const;

    // get sort column id
    int32_t *GetSortCols() const;

    // input page in vector batch
    omniruntime::vec::VectorBatch *GetVecBatch() const;

private:
    const int32_t *sourceTypes;
    int32_t *sortCols = nullptr;
    int32_t *sortAscendings = nullptr;
    int32_t *sortNullFirsts = nullptr;
    int32_t sortColCount = 0;
    omniruntime::vec::VectorBatch *vectorBatch = nullptr;
};

bool operator < (const RowComparator &left, const RowComparator &right);

class TopNOperatorFactory : public OperatorFactory {
public:
    TopNOperatorFactory(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset,
        int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount);

    TopNOperatorFactory(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset,
        std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings, std::vector<int32_t> sortNullFirsts,
        int32_t sortColCount, const config::QueryConfig &queryConfig = config::QueryConfig{});

    ~TopNOperatorFactory() override;

    Operator *CreateOperator() override;

private:
    type::DataTypes sourceTypes;
    std::vector<int32_t> sortCols;
    int32_t limit = 0;
    int32_t offset = 0;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    int32_t sortColCount = 0;
};

class TopNOperator : public Operator {
public:
    TopNOperator(const type::DataTypes &sourceTypes, int32_t limit, int32_t offset, std::vector<int32_t> &sortCols,
        std::vector<int32_t> &sortAscendings, std::vector<int32_t> &sortNullFirsts, int32_t sortColCount,
        const config::QueryConfig &queryConfig);

    ~TopNOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vectorBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:
    type::DataTypes sourceTypes;
    int32_t sourceTypesCount = 0;
    std::vector<int32_t> sortCols;
    int32_t limit = 0;
    int32_t offset = 0;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    int32_t sortColCount = 0;
    int64_t maxRowCount = 0;
    int64_t totalRowCount = 0;
    int64_t outputtedRowCount = 0;
    bool hasFilledResult = false;
    std::priority_queue<RowComparator, std::vector<RowComparator>, std::less<>> pq;
    std::unordered_set<omniruntime::vec::VectorBatch *> singleRowVectorBatchSet;

    std::vector<omniruntime::vec::VectorBatch *> resultVectorBatchList;

    vec::VectorBatch *CreateSingleRowVecBatch(omniruntime::vec::VectorBatch *vectorBatch, int32_t position) const;

    void SetValueForVectorBatch(int32_t typeId, int64_t index, vec::BaseVector *pqVector,
        vec::BaseVector *tmpVector) const;

    void SetVarcharValueForVectorBatch(int64_t rowNum, vec::BaseVector *pqVector, vec::BaseVector *tmpVector) const;

    void SetArrayValueForVectorBatch(int64_t rowNum, vec::BaseVector *pqVector, vec::BaseVector *tmpVector) const;

    void SetMapValueForVectorBatch(int64_t rowNum, vec::BaseVector *pqVector, vec::BaseVector *tmpVector) const;

    void SetRowValueForVectorBatch(int64_t rowNum, vec::BaseVector *pqVector, vec::BaseVector *tmpVector) const;

    void UpdateSingleRowVectorBatch(vec::VectorBatch *vectorBatch, vec::VectorBatch *singleRowVecBatch,
        int32_t position) const;

    void FillResultVectorBatchList();

    bool HasNext()
    {
        return outputtedRowCount < totalRowCount;
    }
};
}
}
#endif // OMNI_RUNTIME_TOPN_H
