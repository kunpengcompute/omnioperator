/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: join result builder implementations
 */
#include "sort_merge_join_resultBuilder.h"
#include <cstring>
#include <memory>
#include "expression/jsonparser/jsonparser.h"
#include "operator/pages_index.h"
#include "operator/omni_id_type_vector_traits.h"
#include "sort_merge_join_scanner.h"
#include "util/compiler_util.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vector/unsafe_vector.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
// Segment-length cutover between the scalar and batched VARCHAR paths.
constexpr int32_t BATCHED_VARCHAR_MIN_ROWS = 8;

using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
}

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

void AddValuesToBuildVector(BaseVector *inputVector, const DataTypePtr &inputDataType, const int32_t *rows,
    int32_t rowCount, BaseVector *outputVector, int32_t outputRowId)
{
    auto typeId = inputDataType->GetId();
    if (typeId == type::OMNI_ARRAY) {
        auto *inputArr = static_cast<ArrayVector *>(inputVector);
        auto *outputArr = static_cast<ArrayVector *>(outputVector);
        for (int32_t i = 0; i < rowCount; i++) {
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
        for (int32_t i = 0; i < rowCount; i++) {
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
    DYNAMIC_TYPE_DISPATCH(AddValuesToVector, typeId, inputVector, rows, rowCount, outputVector, outputRowId);
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

    // Group consecutive output positions that reference the same source batch.
    auto leftGroupedRowsPerBatchId = std::vector<std::pair<int32_t, std::vector<int32_t>>>();
    GroupByMeta(leftGroupedRowsPerBatchId, leftMeta);
    auto rightGroupedRowsPerBatchId = std::vector<std::pair<int32_t, std::vector<int32_t>>>();
    GroupByMeta(rightGroupedRowsPerBatchId, rightMeta);

    // get the vectors of each batch
    int32_t prevBatchId = 0;
    for (auto &batchRows: leftGroupedRowsPerBatchId) { // go over the different batches
        auto batchId = batchRows.first;
        const auto &rowIds = batchRows.second;
        if (prevBatchId != batchId) {
            this->leftTablePagesIndex->FreeBeforeVecBatch(prevBatchId);
            prevBatchId = batchId;
        }
        for (int32_t columnIdx = 0; columnIdx < leftTableOutputColsCount; columnIdx++) {
            auto columnIndex = leftTableOutputCols[columnIdx];
            auto inputDataType = leftTableOutputTypes[columnIdx];
            auto outputVector = buildVectorBatch->Get(columnIdx);
            auto inputVector = leftTablePagesIndex->GetColumn(batchId, columnIndex);
            CopyOutputSegment(inputVector, inputDataType, rowIds.data(), static_cast<int32_t>(rowIds.size()),
                outputVector, buildRowCount);
        }
        buildRowCount += static_cast<int32_t>(rowIds.size());
    }
    if (rightTableOutputColsCount > 0) {
        buildRowCount = initialBuildRowCount;
    }
    int32_t rightRowsCounter = 0;
    for (auto &batchRows: rightGroupedRowsPerBatchId) { // go over the different batches
        auto batchId = batchRows.first;
        const auto &rowIds = batchRows.second;
        for (int32_t columnIdx = 0; columnIdx < rightTableOutputColsCount; columnIdx++) {
            int32_t buildColumnIdx = leftTableOutputColsCount + columnIdx;
            auto columnIndex = rightTableOutputCols[columnIdx];
            auto inputDataType = rightTableOutputTypes[columnIdx];
            auto outputVector = buildVectorBatch->Get(buildColumnIdx);
            auto inputVector = rightTablePagesIndex->GetColumn(batchId, columnIndex);
            CopyOutputSegment(inputVector, inputDataType, rowIds.data(), static_cast<int32_t>(rowIds.size()),
                outputVector, buildRowCount);
        }
        const auto rowsCount = static_cast<int32_t>(rowIds.size());
        rightRowsCounter += rowsCount;
        if (canFreeRights[rightRowsCounter - 1]) {
            this->rightTablePagesIndex->FreeBeforeVecBatch(batchId);
        }
        buildRowCount += rowsCount;
    }
    buildRowCount = counter;
    buildVectorBatchRowCount = buildRowCount;

    return result;
}

void JoinResultBuilder::CopyOutputSegment(BaseVector *inputVector, const DataTypePtr &inputType,
    const int32_t *rows, int32_t rowCount, BaseVector *outputVector, int32_t outputRowId)
{
    if (inputType->GetId() == type::OMNI_VARCHAR && rowCount >= BATCHED_VARCHAR_MIN_ROWS &&
        TryBatchVarcharSegment(inputVector, outputVector, rows, rowCount, outputRowId)) {
        return;
    }
    AddValuesToBuildVector(inputVector, inputType, rows, rowCount, outputVector, outputRowId);
}

void JoinResultBuilder::CopyOutputSelection(const std::vector<int32_t> &batchIds,
    const std::vector<int32_t> &rowIds, DynamicPagesIndex *pagesIndex,
    const std::vector<DataTypePtr> &outputTypes, const int32_t *outputCols, int32_t outputColsCount,
    int32_t columnOffset, int32_t rowBase)
{
    // A segment is a maximal consecutive run in the output selection with the same
    // batchId. Its rowIds may be arbitrary or repeated. Walking runs before columns
    // avoids rescanning batchIds and copying rowIds for every output column.
    const int32_t rowCount = static_cast<int32_t>(batchIds.size());
    for (int32_t start = 0; start < rowCount;) {
        const int32_t batchId = batchIds[start];
        int32_t end = start + 1;
        while (end < rowCount && batchIds[end] == batchId) {
            ++end;
        }
        for (int32_t column = 0; column < outputColsCount; ++column) {
            auto *outputVector = buildVectorBatch->Get(columnOffset + column);
            if (batchId == JOIN_NULL_FLAG) {
                SetRightNullRangeForColumn(outputVector, outputTypes[column], rowBase + start, rowBase + end);
            } else {
                auto *inputVector = pagesIndex->GetColumn(batchId, outputCols[column]);
                CopyOutputSegment(inputVector, outputTypes[column], rowIds.data() + start, end - start,
                    outputVector, rowBase + start);
            }
        }
        start = end;
    }
}

bool JoinResultBuilder::TryBatchVarcharSegment(BaseVector *inputVector, BaseVector *outputVector, const int32_t *rows,
    int32_t rowCount, int32_t outputRowId)
{
    // SetValues only supports an independently allocated output vector.
    if (outputVector->GetEncoding() != OMNI_FLAT || outputVector->IsSliced() || outputVector->GetOffset() != 0) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "invalid batched SMJ VARCHAR output vector");
    }
    // CONST/DICTIONARY sources keep the scalar path. Every check runs before the
    // first output mutation, so a fallback leaves the batch exactly as the scalar
    // path sees it; partially gathered scratch is simply discarded.
    if (inputVector->GetEncoding() != OMNI_FLAT) {
        return false;
    }
    auto *source = static_cast<VarcharVector *>(inputVector);
    auto *target = static_cast<VarcharVector *>(outputVector);
    // GetOffsets already includes the source slice offset and GetValues returns the
    // container base, so views address the raw buffer with local row positions.
    const char *bytes = unsafe::UnsafeStringVector::GetValues(source);
    const int32_t *offsets = unsafe::UnsafeStringVector::GetOffsets(source);
    batchVarcharValues.resize(rowCount);
    batchVarcharNulls.resize(rowCount);
    std::string_view *values = batchVarcharValues.data();
    uint8_t *nulls = batchVarcharNulls.data();
    // Accumulate the exact payload size so SetValues can skip its per-value
    // validation scan. Sources come from PagesIndex pages and never alias this
    // independently allocated output container.
    uint64_t totalBytes = 0;
    for (int32_t i = 0; i < rowCount; ++i) {
        const int32_t row = rows[i];
        // Nulls keep the per-row source semantics; a NULL row skips offset reads and
        // leaves an empty view that SetValues ignores.
        if (source->IsNull(row)) {
            nulls[i] = 1;
            values[i] = std::string_view{};
            continue;
        }
        nulls[i] = 0;
        // Rows of one source batch are gathered, not contiguous: build the view per
        // row instead of memcpy-ing a source string region. Empty strings may have
        // no allocated byte buffer, so avoid nullptr arithmetic.
        const int32_t begin = offsets[row];
        const auto size = static_cast<size_t>(offsets[row + 1] - begin);
        values[i] = size == 0 ? std::string_view{} : std::string_view(bytes + begin, size);
        totalBytes += size;
    }
    target->SetValues(outputRowId, values, nulls, rowCount, totalBytes);
    return true;
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

    CopyOutputSelection(leftOutputBatchIds, leftOutputRowIds, leftTablePagesIndex, leftTableOutputTypes,
        leftTableOutputCols, leftTableOutputColsCount, 0, rowBase);
    CopyOutputSelection(rightOutputBatchIds, rightOutputRowIds, rightTablePagesIndex, rightTableOutputTypes,
        rightTableOutputCols, rightTableOutputColsCount, leftTableOutputColsCount, rowBase);

    // Input vectors are still needed while the output columns are copied. Release only
    // after materialization, and only for addresses consumed by this output batch.
    // Streamed side: every address of this window is materialized above, so the surviving
    // streamed readers are the scanner's pending pre-row key comparison (streamedPinBatchId,
    // wired by the operator) and this builder's pending unmatched row (preStreamedBatchId,
    // always the last processed row here). Free everything strictly below the lowest pin;
    // the PagesIndex monotonic guard makes repeated calls no-ops. Unmatched (right-null-
    // padded) rows carry JOIN_NULL_FLAG on the buffered side but a valid streamed batch —
    // skipping them would freeze the left watermark for joins whose output is dominated
    // by unmatched rows (q3_3 retains ~1000 batches that way).
    int32_t minConsumedInWindow = INT32_MAX;
    bool hasStreamedAddress = false;
    for (int32_t processedAddress = addressStart; processedAddress < addressOffset; processedAddress++) {
        const int32_t leftBatchId = static_cast<int32_t>(DecodeSliceIndex(streamedTableValueAddresses[processedAddress]));
        const int32_t rightBatchId = static_cast<int32_t>(DecodeSliceIndex(bufferedTableValueAddresses[processedAddress]));
        if (rightBatchId != JOIN_NULL_FLAG) {
            FreeRightVectorBatches(isPreKeyMatched[processedAddress], leftBatchId, rightBatchId);
        }
        if (leftBatchId >= 0) {
            hasStreamedAddress = true;
            if (leftBatchId < minConsumedInWindow) {
                minConsumedInWindow = leftBatchId;
            }
        }
    }
    if (hasStreamedAddress) {
        int32_t streamedFreeLimit = minConsumedInWindow;
        // Matched-group lease: with duplicated buffered keys a matched group spans
        // several streamed batches and is consumed over multiple output windows; the
        // floor of every address consumed so far stays guarding those batches until
        // the group completes (cleared in Clear()).
        if (minConsumedStreamedBatchId < streamedFreeLimit) {
            streamedFreeLimit = minConsumedStreamedBatchId;
        }
        if (streamedPinBatchId < streamedFreeLimit) {
            streamedFreeLimit = streamedPinBatchId;
        }
        if (preStreamedBatchId >= 0 && preStreamedBatchId < streamedFreeLimit) {
            streamedFreeLimit = preStreamedBatchId;
        }
        leftTablePagesIndex->FreeBeforeVecBatch(streamedFreeLimit);

        // Publish this window's consumed floor only after the release. Do not track
        // streamedFreeLimit: it may include a temporary scanner or pending-row pin.
        TrackConsumedStreamedBatch(minConsumedInWindow);
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
        leftTablePagesIndex->FreeBeforeVecBatch(leftBatchId);
        rightTablePagesIndex->FreeBeforeVecBatch(rightBatchId);
        lastUnMatchedStreamedBatchId = leftBatchId;
    }
}

void JoinResultBuilder::FreeRightVectorBatches(bool isPreMatched, int32_t leftBatchId, int32_t rightBatchId)
{
    if (!isPreMatched && leftBatchId > lastUnMatchedStreamedBatchId) {
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
