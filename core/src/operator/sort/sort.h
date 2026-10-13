/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */
#ifndef __SORT_H__
#define __SORT_H__

#include <atomic>
#include <vector>
#include <memory>
#include "plannode/planNode.h"
#include "util/error_code.h"
#include "operator/operator_factory.h"
#include "operator/config/operator_config.h"
#include "operator/pages_index.h"
#include "operator/spill/spiller.h"
#include "operator/spill/spill_merger.h"
#include "operator/spillable_operator.h"
#include "type/data_types.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
class SortOperatorFactory : public OperatorFactory {
public:
    SortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols, int32_t outputColCount,
        int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
        const OperatorConfig &operatorConfig);

    SortOperatorFactory(const type::DataTypes &dataTypes, std::vector<int32_t> outputCols,
        std::vector<int32_t> sortCols, std::vector<int32_t> sortAscendings, std::vector<int32_t> sortNullFirsts,
        const OperatorConfig &&operatorConfig);

    ~SortOperatorFactory() override;

    static SortOperatorFactory *CreateSortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols,
        int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount);

    static SortOperatorFactory *CreateSortOperatorFactory(const type::DataTypes &dataTypes, int32_t *outputCols,
        int32_t outputColCount, int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortColCount, const OperatorConfig &operatorConfig);

    static SortOperatorFactory *CreateSortOperatorFactory(std::shared_ptr<const OrderByNode> planNode,
        const config::QueryConfig &queryConfig);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

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

class SortOperator : public Operator, public SpillableOperator {
public:
    // Pool-bound ctor chains Operator(pool)
    SortOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const type::DataTypes &dataTypes,
        std::vector<int32_t> &outputCols, std::vector<int32_t> &sortCols,
        std::vector<int32_t> &sortAscendings, std::vector<int32_t> &sortNullFirsts,
        const OperatorConfig &operatorConfig);

    ~SortOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    uint64_t GetSpilledBytes() override;

    OmniStatus Init() override;
    uint64_t reclaimableBytes() const override;
    int64_t reclaim(int64_t targetBytes) override;
    bool isInNonReclaimableSection() const noexcept override
    {
        return Operator::isInNonReclaimableSection();
    }
    Operator *operatorBase() noexcept override { return this; }
    const char *reclaimableName() const noexcept override { return "SortOperator"; }

    PagesIndex *GetPagesIndex() const
    {
        return pagesIndex.get();
    }

private:
    // Shared init body for legacy + pool-bound ctors.
    void InitFromCtor(const type::DataTypes &dataTypes);
    int64_t EstimateInputReservation(const omniruntime::vec::VectorBatch *vecBatch) const;
    void EnsureReservationPctsCached();
    void ReserveForInput(const omniruntime::vec::VectorBatch *vecBatch);

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
    std::atomic<bool> reclaimInProgress_{false};

    // QueryConfig is immutable for a query's lifetime (QueryConfig.h). Cache the
    // clamped reservation percentages once instead of copying QueryConfig and
    // re-parsing the config map on every AddInput. Defaults mirror QueryConfig's
    // kDefaultPct values; EnsureReservationPctsCached() fills them on first use.
    int32_t minSpillableReservationPct_{5};
    int32_t spillableReservationGrowthPct_{10};
    bool reservationPctsCached_{false};
};
} // end of namespace op
} // end of namespace omniruntime
#endif
