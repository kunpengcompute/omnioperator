/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: join result builder implementations
 */
#include "sort_merge_join_resultBuilder.h"
#include <memory>
#include "expression/jsonparser/jsonparser.h"
#include "operator/pages_index.h"
#include "operator/omni_id_type_vector_traits.h"
#include "sort_merge_join_scanner.h"
#include "util/compiler_util.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::type;

JoinResultBuilder::JoinResultBuilder(const std::vector<DataTypePtr> &leftTableOutputTypes, int32_t *leftTableOutputCols,
    int32_t leftTableOutputColsCount, int32_t originalLeftTableColsCount, DynamicPagesIndex *leftTablePagesIndex,
    const std::vector<DataTypePtr> &rightTableOutputTypes, int32_t *rightTableOutputCols,
    int32_t rightTableOutputColsCount, int32_t originalRightTableColsCount, DynamicPagesIndex *rightTablePagesIndex,
    std::string &filter, JoinType joinType, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : leftTableOutputTypes(leftTableOutputTypes),
      leftTableOutputCols(leftTableOutputCols),
      leftTableOutputColsCount(leftTableOutputColsCount),
      originalLeftTableColsCount(originalLeftTableColsCount),
      leftTablePagesIndex(leftTablePagesIndex),
      rightTableOutputTypes(rightTableOutputTypes),
      rightTableOutputCols(rightTableOutputCols),
      rightTableOutputColsCount(rightTableOutputColsCount),
      originalRightTableColsCount(originalRightTableColsCount),
      rightTablePagesIndex(rightTablePagesIndex),
      filterExpStr(filter),
      joinType(joinType)
{
    executionContext = new ExecutionContext();
    executionContext->SetConfig(queryConfig);
    int32_t leftRowSize = OperatorUtil::GetRowSize(leftTableOutputTypes);
    int32_t rightRowSize = OperatorUtil::GetRowSize(rightTableOutputTypes);
    int32_t outputRowSize = leftRowSize + rightRowSize;
    this->maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        outputRowSize != 0 ? outputRowSize : DEFAULT_ROW_SIZE, executionContext != nullptr ? &executionContext->queryConfigRef() : nullptr);
    this->JoinFilterCodeGen(overflowConfig);
    allTypes.insert(allTypes.cend(), leftTableOutputTypes.cbegin(), leftTableOutputTypes.cend());
    allTypes.insert(allTypes.cend(), rightTableOutputTypes.cbegin(), rightTableOutputTypes.cend());
}

JoinResultBuilder::JoinResultBuilder(const std::vector<DataTypePtr>& leftTableOutputTypes, int32_t* leftTableOutputCols,
    int32_t leftTableOutputColsCount, int32_t originalLeftTableColsCount, DynamicPagesIndex* leftTablePagesIndex,
    const std::vector<DataTypePtr>& rightTableOutputTypes, int32_t* rightTableOutputCols,
    int32_t rightTableOutputColsCount, int32_t originalRightTableColsCount, DynamicPagesIndex* rightTablePagesIndex,
    Expr* filter, JoinType joinType, OverflowConfig* overflowConfig, const config::QueryConfig &queryConfig)
    : leftTableOutputTypes(leftTableOutputTypes),
      leftTableOutputCols(leftTableOutputCols),
      leftTableOutputColsCount(leftTableOutputColsCount),
      originalLeftTableColsCount(originalLeftTableColsCount),
      leftTablePagesIndex(leftTablePagesIndex),
      rightTableOutputTypes(rightTableOutputTypes),
      rightTableOutputCols(rightTableOutputCols),
      rightTableOutputColsCount(rightTableOutputColsCount),
      originalRightTableColsCount(originalRightTableColsCount),
      rightTablePagesIndex(rightTablePagesIndex),
      filterExpr(filter),
      joinType(joinType)
{
    executionContext = new ExecutionContext();
    executionContext->SetConfig(queryConfig);
    int32_t leftRowSize = OperatorUtil::GetRowSize(leftTableOutputTypes);
    int32_t rightRowSize = OperatorUtil::GetRowSize(rightTableOutputTypes);
    int32_t outputRowSize = leftRowSize + rightRowSize;
    this->maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        outputRowSize != 0 ? outputRowSize : DEFAULT_ROW_SIZE, executionContext != nullptr ? &executionContext->queryConfigRef() : nullptr);
    this->JoinFilterExprCodeGen(overflowConfig);
    allTypes.insert(allTypes.cend(), leftTableOutputTypes.cbegin(), leftTableOutputTypes.cend());
    allTypes.insert(allTypes.cend(), rightTableOutputTypes.cbegin(), rightTableOutputTypes.cend());
}

void JoinResultBuilder::JoinFilterCodeGen(OverflowConfig* overflowConfig)
{
    if (filterExpStr.empty()) {
        return;
    }

    omniruntime::expressions::Expr* filterExpression = JSONParser::ParseJSON(filterExpStr);
    simpleFilter = new SimpleFilter(*filterExpression);
    auto result = simpleFilter->Initialize(overflowConfig);
    if (!result) {
        delete executionContext;
        delete simpleFilter;
        simpleFilter = nullptr;
        delete filterExpression;
        throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT", "The expression is not supported yet.");
    }
    delete filterExpression;

    streamedColsInFilter = new int32_t[originalLeftTableColsCount];
    bufferedColsInFilter = new int32_t[originalRightTableColsCount];
    auto colsInFilter = simpleFilter->GetVectorIndexes();
    for (auto col : colsInFilter) {
        if (col < originalLeftTableColsCount) {
            streamedColsInFilter[streamedColsCountInFilter++] = col;
        } else {
            bufferedColsInFilter[bufferedColsCountInFilter++] = col - originalLeftTableColsCount;
        }
    }
    auto originalAllColsCount = originalLeftTableColsCount + originalRightTableColsCount;
    values = new int64_t[originalAllColsCount];
    nulls = new bool[originalAllColsCount];
    lengths = new int32_t[originalAllColsCount];
    memset(values, 0, sizeof(int64_t) * originalAllColsCount);
    memset(nulls, 0, sizeof(bool) * originalAllColsCount);
    memset(lengths, 0, sizeof(int32_t) * originalAllColsCount);
}

