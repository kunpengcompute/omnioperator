/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * @Description: lookup join implementations
 */
#include <iostream>
#include <memory>
#include <vector>
#include "hash_builder.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"
#include "nest_loop_join_lookup.h"
#include "vectorization/ExprEval.h"
#include "nest_loop_join_builder.h"

using namespace omniruntime::vec;

namespace omniruntime::op {

NestLoopJoinLookupOperatorFactory::NestLoopJoinLookupOperatorFactory(JoinType joinType, DataTypes probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount, Filter *filter, DataTypes joinedTypes,
    int64_t buildOpFactoryAddr, OverflowConfig *overflowConfig)
    : probeTypes(probeTypes),
      joinType(joinType),
      probeOutputCols(std::vector<int32_t>(probeOutputCols, probeOutputCols + probeOutputColsCount)),
      filter(filter),
      joinedTypes(joinedTypes),
      buildOpFactoryAddr(buildOpFactoryAddr)
{}

NestLoopJoinLookupOperatorFactory::NestLoopJoinLookupOperatorFactory(JoinType joinType, DataTypes probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount, Filter *filter, DataTypes joinedTypes,
    int64_t buildOpFactoryAddr, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig, Expr *filterExpr)
    : NestLoopJoinLookupOperatorFactory(joinType, probeTypes, probeOutputCols, probeOutputColsCount, filter, joinedTypes, buildOpFactoryAddr, overflowConfig)
{
    this->queryConfig_ = queryConfig;
    this->filterExpr = filterExpr;
}

NestLoopJoinLookupOperatorFactory *NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
    JoinType &joinTypePtr, DataTypes &probeTypesPtr, int32_t *probeOutputColsPtr, int32_t probeOutputColsCount,
    Expr *filterExpr, int64_t buildOperatorFactoryAddr, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
{
    auto &probeTypesVector = probeTypesPtr.Get();
    auto nestedLoopJoinBuilderFactory =
        reinterpret_cast<NestedLoopJoinBuildOperatorFactory *>(buildOperatorFactoryAddr);
    auto &buildTypesVector = nestedLoopJoinBuilderFactory->GetBuildDataTypes().Get();
    std::vector<DataTypePtr> joinedTypeVector;
    joinedTypeVector.reserve((probeTypesVector.size() + buildTypesVector.size()));
    joinedTypeVector.insert(joinedTypeVector.end(), probeTypesVector.begin(), probeTypesVector.end());
    joinedTypeVector.insert(joinedTypeVector.end(), buildTypesVector.begin(), buildTypesVector.end());
    DataTypes joinedDataTypes(joinedTypeVector);
    Filter *filterPtr = nullptr;
    if (filterExpr != nullptr) {
        filterPtr = new Filter(*filterExpr, joinedDataTypes, overflowConfig);
        if (!filterPtr->IsSupported()) {
            delete filterPtr;
            filterPtr = nullptr;
            throw OmniException("FILTER IS NOT SUPPORTED", "the filter in this function is not support,please "
                "change another filter");
        }
    }
    auto pOperatorFactory = new NestLoopJoinLookupOperatorFactory(joinTypePtr, probeTypesPtr, probeOutputColsPtr, probeOutputColsCount,
        filterPtr, joinedDataTypes, buildOperatorFactoryAddr, overflowConfig, queryConfig, filterExpr);

    delete overflowConfig;
    overflowConfig = nullptr;
    return pOperatorFactory;
}

NestLoopJoinLookupOperatorFactory *NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
    std::shared_ptr<const NestedLoopJoinNode> planNode, NestedLoopJoinBuildOperatorFactory* builderOperatorFactory,
    const config::QueryConfig &queryConfig)
{
    auto overflowConfig = queryConfig.IsOverFlowASNull()
                          ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                          : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);

    auto joinType = planNode->GetJoinType();
    auto outputTypes = planNode->OutputType();
    Filter *filterPtr = nullptr;
    auto filterExpr = planNode->Filter();
    if (filterExpr != nullptr && !(queryConfig.PreferVectorizationExpression() && filterExpr->supportVectorized())) {
        filterPtr = new Filter(*filterExpr, *outputTypes, overflowConfig);
    }

    auto buildOutputTypes = planNode->RightOutputType();
    auto probeOutputTypes = planNode->LeftOutputType();
    auto probeOutputColsCount = probeOutputTypes->GetSize();
    std::vector<int32_t> probeOutputCols;
    for (size_t index = 0; index < probeOutputColsCount; index++) {
        probeOutputCols.emplace_back(index);
    }

    return new NestLoopJoinLookupOperatorFactory(joinType, *probeOutputTypes, probeOutputCols.data(),
        probeOutputColsCount, filterPtr, *outputTypes, (int64_t)builderOperatorFactory, overflowConfig, queryConfig, filterExpr);
}

