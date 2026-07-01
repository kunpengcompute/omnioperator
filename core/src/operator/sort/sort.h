/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */
#ifndef __SORT_H__
#define __SORT_H__

#include <vector>
#include <memory>
#include "plannode/planNode.h"
#include "util/error_code.h"
#include "operator/operator_factory.h"
#include "operator/config/operator_config.h"
#include "operator/pages_index.h"
#include "operator/spill/spiller.h"
#include "operator/spill/spill_merger.h"
#include "type/data_types.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
class SortOperatorFactory : public OperatorFactory {
public:
    SortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols, int32_t outputColCount,
        int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
        const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig = config::QueryConfig{});

    SortOperatorFactory(const type::DataTypes &dataTypes, std::vector<int32_t> outputCols,
        std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings, std::vector<int32_t> sortNullFirsts,
        const OperatorConfig &&operatorConfig, const config::QueryConfig &queryConfig = config::QueryConfig{});

    ~SortOperatorFactory() override;

    static SortOperatorFactory *CreateSortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols,
        int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount);

    static SortOperatorFactory *CreateSortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols,
        int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount, const OperatorConfig &operatorConfig,
        const config::QueryConfig &queryConfig = config::QueryConfig{});

    static SortOperatorFactory *CreateSortOperatorFactory(std::shared_ptr<const OrderByNode> planNode,
        const config::QueryConfig &queryConfig);

    Operator *CreateOperator() override;

    int32_t GetSourceTypeCount()
    {
        return sourceTypes.GetSize();
    }

    int32_t *GetOutputCols()
    {
        return outputCols.data();
    }

    int32_t GetOutputColCount()
    {
        return outputCols.size();
    }

    int32_t *GetSortCols()
    {
        return sortCols.data();
    }

    int32_t *GetSortAscendings()
    {
        return sortAscendings.data();
    }

    int32_t *GetSortNullFirsts()
    {
        return sortNullFirsts.data();
    }

    int32_t GetSortColCount()
    {
        return sortCols.size();
    }

private:
    DataTypes sourceTypes;
    std::vector<int32_t> outputCols;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    OperatorConfig operatorConfig;
};

class SortOperator : public Operator {
public:
    SortOperator(const type::DataTypes &dataTypes, std::vector<int32_t> &outputCols, std::vector<int32_t> &sortCols,
        std::vector<int32_t> &sortAscendings, std::vector<int32_t> &sortNullFirsts,
        const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig);

    ~SortOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    uint64_t GetSpilledBytes() override;

    PagesIndex *GetPagesIndex() const
    {
        return pagesIndex.get();
    }

private:
    void PrepareSort();
    ErrorCode SpillToDisk();
    void Sort();
    bool CanUseRadixSort();
    bool CanUseRadixSortByRuntimeInfo();
    void GetOutputFromMemory(VectorBatch **outputVecBatch);
    void GetOutputFromDisk(vec::VectorBatch **outputVecBatch);
    void SetSpillOutputVecBatch(vec::VectorBatch *outputVecBatch, int32_t &rowOffset, int32_t rowCount);
    template <typename T>
    void SetSpillOutputVector(vec::BaseVector *outputVector, int32_t outputRowIdx, int32_t outputRowCount,
        int32_t outputCol);
    void SetSpillOutputArrayVector(vec::BaseVector *outputVector, int32_t outputRowIdx, int32_t outputRowCount,
        int32_t outputCol);

    bool hasSorted = false;
    bool canInplaceSort = false;
    bool canRadixSort = false;

    int32_t radixSortSizeThreshold = -1;
    int32_t maxRowCountPerBatch = 0;
    size_t totalRowCount = 0;
    size_t rowCountOutputted = 0;

    type::DataTypes sourceTypes;
    std::vector<int32_t> outputCols;
    std::vector<int32_t> sortCols;
    std::vector<int32_t> sortAscendings;
    std::vector<int32_t> sortNullFirsts;
    std::vector<DataTypePtr> outputTypes;

    std::unique_ptr<PagesIndex> pagesIndex;

    // for spill
    OperatorConfig operatorConfig;
    Spiller *spiller = nullptr;
    SpillMerger *spillMerger = nullptr;
    std::vector<vec::VectorBatch *> batches;
    std::vector<int32_t> rowIdxes;
    bool hasSpill = false;
    uint64_t spilledBytes = 0;
};
} // end of namespace op
} // end of namespace omniruntime
#endif