void JoinResultBuilder::JoinFilterExprCodeGen(OverflowConfig *overflowConfig)
{
    if (filterExpr == nullptr) {
        return;
    }

    simpleFilter = new SimpleFilter(*filterExpr);
    auto result = simpleFilter->Initialize(overflowConfig);
    if (!result) {
        delete executionContext;
        delete simpleFilter;
        simpleFilter = nullptr;
        throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT", "The expression is not supported yet.");
    }

    streamedColsInFilter = new int32_t[originalLeftTableColsCount];
    bufferedColsInFilter = new int32_t[originalRightTableColsCount];
    auto colsInFilter = simpleFilter->GetVectorIndexes();
    for (auto col : colsInFilter) {
        if (col < originalLeftTableColsCount) {
            streamedColsInFilter[streamedColsCountInFilter++] = col;
        } else {
            bufferedColsInFilter[bufferedColsCountInFilter++] = col - originalLeftTableColsCount;
        }
    }
    auto originalAllColsCount = originalLeftTableColsCount + originalRightTableColsCount;
    values = new int64_t[originalAllColsCount];
    nulls = new bool[originalAllColsCount];
    lengths = new int32_t[originalAllColsCount];
    memset(values, 0, sizeof(int64_t) * originalAllColsCount);
    memset(nulls, 0, sizeof(bool) * originalAllColsCount);
    memset(lengths, 0, sizeof(int32_t) * originalAllColsCount);
}

VectorBatch *JoinResultBuilder::NewEmptyVectorBatch(int32_t rowCount) const
{
    // using smart ptr to avoid memory leak when task recovery
    auto vectorBatch = std::make_unique<VectorBatch>(rowCount);

    for (auto &type : allTypes) {
        // ARRAY/MAP/ROW must use CreateComplexVector; CreateFlatVector only supports primitive types
        vectorBatch->Append(VectorHelper::CreateComplexVector(type.get(), rowCount));
    }
    return vectorBatch.release();
}

void JoinResultBuilder::EnsureBuildVectorBatchCapacity(int32_t rowCount)
{
    if (rowCount <= 0) {
        return;
    }

    if (buildVectorBatch == nullptr) {
        buildVectorBatch = NewEmptyVectorBatch(rowCount);
        return;
    }

    if (rowCount > buildVectorBatch->GetRowCount()) {
        buildVectorBatch->Expand(static_cast<size_t>(rowCount));
    }
}

template <type::DataTypeId typeId>
void AddValueToVector(BaseVector *inputVector, int32_t inputRowId, BaseVector *outputVector, int32_t outputRowId)
{
    using Type = typename NativeAndVectorType<typeId>::type;
    using Vector = typename NativeAndVectorType<typeId>::vector;

    if (UNLIKELY(inputVector->IsNull(inputRowId))) {
        reinterpret_cast<Vector *>(outputVector)->SetNull(outputRowId);
    } else {
        // The input may be dictionary.
        Type value;
        if (UNLIKELY(inputVector->GetEncoding() == OMNI_DICTIONARY)) {
            value = reinterpret_cast<omniruntime::vec::Vector<vec::DictionaryContainer<Type >> *>(inputVector)
                    ->GetValue(inputRowId);
        } else {
            value = reinterpret_cast<Vector *>(inputVector)->GetValue(inputRowId);
        }
        reinterpret_cast<Vector *>(outputVector)->SetValue(outputRowId, value);
    }
}

template <type::DataTypeId typeId>
void AddValuesToVector(BaseVector *inputVector, const int32_t* inputRowIds, int32_t inputSize,
    BaseVector *outputVector, int32_t outputRowId)
{
    using Type = typename NativeAndVectorType<typeId>::type;
    using Vector = typename NativeAndVectorType<typeId>::vector;
    for (int32_t i = 0; i < inputSize; i++) {
        auto inputRowId = inputRowIds[i];
        if (UNLIKELY(inputVector->IsNull(inputRowId))) {
            reinterpret_cast<Vector *>(outputVector)->SetNull(outputRowId);
        } else {
            // The input may be dictionary.
            Type value;
            if (UNLIKELY(inputVector->GetEncoding() == OMNI_DICTIONARY)) {
                value = reinterpret_cast<omniruntime::vec::Vector<vec::DictionaryContainer<Type >> *>(inputVector)
                        ->GetValue(inputRowId);
            } else {
                value = reinterpret_cast<Vector *>(inputVector)->GetValue(inputRowId);
            }
            reinterpret_cast<Vector *>(outputVector)->SetValue(outputRowId, value);
        }
        ++outputRowId;
    }
}