Operator *NestLoopJoinLookupOperatorFactory::CreateOperator()
{
    auto nestedLoopJoinBuilderFactory = reinterpret_cast<NestedLoopJoinBuildOperatorFactory *>(buildOpFactoryAddr);
    auto pNestLoopLookupJoinOperator = new NestLoopJoinLookupOperator(joinType, probeOutputCols, filter, probeTypes,
        nestedLoopJoinBuilderFactory->GetBuildVectorBatch(), nestedLoopJoinBuilderFactory->GetBuildDataTypes(),
        nestedLoopJoinBuilderFactory->GetbuildOutputCols(), joinedTypes, queryConfig_, filterExpr);
    return pNestLoopLookupJoinOperator;
}

NestLoopJoinLookupOperatorFactory::~NestLoopJoinLookupOperatorFactory()
{
    delete filter;
    filter = nullptr;
}

NestLoopJoinLookupOperator::NestLoopJoinLookupOperator(JoinType joinType, std::vector<int32_t> &probeOutputCols,
    Filter *filter, DataTypes probeTypes, VectorBatch *buildVectorBatch, DataTypes buildTypes,
    std::vector<int32_t> &buildOutputCols, DataTypes joinedTypes)
    : buildVectorBatch(buildVectorBatch),
      buildOutputCols(buildOutputCols),
      joinType(joinType),
      probeOutputCols(probeOutputCols),
      filter(filter),
      joinedTypes(joinedTypes),
      buildTypes(buildTypes)
{
    SetOperatorName(metricsNameNestedLoopJoinLookup);
    int32_t leftRowSize =
        OperatorUtil::GetOutputRowSize(probeTypes.Get(), probeOutputCols.data(), probeOutputCols.size());
    int32_t rightRowSize =
        OperatorUtil::GetOutputRowSize(buildTypes.Get(), buildOutputCols.data(), buildOutputCols.size());
    int32_t outputRowSize = leftRowSize + rightRowSize;
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        outputRowSize != 0 ? outputRowSize : DEFAULT_ROW_SIZE, &executionContext->queryConfigRef());
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    joinedVectorBatch = std::make_unique<VectorBatch>(buildRowCount);
    selectedRows = new int32_t[buildRowCount];
    if ((joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) && this->filterExpr != nullptr) {
        buildMatchedRows.assign(buildRowCount, 0);
        for (int32_t j = 0; j < probeOutputCols.size(); j++) {
            DataTypeId vectorDataTypeId = probeTypes.GetType(probeOutputCols[j])->GetId();
            probeOutputDataTypeIds.push_back(vectorDataTypeId);
            probeOutputDataTypes.push_back(probeTypes.GetType(probeOutputCols[j]));
        }
    }
}

