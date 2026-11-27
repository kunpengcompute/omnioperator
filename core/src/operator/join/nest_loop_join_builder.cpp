/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * @Description: nested loop join builder implementations
 */
#include <memory>
#include <vector>
#include "vector/vector_helper.h"
#include "nest_loop_join_builder.h"

namespace omniruntime {
namespace op {
NestedLoopJoinBuildOperatorFactory::NestedLoopJoinBuildOperatorFactory(DataTypes buildTypes, int32_t *buildOutputCols,
    int32_t buildOutputColsCount)
    : buildTypes(buildTypes),
      buildOutputCols(std::vector<int32_t>(buildOutputCols, buildOutputCols + buildOutputColsCount))
{}

NestedLoopJoinBuildOperatorFactory *NestedLoopJoinBuildOperatorFactory::CreateNestedLoopJoinBuildOperatorFactory(
    std::shared_ptr<const NestedLoopJoinNode> planNode)
{
    auto buildOutputTypes = planNode->RightOutputType();
    auto buildOutputColsCount = buildOutputTypes->GetSize();
    std::vector<int32_t> buildOutputCols;
    for (size_t index = 0; index < buildOutputColsCount; index++) {
        buildOutputCols.emplace_back(index);
    }
    return new NestedLoopJoinBuildOperatorFactory(*buildOutputTypes, buildOutputCols.data(), buildOutputColsCount);
}

VectorBatch *NestedLoopJoinBuildOperatorFactory::GetBuildVectorBatch()
{
    return this->vectorBatch.get();
}

DataTypes &NestedLoopJoinBuildOperatorFactory::GetBuildDataTypes()
{
    return this->buildTypes;
}

std::vector<int32_t> &NestedLoopJoinBuildOperatorFactory::GetbuildOutputCols()
{
    return this->buildOutputCols;
}

// Pool-bound override
Operator *NestedLoopJoinBuildOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    return new NestedLoopJoinBuildOperator(pool, this->vectorBatch, buildTypes);
}

// Pool-bound ctor — same body, chains Operator(pool).
NestedLoopJoinBuildOperator::NestedLoopJoinBuildOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    std::unique_ptr<VectorBatch> &vectorBatch, DataTypes &buildTypes)
    : Operator(pool), inputVectorBatch(vectorBatch), buildTypes(buildTypes)
{
    SetOperatorName(metricsNameNestedLoopJoinBuilder);
}

int32_t NestedLoopJoinBuildOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    UpdateAddInputInfo(vecBatch->GetRowCount());
    inputVectorBatches.emplace_back(vecBatch);
    inputRowCnt += vecBatch->GetRowCount();
    return 0;
}


template <type::DataTypeId typeId>
void CopyVectorToVector(BaseVector *destVector, int32_t index, BaseVector *sourceVector)
{
    using namespace omniruntime::type;
    using T = typename NativeType<typeId>::type;
    using VectorVarchar = Vector<LargeStringContainer<std::string_view>>;
    int32_t rows = sourceVector->GetSize();

    if (sourceVector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
        if (sourceVector->IsNull(0)) {
            for (int32_t i = 0; i < rows; i++) {
                destVector->SetNull(index + i);
            }
        } else {
            auto constVec = static_cast<vec::ConstVector<T> *>(sourceVector);
            T value = constVec->GetConstValue();
            if constexpr (std::is_same_v<T, std::string_view>) {
                auto dest = static_cast<VectorVarchar *>(destVector);
                for (int32_t i = 0; i < rows; i++) {
                    dest->SetValue(index + i, value);
                }
            } else {
                auto dest = static_cast<Vector<T> *>(destVector);
                for (int32_t i = 0; i < rows; i++) {
                    dest->SetValue(index + i, value);
                }
            }
        }
        return;
    }

    if constexpr (std::is_same_v<T, std::string_view>) {
        static_cast<VectorVarchar *>(destVector)->Append(sourceVector, index, rows);
    } else {
        static_cast<Vector<T> *>(destVector)->Append(sourceVector, index, rows);
    }
}

int32_t NestedLoopJoinBuildOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (this->isFinished()) {
        return 0;
    }

    this->inputVectorBatch = std::make_unique<VectorBatch>(inputRowCnt);
    auto *vectorBatchPtr = inputVectorBatch.get();
    if (!inputVectorBatches.empty()) {
        int32_t vecCnt = inputVectorBatches[0]->GetVectorCount();
        int32_t batchCnt = inputVectorBatches.size();
        for (int32_t i = 0; i < vecCnt; i++) {
            BaseVector *vector = inputVectorBatches[0]->Get(i);
            DataTypeId vectorDataTypeId = vector->GetTypeId();
            BaseVector *destVector = VectorHelper::CreateComplexVector(this->pool(), buildTypes.Get()[i].get(), inputRowCnt);
            int32_t index = 0;
            for (int32_t j = 0; j < batchCnt; j++) {
                BaseVector *sourceVector = inputVectorBatches[j]->Get(i);
                if (vectorDataTypeId == OMNI_ARRAY || vectorDataTypeId == OMNI_MAP || vectorDataTypeId == OMNI_ROW) {
                    VectorHelper::AppendVector(destVector, index, sourceVector, inputVectorBatches[j]->GetRowCount());
                } else {
                    DYNAMIC_TYPE_DISPATCH(CopyVectorToVector, vectorDataTypeId, destVector, index, sourceVector);
                }
                index += inputVectorBatches[j]->GetRowCount();
            }
            vectorBatchPtr->Append(destVector);
        }
        for (int32_t j = 0; j < batchCnt; j++) {
            VectorHelper::FreeVecBatch(inputVectorBatches[j]);
            inputVectorBatches[j] = nullptr;
        }
    } else {
        auto &buildTypeList = buildTypes.Get();
        for (const DataTypePtr &dataTypePtr : buildTypeList) {
            BaseVector *destVector = VectorHelper::CreateComplexVector(this->pool(), dataTypePtr.get(), 0);
            vectorBatchPtr->Append(destVector);
        }
    }
    UpdateGetOutputInfo(inputRowCnt);
    SetStatus(OMNI_STATUS_FINISHED);
    return 0;
}

OmniStatus NestedLoopJoinBuildOperator::Close()
{
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // namespace op
} // namespace omniruntime