void AddValueToBuildVector(BaseVector *inputVector, const DataTypePtr &inputDataType, int32_t inputRowId,
    BaseVector *outputVector, int32_t outputRowId)
{
    auto typeId = inputDataType->GetId();
    if (typeId == type::OMNI_ARRAY) {
        auto *inputArr = static_cast<ArrayVector *>(inputVector);
        auto *outputArr = static_cast<ArrayVector *>(outputVector);
        if (UNLIKELY(inputArr->IsNull(inputRowId))) {
            outputArr->SetNull(outputRowId);
        } else {
            BaseVector *slice = inputArr->GetValue(inputRowId);
            outputArr->SetValue(outputRowId, slice);
            delete slice;
        }
        return;
    }
    if (typeId == type::OMNI_ROW) {
        const auto *rowType = static_cast<const RowType *>(inputDataType.get());
        auto *inputRow = static_cast<RowVector *>(inputVector);
        auto *outputRow = static_cast<RowVector *>(outputVector);
        if (UNLIKELY(inputRow->IsNull(inputRowId))) {
            outputRow->SetNull(outputRowId);
        } else {
            for (uint32_t i = 0; i < rowType->Size(); i++) {
                AddValueToBuildVector(inputRow->ChildAt(i).get(), rowType->childAt(i), inputRowId,
                    outputRow->ChildAt(i).get(), outputRowId);
            }
        }
        return;
    }
    DYNAMIC_TYPE_DISPATCH(AddValueToVector, typeId, inputVector, inputRowId, outputVector, outputRowId);
}

void AddValuesToBuildVector(BaseVector *inputVector, const DataTypePtr &inputDataType, const std::vector<int32_t> &rows,
    BaseVector *outputVector, int32_t outputRowId)
{
    auto typeId = inputDataType->GetId();
    if (typeId == type::OMNI_ARRAY) {
        auto *inputArr = static_cast<ArrayVector *>(inputVector);
        auto *outputArr = static_cast<ArrayVector *>(outputVector);
        for (size_t i = 0; i < rows.size(); i++) {
            int32_t inputRowId = rows[i];
            if (UNLIKELY(inputArr->IsNull(inputRowId))) {
                outputArr->SetNull(outputRowId);
            } else {
                BaseVector *slice = inputArr->GetValue(inputRowId);
                outputArr->SetValue(outputRowId, slice);
                delete slice;
            }
            ++outputRowId;
        }
        return;
    }
    if (typeId == type::OMNI_ROW) {
        const auto *rowType = static_cast<const RowType *>(inputDataType.get());
        auto *inputRow = static_cast<RowVector *>(inputVector);
        auto *outputRow = static_cast<RowVector *>(outputVector);
        for (size_t i = 0; i < rows.size(); i++) {
            int32_t inputRowId = rows[i];
            if (UNLIKELY(inputRow->IsNull(inputRowId))) {
                outputRow->SetNull(outputRowId);
            } else {
                for (uint32_t j = 0; j < rowType->Size(); j++) {
                    AddValueToBuildVector(inputRow->ChildAt(j).get(), rowType->childAt(j), inputRowId,
                        outputRow->ChildAt(j).get(), outputRowId);
                }
            }
            ++outputRowId;
        }
        return;
    }
    auto rowIds = rows.data();
    auto rowIdsCount = rows.size();
    DYNAMIC_TYPE_DISPATCH(AddValuesToVector, typeId, inputVector, rowIds, rowIdsCount, outputVector, outputRowId);
}

void AddValueNullToBuildVector(const DataTypePtr &dataType, BaseVector *vector, int32_t rowId)
{
    auto typeId = dataType->GetId();
    if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR) {
        static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(rowId);
    } else {
        vector->SetNull(rowId);
    }
}

void JoinResultBuilder::ParsingAndOrganizationResultsForLeftTable(int32_t leftBatchId, int32_t leftRowId)
{
    if (leftBatchId == JOIN_NULL_FLAG) {
        PaddingLeftTableNull();
    } else {
        for (int columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
            AddValueToBuildVector(leftTablePagesIndex->GetColumn(leftBatchId, leftTableOutputCols[columnIdx]),
                leftTableOutputTypes[columnIdx], leftRowId, buildVectorBatch->Get(columnIdx), buildRowCount);
        }
    }
}

void JoinResultBuilder::ParsingAndOrganizationResultsForRightTable(int32_t rightBatchId, int32_t rightRowId)
{
    if (rightBatchId == JOIN_NULL_FLAG) {
        PaddingRightTableNull();
    } else {
        for (int columnIdx = 0; columnIdx < rightTableOutputColsCount; columnIdx++) {
            int32_t buildColumnIdx = leftTableOutputColsCount + columnIdx;
            AddValueToBuildVector(rightTablePagesIndex->GetColumn(rightBatchId, rightTableOutputCols[columnIdx]),
                rightTableOutputTypes[columnIdx], rightRowId, buildVectorBatch->Get(buildColumnIdx), buildRowCount);
        }
    }
}

void JoinResultBuilder::PaddingLeftTableNull(int32_t rightBatchId, int32_t rightRowId)
{
    PaddingLeftTableNull();
    ParsingAndOrganizationResultsForRightTable(rightBatchId, rightRowId);
    buildRowCount++;
}

void JoinResultBuilder::PaddingRightTableNull(int32_t leftBatchId, int32_t leftRowId)
{
    ParsingAndOrganizationResultsForLeftTable(leftBatchId, leftRowId);
    PaddingRightTableNull();
    buildRowCount++;
}