NestLoopJoinLookupOperator::NestLoopJoinLookupOperator(JoinType joinType, std::vector<int32_t> &probeOutputCols,
    Filter *filter, DataTypes probeTypes, VectorBatch *buildVectorBatch, DataTypes buildTypes,
    std::vector<int32_t> &buildOutputCols, DataTypes joinedTypes, const config::QueryConfig &queryConfig, Expr *filterExpr)
    : buildVectorBatch(buildVectorBatch),
      buildOutputCols(buildOutputCols),
      joinType(joinType),
      probeOutputCols(probeOutputCols),
      filter(filter),
      joinedTypes(joinedTypes),
      buildTypes(buildTypes)
{
    executionContext->SetConfig(queryConfig);
    this->filterExpr = filterExpr;
    SetOperatorName(metricsNameNestedLoopJoinLookup);
    int32_t leftRowSize =
        OperatorUtil::GetOutputRowSize(probeTypes.Get(), probeOutputCols.data(), probeOutputCols.size());
    int32_t rightRowSize =
        OperatorUtil::GetOutputRowSize(buildTypes.Get(), buildOutputCols.data(), buildOutputCols.size());
    int32_t outputRowSize = leftRowSize + rightRowSize;
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        outputRowSize != 0 ? outputRowSize : DEFAULT_ROW_SIZE, &executionContext->queryConfigRef());
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    joinedVectorBatch = std::make_unique<VectorBatch>(buildRowCount);
    selectedRows = new int32_t[buildRowCount];
    if ((joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) && this->filterExpr != nullptr) {
        buildMatchedRows.assign(buildRowCount, 0);
        for (int32_t j = 0; j < probeOutputCols.size(); j++) {
            DataTypeId vectorDataTypeId = probeTypes.GetType(probeOutputCols[j])->GetId();
            probeOutputDataTypeIds.push_back(vectorDataTypeId);
            probeOutputDataTypes.push_back(probeTypes.GetType(probeOutputCols[j]));
        }
    }
}

void NestLoopJoinLookupOperator::InitJoinedVectorBatch()
{
    int32_t probeVectorCount = curInputBatch->GetVectorCount();
    int32_t buildVectorCount = buildVectorBatch->GetVectorCount();
    auto *joinedVectorBatchPtr = joinedVectorBatch.get();
    joinedVectorBatchPtr->ResizeVectorCount(probeVectorCount + buildVectorCount);
    for (int32_t j = 0; j < probeVectorCount; j++) {
        joinedVectorBatchPtr->SetVector(j, nullptr);
    }
    for (int32_t j = 0; j < buildVectorCount; j++) {
        joinedVectorBatchPtr->SetVector(j + probeVectorCount, buildVectorBatch->Get(j));
    }
}

void NestLoopJoinLookupOperator::UpdateJoinedVectorBatch(int32_t probeRow)
{
    int32_t probeVectorCount = curInputBatch->GetVectorCount();
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    int32_t *probeRows = selectedRows;
    auto *joinedVectorBatchPtr = joinedVectorBatch.get();
    std::fill_n(probeRows, buildRowCount, probeRow);
    for (int32_t j = 0; j < probeVectorCount; j++) {
        BaseVector *preProbeVector = joinedVectorBatchPtr->Get(j);
        delete preProbeVector;
        joinedVectorBatchPtr->SetVector(j,
            VectorHelper::CopyPositionsVector(curInputBatch->Get(j), probeRows, 0, buildRowCount));
    }
}

