/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: Hash Aggregation WithExpr Header
 */


#ifndef OMNI_RUNTIME_AGG_UTIL_H
#define OMNI_RUNTIME_AGG_UTIL_H

#include "vector/vector_common.h"
#include "operator/execution_context.h"
#include "operator/filter/filter_and_project.h"
#include "operator/util/operator_util.h"

namespace omniruntime {
namespace op {
class AggUtil {
public:
    static bool IsAggPositionEligible(int32_t rowId, VectorBatch *inputVecBatch, SimpleFilter *aggSimpleFilter,
        ExecutionContext *executionContext, DataTypes &originTypes)
    {
        const int32_t allColsCount = inputVecBatch->GetVectorCount();
        auto originTypeIds = originTypes.GetIds();
        int64_t values[allColsCount];
        bool nulls[allColsCount];
        int32_t lengths[allColsCount];
        std::set<int32_t> &usedVectors = aggSimpleFilter->GetVectorIndexes();
        for (auto iter = usedVectors.begin(); iter != usedVectors.end(); ++iter) {
            auto vecIdx = *iter;
            auto vector = inputVecBatch->Get(vecIdx);
            nulls[vecIdx] = vector->IsNull(rowId);
            values[vecIdx] = OperatorUtil::GetValuePtrAndLength(vector, rowId, lengths + vecIdx, originTypeIds[vecIdx]);
        }

        return aggSimpleFilter->Evaluate(values, nulls, lengths, reinterpret_cast<int64_t>(executionContext));
    }

    static inline bool IsColumnFilter(VectorBatch *inputVecBatch, SimpleFilter *aggSimpleFilter)
    {
        return aggSimpleFilter->IsColumnFilter() &&
               inputVecBatch->Get(*(aggSimpleFilter->GetVectorIndexes().begin()))->GetEncoding() == OMNI_FLAT;
    }

    static BaseVector *ColumnFilterFlatVectorSliceHelper(VectorBatch *inputVecBatch, SimpleFilter *aggSimpleFilter)
    {
        auto *sourceVector = inputVecBatch->Get(*aggSimpleFilter->GetVectorIndexes().begin());
        return reinterpret_cast<Vector<bool> *>(sourceVector)->Slice(0, sourceVector->GetSize());
    }

    static VectorBatch *AggFilterRequiredVectors(VectorBatch *inputVecBatch, const DataTypes &originTypes,
        const DataTypes &inputTypes, const std::vector<std::unique_ptr<Projection>> &projections,
        ExecutionContext *executionContext)
    {
        int32_t rowCount = inputVecBatch->GetRowCount();
        auto newInputVecBatch = std::make_unique<VectorBatch>(rowCount);
        if (rowCount == 0) {
            VectorHelper::AppendVectors(newInputVecBatch.get(), inputTypes, rowCount, executionContext->GetPool());
            return newInputVecBatch.release();
        }

        int32_t originVecCount = inputVecBatch->GetVectorCount();
        int64_t valueAddrs[originVecCount];
        int64_t nullAddrs[originVecCount];
        int64_t offsetAddrs[originVecCount];
        int64_t dictionaryVectors[originVecCount];
        bool hasSetAddr = false;

        for (auto &projection : projections) {
            BaseVector *projectVec = nullptr;
            if (executionContext->queryConfig().PreferVectorizationExpression() &&
                projection->GetExpr()->supportVectorized()) {
                projectVec = projection->ProjectVec(inputVecBatch, executionContext);
            } else {
                if (!hasSetAddr) {
                    GetAddr(*inputVecBatch, valueAddrs, nullAddrs, offsetAddrs, dictionaryVectors, originTypes);
                    hasSetAddr = true;
                }
                projectVec = projection->Project(inputVecBatch, valueAddrs, nullAddrs, offsetAddrs, executionContext,
                    dictionaryVectors, originTypes.GetIds());
            }
            if (executionContext->HasError()) {
                executionContext->GetArena()->Reset();
                std::string errorMessage = executionContext->GetError();
                throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
            }
            newInputVecBatch->Append(projectVec);
        }
        return newInputVecBatch.release();
    }

    static void AddFilterColumn(VectorBatch *inputVecBatch, VectorBatch *newInputVecBatch,
        std::vector<SimpleFilter *> &aggSimpleFilters, ExecutionContext *context, DataTypes &originTypes)
    {
        auto aggFilterNum = aggSimpleFilters.size();
        auto rowCount = inputVecBatch->GetRowCount();

        for (size_t i = 0; i < aggFilterNum; ++i) {
            auto aggSimpleFilter = aggSimpleFilters[i];
            if (aggSimpleFilter != nullptr) {
                if (IsColumnFilter(inputVecBatch, aggSimpleFilter)) {
                    newInputVecBatch->Append(ColumnFilterFlatVectorSliceHelper(inputVecBatch, aggSimpleFilter));
                } else {
                    // if the agg expression has filter, then append a boolean vector to vector batch
                    auto *booleanVector = new Vector<bool>(rowCount, context->GetPool());
                    for (int32_t j = 0; j < rowCount; ++j) {
                        if (AggUtil::IsAggPositionEligible(j, inputVecBatch, aggSimpleFilter, context, originTypes)) {
                            booleanVector->SetValue(j, true);
                        } else {
                            booleanVector->SetValue(j, false);
                        }
                    }
                    newInputVecBatch->Append(booleanVector);
                }
            }
        }
    }
};
}
}


#endif // OMNI_RUNTIME_AGG_UTIL_H