void JoinResultBuilder::SetRightNullRangeForColumn(BaseVector *vector, const DataTypePtr &dataType, int32_t startRow,
    int32_t endRow)
{
    auto typeId = dataType->GetId();
    if (typeId == type::OMNI_VARCHAR || typeId == type::OMNI_CHAR) {
        auto *varcharVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
        for (int32_t i = startRow; i < endRow; i++) {
            varcharVec->SetNull(i);
        }
    } else {
        vector->SetNulls(startRow, true, endRow - startRow);
    }
}

void JoinResultBuilder::UpdateLeftAntiJoinHandler(int32_t addressPosition,
    std::vector<int8_t> &isSameBufferedKeyMatched, int32_t inputSize)
{
    if (!isSameBufferedKeyMatched.empty()) {
        if (addressPosition == inputSize - 1) {
            leftAntiJoinHandler.hasSameBufferedRow = false;
        } else {
            if (isSameBufferedKeyMatched[addressPosition] == 0) {
                leftAntiJoinHandler.printThisStreamRowOutFlag = true;
            }
            if (isSameBufferedKeyMatched[addressPosition + 1] == 0) {
                leftAntiJoinHandler.hasSameBufferedRow = false;
            } else {
                leftAntiJoinHandler.hasSameBufferedRow = true;
            }
        }
    } else {
        leftAntiJoinHandler.hasSameBufferedRow = false;
        leftAntiJoinHandler.printThisStreamRowOutFlag = true;
    }
}

int32_t JoinResultBuilder::CollectRowsInfo(std::vector<std::pair<int32_t, int32_t>> &leftMeta,
    std::vector<std::pair<int32_t, int32_t>> &rightMeta,
    std::vector<bool> &canFreeRights, int32_t inputSize, int32_t &counter)
{
    bool canFreeRight = false;
    for (int32_t addressPosition = addressOffset; addressPosition < inputSize; addressPosition++) {
        int64_t streamedRowAddress = streamedTableValueAddresses[addressPosition];
        int64_t bufferedRowAddress = bufferedTableValueAddresses[addressPosition];
        int32_t leftBatchId = DecodeSliceIndex(streamedRowAddress);
        int32_t leftRowId = DecodePosition(streamedRowAddress);
        int32_t rightBatchId = DecodeSliceIndex(bufferedRowAddress);
        int32_t rightRowId = DecodePosition(bufferedRowAddress);
        canFreeRight = CanFreeRightBatches(isPreKeyMatched[addressPosition], leftBatchId, rightBatchId);

        bool isEligible = IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId);
        if (isEligible) {
            leftMeta.emplace_back(std::make_pair(leftBatchId, leftRowId));
            rightMeta.emplace_back(std::make_pair(rightBatchId, rightRowId));
            canFreeRights.emplace_back(canFreeRight);
            ++counter;
        }
        ++addressOffset;
        if (counter >= maxRowCount) {
            return 1;
        }
    }
    return 0;
}

int32_t JoinResultBuilder::ConstructInnerJoinOutput()
{
    auto inputSize = static_cast<int32_t>(streamedTableValueAddresses.size());
    if (addressOffset >= inputSize) {
        buildVectorBatchRowCount = buildRowCount;
        return 0;
    }
    auto initialBuildRowCount = buildRowCount;
    auto initialAddressOffset = addressOffset;
    std::vector<std::pair<int32_t, int32_t>> leftMeta;
    leftMeta.reserve(inputSize - initialAddressOffset);
    std::vector<std::pair<int32_t, int32_t>> rightMeta;
    rightMeta.reserve(inputSize - initialAddressOffset);
    int32_t counter = buildRowCount;
    std::vector<bool> canFreeRights;
    canFreeRights.reserve(inputSize - initialAddressOffset);
    auto result = CollectRowsInfo(leftMeta, rightMeta, canFreeRights, inputSize, counter);

    // put all the positions belonging to the same batch together
    auto leftGroupedRowsPerBatchId = std::vector<std::pair<int32_t, std::vector<int32_t>>>();
    GroupByMeta(leftGroupedRowsPerBatchId, leftMeta);
    auto rightGroupedRowsPerBatchId = std::vector<std::pair<int32_t, std::vector<int32_t>>>();
    GroupByMeta(rightGroupedRowsPerBatchId, rightMeta);

    // get the vectors of each batch
    int32_t prevBatchId = 0;
    for (auto &batchRows: leftGroupedRowsPerBatchId) { // go over the different batches
        auto batchId = batchRows.first;
        auto rowIds = batchRows.second;
        if (prevBatchId != batchId) {
            this->leftTablePagesIndex->FreeBeforeVecBatch(prevBatchId);
            prevBatchId = batchId;
        }
        for (int32_t columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
            auto columnIndex = leftTableOutputCols[columnIdx];
            auto inputDataType = leftTableOutputTypes[columnIdx];
            auto outputVector = buildVectorBatch->Get(columnIdx);
            auto inputVector = leftTablePagesIndex->GetColumn(batchId, columnIndex);
            AddValuesToBuildVector(inputVector, inputDataType, rowIds, outputVector, buildRowCount);
        }
        buildRowCount+= rowIds.size();
    }
    if (rightTableOutputColsCount > 0) {
        buildRowCount = initialBuildRowCount;
    }
    int32_t rightRowsCounter = 0;
    for (auto &batchRows: rightGroupedRowsPerBatchId) { // go over the different batches
        auto batchId = batchRows.first;
        auto rowIds = batchRows.second;
        for (int32_t columnIdx = 0; columnIdx < rightTableOutputColsCount; columnIdx++) {
            int32_t buildColumnIdx = leftTableOutputColsCount + columnIdx;
            auto columnIndex = rightTableOutputCols[columnIdx];
            auto inputDataType = rightTableOutputTypes[columnIdx];
            auto outputVector = buildVectorBatch->Get(buildColumnIdx);
            auto inputVector = rightTablePagesIndex->GetColumn(batchId, columnIndex);
            AddValuesToBuildVector(inputVector, inputDataType, rowIds, outputVector, buildRowCount);
        }
        auto rowsCount = rowIds.size();
        rightRowsCounter += rowsCount;
        if (canFreeRights[rightRowsCounter - 1]) {
            this->rightTablePagesIndex->FreeBeforeVecBatch(batchId);
        }
        buildRowCount+= rowsCount;
    }
    buildRowCount = counter;
    buildVectorBatchRowCount = buildRowCount;

    return result;
}