int32_t NestLoopJoinLookupOperator::GetNumSelectedRows()
{
    int32_t numSelectedRows = 0;
    ExecutionContext *context = executionContext.get();
    auto *joinedVectorBatchPtr = joinedVectorBatch.get();
    if (executionContext->queryConfig().PreferVectorizationExpression() && filterExpr->supportVectorized()) {
        auto temp = context->GetResultRowSize();
        context->SetResultRowSize(joinedVectorBatchPtr->GetRowCount());
        auto joinedSnapshot = VectorHelper::CopyVectorBatch(joinedVectorBatch.get());
        ExprEval e(joinedSnapshot.get(), context);
        e.VisitExpr(*filterExpr);
        auto *outCol = e.GetResult();
        context->SetResultRowSize(temp);
        if (context->HasError()) {
            context->GetArena()->Reset();
            std::string errorMessage = context->GetError();
            delete outCol;
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
        for (int i = 0, j = 0; i < outCol->GetSize(); ++i){
            if (!outCol->IsNull(i) && VectorHelper::GetValueFromVector<bool>(outCol, i)){
                numSelectedRows++;
                selectedRows[j++] = i;
            }
        }
        delete outCol;
    } else {
        int32_t originVecCount = joinedVectorBatchPtr->GetVectorCount();
        int32_t rowCount = joinedVectorBatchPtr->GetRowCount();
        int64_t valueAddrs[originVecCount];
        int64_t nullAddrs[originVecCount];
        int64_t offsetAddrs[originVecCount];
        int64_t dictionaryVectors[originVecCount];
        GetAddr(*joinedVectorBatchPtr, valueAddrs, nullAddrs, offsetAddrs, dictionaryVectors, joinedTypes);
        numSelectedRows = filter->GetFilterFunc()(valueAddrs, rowCount, selectedRows, nullAddrs, offsetAddrs,
            reinterpret_cast<int64_t>(context), dictionaryVectors);
        if (context->HasError()) {
            context->GetArena()->Reset();
            std::string errorMessage = context->GetError();
            throw OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
    }

    return numSelectedRows;
}

void NestLoopJoinLookupOperator::ProbeInnerJoin(int32_t probeRowCount, int32_t resultOutputRows)
{
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    if (buildRowCount == 0) {
        curProbePosition = probeRowCount;
        return;
    }
    int32_t numSelectedRows;
    int32_t probeOutputRows = 0;
    for (int32_t i = curProbePosition; i < probeRowCount; i++) {
        UpdateJoinedVectorBatch(i);
        numSelectedRows = GetNumSelectedRows();
        for (int32_t j = 0; j < numSelectedRows; j++) {
            buildResultOutputRows.emplace_back(selectedRows[j]);
        }
        probeResultOutputRows.insert(probeResultOutputRows.end(), numSelectedRows, i);
        probeOutputRows += numSelectedRows;
        curProbePosition = i + 1;
        if (probeOutputRows >= resultOutputRows) {
            break;
        }
    }
}

void NestLoopJoinLookupOperator::ProbeCrossJoin(int32_t probeRowCount, int32_t buildRowCount, int32_t resultOutputRows)
{
    if (buildRowCount == 0) {
        curProbePosition = probeRowCount;
        return;
    }
    int32_t probeOutputRows = 0;
    for (int32_t i = curProbePosition; i < probeRowCount; i++) {
        for (int32_t j = 0; j < buildRowCount; j++) {
            buildResultOutputRows.emplace_back(j);
        }
        probeResultOutputRows.insert(probeResultOutputRows.end(), buildRowCount, i);
        probeOutputRows += buildRowCount;
        curProbePosition = i + 1;
        if (probeOutputRows >= resultOutputRows) {
            break;
        }
    }
}

void NestLoopJoinLookupOperator::ProbeOuterJoin(int32_t probeRowCount, int32_t resultOutputRows)
{
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    int32_t numSelectedRows;
    int32_t probeOutputRows = 0;
    if (buildRowCount == 0 && (joinType == OMNI_JOIN_TYPE_LEFT || joinType == OMNI_JOIN_TYPE_FULL)) {
        for (int32_t i = curProbePosition; i < probeRowCount; i++) {
            buildNullPosition.emplace_back(probeResultOutputRows.size());
            probeResultOutputRows.emplace_back(i);
            buildResultOutputRows.emplace_back(0);
            probeOutputRows++;
            curProbePosition = i + 1;
            if (probeOutputRows >= resultOutputRows) {
                break;
            }
        }
        return;
    }
    for (int32_t i = curProbePosition; i < probeRowCount; i++) {
        UpdateJoinedVectorBatch(i);
        numSelectedRows = GetNumSelectedRows();
        for (int32_t j = 0; j < numSelectedRows; j++) {
            buildResultOutputRows.emplace_back(selectedRows[j]);
        }
        if (joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) {
            for (int32_t j = 0; j < numSelectedRows; j++) {
                buildMatchedRows[selectedRows[j]] = 1;
            }
        }

        if (numSelectedRows > 0) {
            probeResultOutputRows.insert(probeResultOutputRows.end(), numSelectedRows, i);
            probeOutputRows += numSelectedRows;
        } else if (joinType == OMNI_JOIN_TYPE_LEFT || joinType == OMNI_JOIN_TYPE_FULL) {
            buildNullPosition.emplace_back(probeResultOutputRows.size());
            probeResultOutputRows.emplace_back(i);
            buildResultOutputRows.emplace_back(0);
            probeOutputRows++;
        }
        curProbePosition = i + 1;
        if (probeOutputRows >= resultOutputRows) {
            break;
        }
    }
}

void NestLoopJoinLookupOperator::PrepareCurrentProbe()
{
    int32_t probeRowCount = curInputBatch->GetRowCount();
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    int32_t resultOutputRows;
    if (buildRowCount == 0) {
        resultOutputRows = maxRowCount;
    } else if (buildRowCount >= maxRowCount) {
        resultOutputRows = buildRowCount;
    } else {
        resultOutputRows = std::min(maxRowCount / buildRowCount, probeRowCount - curProbePosition) * buildRowCount;
    }
    buildNullPosition.clear();
    probeResultOutputRows.clear();
    buildResultOutputRows.clear();
    if (this->filterExpr != nullptr) {
        switch (joinType) {
            case OMNI_JOIN_TYPE_INNER:
                ProbeInnerJoin(probeRowCount, resultOutputRows);
                break;
            case OMNI_JOIN_TYPE_LEFT:
            case OMNI_JOIN_TYPE_RIGHT:
            case OMNI_JOIN_TYPE_FULL:
                ProbeOuterJoin(probeRowCount, resultOutputRows);
                break;
            default:
                std::string omniExceptionInfo = "Error in ProcessProbe, no such join type " + std::to_string(joinType);
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    } else {
        ProbeCrossJoin(probeRowCount, buildRowCount, resultOutputRows);
    }
}

int32_t NestLoopJoinLookupOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    UpdateAddInputInfo(vecBatch->GetRowCount());
    curInputBatch = vecBatch;
    if (firstVecBatch) {
        firstVecBatch = false;
        InitJoinedVectorBatch();
    }
    curProbePosition = 0;
    SetStatus(OMNI_STATUS_NORMAL);
    return 0;
}

void NestLoopJoinLookupOperator::BuildOutput(VectorBatch **outputVecBatch)
{
    int32_t empty[0];
    int32_t buildRowCount = buildVectorBatch->GetRowCount();
    int32_t probeOutputColsSize = probeOutputCols.size();
    int32_t buildOutputColsSize = buildOutputCols.size();
    int32_t buildNullPositionSize = buildNullPosition.size();
    int32_t *dataPtr = probeResultOutputRows.data();
    int32_t *buildDataPtr = buildResultOutputRows.data();
    int32_t outputRows = probeResultOutputRows.size();
    auto output = std::make_unique<VectorBatch>(outputRows);
    if (outputRows == 0) {
        dataPtr = empty;
        buildDataPtr = empty;
    }
    VectorBatch *outputPtr = output.get();
    for (int32_t i = 0; i < probeOutputColsSize; i++) {
        BaseVector *outputVector =
            VectorHelper::CopyPositionsVector(curInputBatch->Get(probeOutputCols[i]), dataPtr, 0, outputRows);
        outputPtr->Append(outputVector);
    }
    if (buildNullPositionSize <= 0) {
        for (int32_t j = 0; j < buildOutputColsSize; j++) {
            BaseVector *outputVector = VectorHelper::CopyPositionsVector(buildVectorBatch->Get(buildOutputCols[j]),
                buildDataPtr, 0, outputRows);
            outputPtr->Append(outputVector);
        }
    } else if (buildRowCount == 0) {
        for (int32_t j = 0; j < buildOutputColsSize; j++) {
            BaseVector *vector = buildVectorBatch->Get(buildOutputCols[j]);
            BaseVector *outputVector = VectorHelper::CreateComplexVector(buildTypes.GetType(buildOutputCols[j]).get(), outputRows);
            for (int32_t i = 0; i < buildNullPositionSize; ++i) {
                outputVector->SetNull(buildNullPosition[i]);
            }
            outputPtr->Append(outputVector);
        }
    } else {
        for (int32_t j = 0; j < buildOutputColsSize; j++) {
            BaseVector *outputVector = VectorHelper::CopyPositionsVector(buildVectorBatch->Get(buildOutputCols[j]),
                buildDataPtr, 0, outputRows);
            for (int32_t i = 0; i < buildNullPositionSize; ++i) {
                outputVector->SetNull(buildNullPosition[i]);
            }
            outputPtr->Append(outputVector);
        }
    }
    *outputVecBatch = output.release();
}

void NestLoopJoinLookupOperator::BuildNotMatchOutput(VectorBatch **outputVecBatch)
{
    int32_t probeOutputColsSize = probeOutputCols.size();
    int32_t buildOutputColsSize = buildOutputCols.size();
    // build side not match rows
    buildResultOutputRows.clear();
    for (int32_t i = 0; i < buildMatchedRows.size(); ++i) {
        if (buildMatchedRows[i] == 0) {
            buildResultOutputRows.push_back(i);
        }
    }
    int32_t *buildDataPtr = buildResultOutputRows.data();
    int32_t outputRows = buildResultOutputRows.size();
    auto output = std::make_unique<VectorBatch>(outputRows);
    VectorBatch *outputPtr = output.get();

    // probe side set null
    for (int32_t j = 0; j < probeOutputColsSize; j++) {
        BaseVector *outputVector = VectorHelper::CreateComplexVector(probeOutputDataTypes[probeOutputCols[j]].get(), outputRows);
        for (int32_t i = 0; i < outputRows; ++i) {
            outputVector->SetNull(i);
        }
        outputPtr->Append(outputVector);
    }

    // build side copy origin rows
    for (int32_t j = 0; j < buildOutputColsSize; j++) {
        BaseVector *outputVector = VectorHelper::CopyPositionsVector(buildVectorBatch->Get(buildOutputCols[j]),
                                                                     buildDataPtr, 0, outputRows);
        outputPtr->Append(outputVector);
    }
    *outputVecBatch = output.release();
}

int32_t NestLoopJoinLookupOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (curInputBatch == nullptr) {
        if (noMoreInput_ && !buildMatchedRows.empty()) {
            BuildNotMatchOutput(outputVecBatch);
            if ((*outputVecBatch != nullptr)) {
                UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
            } else {
                UpdateGetOutputInfo(0);
            }
            buildMatchedRows.clear();
        }
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
    auto inputRowCount = curInputBatch->GetRowCount();
    if (curProbePosition < inputRowCount) {
        PrepareCurrentProbe();
        executionContext->GetArena()->Reset();
    }
    BuildOutput(outputVecBatch);
    if (curProbePosition >= inputRowCount || probeResultOutputRows.empty()) {
        VectorHelper::FreeVecBatch(curInputBatch);
        curInputBatch = nullptr;
        curProbePosition = 0;

        if (noMoreInput_ && buildMatchedRows.empty()) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
    }
    if ((*outputVecBatch != nullptr)) {
        UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    } else {
        UpdateGetOutputInfo(0);
    }
    return 0;
}

NestLoopJoinLookupOperator::~NestLoopJoinLookupOperator()
{
    if (curInputBatch != nullptr) {
        VectorHelper::FreeVecBatch(curInputBatch);
        curInputBatch = nullptr;
    }

    // the buildVector is shared and lookup operator dont release them
    auto *joinedVectorBatchPtr = joinedVectorBatch.get();
    if (!firstVecBatch) {
        int32_t joinedVectorBatchVecCnt = joinedVectorBatchPtr->GetVectorCount();
        int32_t probeVectorCnt = joinedVectorBatchVecCnt - buildVectorBatch->GetVectorCount();
        for (int32_t i = probeVectorCnt; i < joinedVectorBatchVecCnt; i++) {
            joinedVectorBatchPtr->SetVector(i, nullptr);
        }
        joinedVectorBatchPtr->ResizeVectorCount(probeVectorCnt);
    }
    joinedVectorBatch.reset();
    delete[] selectedRows;
    selectedRows = nullptr;
}

OmniStatus NestLoopJoinLookupOperator::Close()
{
    if (curInputBatch != nullptr) {
        VectorHelper::FreeVecBatch(curInputBatch);
        curInputBatch = nullptr;
    }
    executionContext->GetArena()->Reset();
    UpdateCloseInfo();
    return OMNI_STATUS_NORMAL;
}
} // namespace omniruntime::op