int32_t JoinResultBuilder::ConstructLeftJoinOutput()
{
    auto inputSize = static_cast<int32_t>(streamedTableValueAddresses.size());
    const int32_t addressStart = addressOffset;
    // Rows already materialized in buildVectorBatch from previous AddJoinValueAddresses in the same batch
    const int32_t rowBase = buildRowCount;
    leftOutputBatchIds.clear();
    leftOutputRowIds.clear();
    rightOutputBatchIds.clear();
    rightOutputRowIds.clear();

    bool batchFull = false;

    for (int32_t addressPosition = addressOffset; addressPosition < inputSize; addressPosition++) {
        int64_t streamedRowAddress = streamedTableValueAddresses[addressPosition];
        int64_t bufferedRowAddress = bufferedTableValueAddresses[addressPosition];
        if (preStreamedRowAddress != INT64_MAX && preStreamedRowAddress != streamedRowAddress &&
            !preLeftTableRowMatchedOut) {
            leftOutputBatchIds.push_back(preStreamedBatchId);
            leftOutputRowIds.push_back(preStreamedRowId);
            rightOutputBatchIds.push_back(JOIN_NULL_FLAG);
            rightOutputRowIds.push_back(0);
        }
        if (preStreamedRowAddress == INT64_MAX || preStreamedRowAddress != streamedRowAddress) {
            preLeftTableRowMatchedOut = false;
        }

        int32_t leftBatchId = DecodeSliceIndex(streamedRowAddress);
        int32_t leftRowId = DecodePosition(streamedRowAddress);
        int32_t rightBatchId = DecodeSliceIndex(bufferedRowAddress);
        int32_t rightRowId = DecodePosition(bufferedRowAddress);

        if (leftBatchId == JOIN_NULL_FLAG || rightBatchId == JOIN_NULL_FLAG ||
            IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId)) {
            leftOutputBatchIds.push_back(leftBatchId);
            leftOutputRowIds.push_back(leftRowId);
            rightOutputBatchIds.push_back(rightBatchId);
            rightOutputRowIds.push_back(rightRowId);
            preLeftTableRowMatchedOut = true;
        }

        preStreamedRowAddress = streamedRowAddress;
        preStreamedBatchId = leftBatchId;
        preStreamedRowId = leftRowId;
        addressOffset++;
        if (rowBase + static_cast<int32_t>(leftOutputBatchIds.size()) >= maxRowCount) {
            batchFull = true;
            break;
        }
    }

    if (!batchFull && preStreamedRowAddress != INT64_MAX && !preLeftTableRowMatchedOut) {
        leftOutputBatchIds.push_back(preStreamedBatchId);
        leftOutputRowIds.push_back(preStreamedRowId);
        rightOutputBatchIds.push_back(JOIN_NULL_FLAG);
        rightOutputRowIds.push_back(0);
        preStreamedRowAddress = INT64_MAX;
    } else if (!batchFull) {
        preStreamedRowAddress = INT64_MAX;
    }

    int32_t numOutputRows = static_cast<int32_t>(leftOutputBatchIds.size());
    if (numOutputRows == 0) {
        buildVectorBatchRowCount = rowBase;
        return 0;
    }

    EnsureBuildVectorBatchCapacity(rowBase + numOutputRows);

    for (int32_t columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
        int32_t outPos = rowBase;
        int32_t segmentStart = 0;
        while (segmentStart < numOutputRows) {
            int32_t batchId = leftOutputBatchIds[segmentStart];
            int32_t segmentEnd = segmentStart;
            while (segmentEnd < numOutputRows && leftOutputBatchIds[segmentEnd] == batchId) {
                segmentEnd++;
            }
            std::vector<int32_t> rowIds(leftOutputRowIds.begin() + segmentStart, leftOutputRowIds.begin() + segmentEnd);
            if (batchId == JOIN_NULL_FLAG) {
                for (int32_t r = 0; r < static_cast<int32_t>(rowIds.size()); r++) {
                    AddValueNullToBuildVector(leftTableOutputTypes[columnIdx], buildVectorBatch->Get(columnIdx),
                        outPos + r);
                }
            } else {
                auto inputVector = leftTablePagesIndex->GetColumn(batchId, leftTableOutputCols[columnIdx]);
                AddValuesToBuildVector(inputVector, leftTableOutputTypes[columnIdx], rowIds,
                    buildVectorBatch->Get(columnIdx), outPos);
            }
            outPos += static_cast<int32_t>(rowIds.size());
            segmentStart = segmentEnd;
        }
    }

    for (int32_t columnIdx = 0; columnIdx < rightTableOutputColsCount; columnIdx++) {
        int32_t buildColumnIdx = leftTableOutputColsCount + columnIdx;
        int32_t outRow = 0;
        while (outRow < numOutputRows) {
            int32_t batchId = rightOutputBatchIds[outRow];
            int32_t segmentStart = outRow;
            while (outRow < numOutputRows && rightOutputBatchIds[outRow] == batchId) {
                outRow++;
            }
            int32_t segmentEnd = outRow;
            if (batchId == JOIN_NULL_FLAG) {
                SetRightNullRangeForColumn(buildVectorBatch->Get(buildColumnIdx), rightTableOutputTypes[columnIdx],
                    rowBase + segmentStart, rowBase + segmentEnd);
            } else {
                std::vector<int32_t> rowIds(rightOutputRowIds.begin() + segmentStart, rightOutputRowIds.begin() + segmentEnd);
                auto inputVector = rightTablePagesIndex->GetColumn(batchId, rightTableOutputCols[columnIdx]);
                AddValuesToBuildVector(inputVector, rightTableOutputTypes[columnIdx], rowIds,
                    buildVectorBatch->Get(buildColumnIdx), rowBase + segmentStart);
            }
        }
    }

    // Input vectors are still needed while the output columns are copied. Release only
    // after materialization, and only for addresses consumed by this output batch.
    for (int32_t processedAddress = addressStart; processedAddress < addressOffset; processedAddress++) {
        const int32_t leftBatchId = DecodeSliceIndex(streamedTableValueAddresses[processedAddress]);
        const int32_t rightBatchId = DecodeSliceIndex(bufferedTableValueAddresses[processedAddress]);
        if (leftBatchId == JOIN_NULL_FLAG || rightBatchId == JOIN_NULL_FLAG) {
            continue;
        }
        FreeVectorBatches(isPreKeyMatched[processedAddress], leftBatchId, rightBatchId);
    }

    buildRowCount = rowBase + numOutputRows;
    buildVectorBatchRowCount = rowBase + numOutputRows;

    return batchFull ? 1 : 0;
}

int32_t JoinResultBuilder::ConstructFullJoinOutput()
{
    auto inputSize = static_cast<int32_t>(streamedTableValueAddresses.size());
    for (int32_t addressPosition = addressOffset; addressPosition < inputSize; addressPosition++) {
        int64_t streamedRowAddress = streamedTableValueAddresses[addressPosition];
        int64_t bufferedRowAddress = bufferedTableValueAddresses[addressPosition];
        if (preStreamedRowAddress != INT64_MAX && preStreamedRowAddress != streamedRowAddress &&
            !preLeftTableRowMatchedOut) {
            // set values for left but set nulls for right for left join, full join, left anti
            PaddingRightTableNull(preStreamedBatchId, preStreamedRowId);
        }
        if (preStreamedRowAddress == INT64_MAX || preStreamedRowAddress != streamedRowAddress) {
            preLeftTableRowMatchedOut = false;
        }

        int32_t leftBatchId = DecodeSliceIndex(streamedRowAddress);
        int32_t leftRowId = DecodePosition(streamedRowAddress);
        int32_t rightBatchId = DecodeSliceIndex(bufferedRowAddress);
        int32_t rightRowId = DecodePosition(bufferedRowAddress);
        if (leftBatchId != JOIN_NULL_FLAG && rightBatchId != JOIN_NULL_FLAG) {
            FreeVectorBatches(isPreKeyMatched[addressPosition], leftBatchId, rightBatchId);
        }

        // JOIN_NULL_FLAG row direct output && filter match direct output, pad no NULL
        if (leftBatchId == JOIN_NULL_FLAG || rightBatchId == JOIN_NULL_FLAG ||
            IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId)) {
            ParsingAndOrganizationResultsForLeftTable(leftBatchId, leftRowId);
            ParsingAndOrganizationResultsForRightTable(rightBatchId, rightRowId);
            buildRowCount++;
            preLeftTableRowMatchedOut = true;
        } else { // filter mismatch, pad left table NULL
            PaddingLeftTableNull(rightBatchId, rightRowId);
        }

        preStreamedRowAddress = streamedRowAddress;
        preStreamedBatchId = leftBatchId;
        preStreamedRowId = leftRowId;
        addressOffset++;
        if (buildRowCount >= maxRowCount) {
            buildVectorBatchRowCount = buildRowCount;
            return 1;
        }
    }

    if (preStreamedRowAddress != INT64_MAX && !preLeftTableRowMatchedOut) {
        // pad NULL for last mismatch row
        PaddingRightTableNull(preStreamedBatchId, preStreamedRowId);
    }
    buildVectorBatchRowCount = buildRowCount;
    preStreamedRowAddress = INT64_MAX;
    if (buildRowCount >= maxRowCount) {
        return 1;
    }
    return 0;
}

int32_t JoinResultBuilder::ConstructLeftSemiJoinOutput()
{
    auto inputSize = static_cast<int32_t>(streamedTableValueAddresses.size());
    for (int32_t addressPosition = addressOffset; addressPosition < inputSize; addressPosition++) {
        int64_t streamedRowAddress = streamedTableValueAddresses[addressPosition];
        int64_t bufferedRowAddress = bufferedTableValueAddresses[addressPosition];
        int32_t leftBatchId = DecodeSliceIndex(streamedRowAddress);
        int32_t leftRowId = DecodePosition(streamedRowAddress);
        int32_t rightBatchId = DecodeSliceIndex(bufferedRowAddress);
        int32_t rightRowId = DecodePosition(bufferedRowAddress);
        if (leftBatchId != JOIN_NULL_FLAG && rightBatchId != JOIN_NULL_FLAG) {
            FreeVectorBatches(isPreKeyMatched[addressPosition], leftBatchId, rightBatchId);
        }

        // left semi join only needs to output the data of the left table
        if (isSameBufferedKeyMatched[addressPosition] != 0) {
            // same buffered key match and failed to match the previous
            if (!isPreRowMatched && IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId)) {
                for (int columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
                    AddValueToBuildVector(leftTablePagesIndex->GetColumn(leftBatchId, leftTableOutputCols[columnIdx]),
                        leftTableOutputTypes[columnIdx], leftRowId, buildVectorBatch->Get(columnIdx), buildRowCount);
                }
                buildRowCount++;
                isPreRowMatched = true;
            }
        } else {
            // not the same buffered key
            if (IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId)) {
                for (int columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
                    AddValueToBuildVector(leftTablePagesIndex->GetColumn(leftBatchId, leftTableOutputCols[columnIdx]),
                        leftTableOutputTypes[columnIdx], leftRowId, buildVectorBatch->Get(columnIdx), buildRowCount);
                }
                buildRowCount++;
                isPreRowMatched = true;
            } else {
                isPreRowMatched = false;
            }
        }

        preStreamedRowAddress = streamedRowAddress;
        preStreamedBatchId = leftBatchId;
        preStreamedRowId = leftRowId;
        addressOffset++;
        if (buildRowCount >= maxRowCount) {
            buildVectorBatchRowCount = buildRowCount;
            return 1;
        }
    }

    buildVectorBatchRowCount = buildRowCount;
    preStreamedRowAddress = INT64_MAX;
    if (buildRowCount >= maxRowCount) {
        return 1;
    }
    return 0;
}

int32_t JoinResultBuilder::ConstructLeftAntiJoinOutput()
{
    auto inputSize = static_cast<int32_t>(streamedTableValueAddresses.size());
    leftAntiJoinHandler.hasSameBufferedRow = false;
    leftAntiJoinHandler.printThisStreamRowOutFlag = true;
    for (int32_t addressPosition = addressOffset; addressPosition < inputSize; addressPosition++) {
        UpdateLeftAntiJoinHandler(addressPosition, isSameBufferedKeyMatched, inputSize); // only for left anti
        int64_t streamedRowAddress = streamedTableValueAddresses[addressPosition];
        int64_t bufferedRowAddress = bufferedTableValueAddresses[addressPosition];
        if (preStreamedRowAddress != INT64_MAX && preStreamedRowAddress != streamedRowAddress &&
            !preLeftTableRowMatchedOut) {
            // set values for left but set nulls for right for left join, full join, left anti
            PaddingRightTableNull(preStreamedBatchId, preStreamedRowId);
        }
        if (preStreamedRowAddress == INT64_MAX || preStreamedRowAddress != streamedRowAddress) {
            preLeftTableRowMatchedOut = false;
        }

        int32_t leftBatchId = DecodeSliceIndex(streamedRowAddress);
        int32_t leftRowId = DecodePosition(streamedRowAddress);
        int32_t rightBatchId = DecodeSliceIndex(bufferedRowAddress);
        int32_t rightRowId = DecodePosition(bufferedRowAddress);
        if (leftBatchId != JOIN_NULL_FLAG && rightBatchId != JOIN_NULL_FLAG) {
            FreeVectorBatches(isPreKeyMatched[addressPosition], leftBatchId, rightBatchId);
        }

        if (leftAntiJoinHandler.hasSameBufferedRow) {
            if (leftAntiJoinHandler.printThisStreamRowOutFlag &&
                IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId)) {
                leftAntiJoinHandler.printThisStreamRowOutFlag = false;
            }
        } else {
            if (rightBatchId == JOIN_NULL_FLAG) {
                ParsingAndOrganizationResultsForLeftTable(leftBatchId, leftRowId);
                buildRowCount++;
            } else if (!IsJoinPositionEligible(leftBatchId, leftRowId, rightBatchId, rightRowId) &&
                leftAntiJoinHandler.printThisStreamRowOutFlag) {
                ParsingAndOrganizationResultsForLeftTable(leftBatchId, leftRowId);
                buildRowCount++;
            } else {
                leftAntiJoinHandler.printThisStreamRowOutFlag = false;
            }
        }
        preLeftTableRowMatchedOut = true;

        preStreamedRowAddress = streamedRowAddress;
        preStreamedBatchId = leftBatchId;
        preStreamedRowId = leftRowId;
        addressOffset++;
        if (buildRowCount >= maxRowCount) {
            buildVectorBatchRowCount = buildRowCount;
            return 1;
        }
    }

    if (preStreamedRowAddress != INT64_MAX && !preLeftTableRowMatchedOut) {
        // pad NULL for last mismatch row
        PaddingRightTableNull(preStreamedBatchId, preStreamedRowId);
    }
    buildVectorBatchRowCount = buildRowCount;
    preStreamedRowAddress = INT64_MAX;
    if (buildRowCount >= maxRowCount) {
        return 1;
    }
    return 0;
}

int32_t JoinResultBuilder::AddJoinValueAddresses()
{
    // 1 represents the rowCount of buildVectorBatch has reached the maxRowCount
    // 0 represents the rowCount of buildVectorBatch doesn't reached the maxRowCount
    if (buildVectorBatch == nullptr) {
        buildRowCount = 0;
        if (joinType != JoinType::OMNI_JOIN_TYPE_LEFT) {
            buildVectorBatch = NewEmptyVectorBatch(maxRowCount);
        }
    }

    switch (joinType) {
        case JoinType::OMNI_JOIN_TYPE_INNER:
            return ConstructInnerJoinOutput();
        case JoinType::OMNI_JOIN_TYPE_LEFT:
            return ConstructLeftJoinOutput();
        case JoinType::OMNI_JOIN_TYPE_FULL:
            return ConstructFullJoinOutput();
        case JoinType::OMNI_JOIN_TYPE_LEFT_SEMI:
            return ConstructLeftSemiJoinOutput();
        case JoinType::OMNI_JOIN_TYPE_LEFT_ANTI:
            return ConstructLeftAntiJoinOutput();
        default:
            throw OmniException("OPERATOR_RUNTIME_ERROR", "SMJ unsupported join type");
    }
}

void JoinResultBuilder::FreeVectorBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId)
{
    if (!isPreMatched && leftBatchId > lastUnMatchedStreamedBatchId) {
        LogWarn("SMJ_LEFT_JOIN_FREE builder=%p left_batch=%d right_batch=%d previous_left_freed=%d",
            static_cast<const void *>(this), leftBatchId, rightBatchId, lastUnMatchedStreamedBatchId);
        leftTablePagesIndex->FreeBeforeVecBatch(leftBatchId);
        rightTablePagesIndex->FreeBeforeVecBatch(rightBatchId);
        lastUnMatchedStreamedBatchId = leftBatchId;
    }
}

bool JoinResultBuilder::CanFreeRightBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId)
{
    if (!isPreMatched && leftBatchId > lastUnMatchedStreamedBatchId) {
        lastUnMatchedStreamedBatchId = leftBatchId;
        return true;
    }
    return false;
}

VectorBatch *GetVectorBatchFromSlice(VectorBatch *vectorBatch, std::vector<DataTypePtr> &dataTypes, int32_t rowCount)
{
    auto outputColCount = vectorBatch->GetVectorCount();
    auto sliceBatch = std::make_unique<VectorBatch>(rowCount);
    for (int32_t columnIdx = 0; columnIdx < outputColCount; columnIdx++) {
        auto *vector = vectorBatch->Get(columnIdx);
        sliceBatch->Append(vec::VectorHelper::SliceVector(vector, 0, rowCount));
    }
    return sliceBatch.release();
}

int32_t JoinResultBuilder::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (buildVectorBatchRowCount > 0) {
        if (buildVectorBatchRowCount == maxRowCount || buildVectorBatchRowCount == buildVectorBatch->GetRowCount()) {
            *outputVecBatch = buildVectorBatch;
        } else {
            *outputVecBatch = GetVectorBatchFromSlice(buildVectorBatch, allTypes, buildVectorBatchRowCount);
            VectorHelper::FreeVecBatch(buildVectorBatch);
        }
    } else {
        if (buildVectorBatch != nullptr) {
            VectorHelper::FreeVecBatch(buildVectorBatch);
        }
    }

    buildVectorBatchRowCount = 0;
    buildVectorBatch = nullptr;

    return 0;
}

bool JoinResultBuilder::IsJoinPositionEligible(int32_t leftBatchId, int32_t leftRowId, int32_t rightBatchId,
    int32_t rightRowId) const
{
    if (!simpleFilter) {
        return true;
    }

    leftTablePagesIndex->CacheBatch(leftBatchId);
    for (int32_t i = 0; i < streamedColsCountInFilter; i++) {
        auto col = streamedColsInFilter[i];
        auto leftVector = leftTablePagesIndex->GetColumnFromCache(col);
        nulls[col] = leftVector->IsNull(leftRowId);
        auto leftDataType = leftTablePagesIndex->GetColumnTypeId(col);
        values[col] =
            OperatorUtil::GetValuePtrAndLengthFromRawVector(leftVector, leftRowId, lengths + col, leftDataType);
    }
    rightTablePagesIndex->CacheBatch(rightBatchId);
    for (int32_t i = 0; i < bufferedColsCountInFilter; i++) {
        auto col = bufferedColsInFilter[i];
        auto rightVector = rightTablePagesIndex->GetColumnFromCache(col);
        auto colIdx = col + originalLeftTableColsCount;
        nulls[colIdx] = rightVector->IsNull(rightRowId);
        auto rightDataType = rightTablePagesIndex->GetColumnTypeId(col);
        values[colIdx] =
            OperatorUtil::GetValuePtrAndLengthFromRawVector(rightVector, rightRowId, lengths + colIdx, rightDataType);
    }

    return simpleFilter->Evaluate(values, nulls, lengths, reinterpret_cast<int64_t>(executionContext));
}

void JoinResultBuilder::Finish()
{
    if (!isFinished) {
        leftTablePagesIndex->FreeAllRemainingVecBatch();
        rightTablePagesIndex->FreeAllRemainingVecBatch();
        isFinished = true;
    }
}

JoinResultBuilder::~JoinResultBuilder()
{
    delete simpleFilter;
    delete[] streamedColsInFilter;
    delete[] bufferedColsInFilter;
    delete[] values;
    delete[] nulls;
    delete[] lengths;

    delete executionContext;
    executionContext = nullptr;
}
}
}
