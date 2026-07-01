/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: sort merge join v3 core implementations
 */

#include "sort_merge_join_v3.h"
#include "expression/jsonparser/jsonparser.h"

namespace omniruntime::op {
using namespace omniruntime::vec;

template <type::DataTypeId typeId>
bool EqualValueIgnoreNull(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    RawDataType leftValue;
    RawDataType rightValue;
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        if (leftVec->GetEncoding() == OMNI_DICTIONARY) {
            leftValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        } else {
            leftValue = static_cast<Vector<LargeStringContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        }
        if (rightVec->GetEncoding() == OMNI_DICTIONARY) {
            rightValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        } else {
            rightValue = static_cast<Vector<LargeStringContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        }

        auto leftLength = leftValue.length();
        auto rightLength = rightValue.length();
        if (leftLength != rightLength) {
            return false;
        } else {
            return memcmp(leftValue.data(), rightValue.data(), leftLength) == 0;
        }
    } else {
        if (leftVec->GetEncoding() == OMNI_DICTIONARY) {
            leftValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        } else {
            leftValue = static_cast<Vector<RawDataType> *>(leftVec)->GetValue(leftPos);
        }
        if (rightVec->GetEncoding() == OMNI_DICTIONARY) {
            rightValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        } else {
            rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        }

        if constexpr (typeId == OMNI_DOUBLE) {
            if (std::abs(leftValue - rightValue) < __DBL_EPSILON__) {
                return true;
            } else {
                return false;
            }
        } else {
            return leftValue == rightValue;
        }
    }
}

template <type::DataTypeId typeId>
static int32_t CompareValueFromFlat(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    RawDataType leftValue;
    RawDataType rightValue;
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        if (leftVec->GetEncoding() == OMNI_DICTIONARY) {
            leftValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        } else {
            leftValue = static_cast<Vector<LargeStringContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        }
        if (rightVec->GetEncoding() == OMNI_DICTIONARY) {
            rightValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        } else {
            rightValue = static_cast<Vector<LargeStringContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        }

        auto leftLength = leftValue.length();
        auto rightLength = rightValue.length();
        int32_t result = memcmp(leftValue.data(), rightValue.data(), std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }

        return leftLength - rightLength;
    } else {
        if (leftVec->GetEncoding() == OMNI_DICTIONARY) {
            leftValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        } else {
            leftValue = static_cast<Vector<RawDataType> *>(leftVec)->GetValue(leftPos);
        }
        if (rightVec->GetEncoding() == OMNI_DICTIONARY) {
            rightValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(rightVec)->GetValue(rightPos);
        } else {
            rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        }

        return leftValue > rightValue ? 1 : leftValue < rightValue ? -1 : 0;
    }
}

static std::vector<EqualFunc> equalFromFlatFuncs = {
    nullptr,                               // OMNI_NONE,
    EqualValueIgnoreNull<OMNI_INT>,        // OMNI_INT
    EqualValueIgnoreNull<OMNI_LONG>,       // OMNI_LONG
    EqualValueIgnoreNull<OMNI_DOUBLE>,     // OMNI_DOUBLE
    EqualValueIgnoreNull<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    EqualValueIgnoreNull<OMNI_SHORT>,      // OMNI_SHORT
    EqualValueIgnoreNull<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    EqualValueIgnoreNull<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    EqualValueIgnoreNull<OMNI_DATE32>,     // OMNI_DATE32
    EqualValueIgnoreNull<OMNI_DATE64>,     // OMNI_DATE64
    EqualValueIgnoreNull<OMNI_TIME32>,     // OMNI_TIME32
    EqualValueIgnoreNull<OMNI_TIME64>,     // OMNI_TIME64
    EqualValueIgnoreNull<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                               // OMNI_INTERVAL_MONTHS
    nullptr,                               // OMNI_INTERVAL_DAY_TIME
    EqualValueIgnoreNull<OMNI_VARCHAR>,    // OMNI_VARCHAR
    EqualValueIgnoreNull<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr                                // OMNI_CONTAINER,
};

static std::vector<CompareFunc> compareFromFlatFuncs = {
    nullptr,                               // OMNI_NONE,
    CompareValueFromFlat<OMNI_INT>,        // OMNI_INT
    CompareValueFromFlat<OMNI_LONG>,       // OMNI_LONG
    CompareValueFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    CompareValueFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    CompareValueFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    CompareValueFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    CompareValueFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    CompareValueFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    CompareValueFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    CompareValueFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    CompareValueFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    CompareValueFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                               // OMNI_INTERVAL_MONTHS
    nullptr,                               // OMNI_INTERVAL_DAY_TIME
    CompareValueFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    CompareValueFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr                                // OMNI_CONTAINER,
};

template <typename type::DataTypeId typeId>
static BaseVector *ConstructColumnWithoutNullIndex(BaseVector **outputColumns, uint64_t *outputAddresses,
    int32_t *outputIndexes, int32_t probeRowCount)
{
    if constexpr (typeId == OMNI_VARCHAR) {
        using DictionaryVector = Vector<DictionaryContainer<std::string_view>>;
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto resultColumn = new VarcharVector(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            auto valueIdx = outputIndexes[i];
            auto valueAddress = outputAddresses[valueIdx];
            auto vecBatchIdx = DecodeSliceIndex(valueAddress);
            auto rowIdx = DecodePosition(valueAddress);
            auto column = outputColumns[vecBatchIdx];
            if (column->IsNull(rowIdx)) {
                resultColumn->SetNull(i);
            } else {
                std::string_view value;
                if (column->GetEncoding() == OMNI_DICTIONARY) {
                    value = static_cast<DictionaryVector *>(column)->GetValue(rowIdx);
                } else {
                    value = static_cast<VarcharVector *>(column)->GetValue(rowIdx);
                }
                resultColumn->SetValue(i, value);
            }
        }
        return resultColumn;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto resultColumn = new Vector<RawDataType>(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            auto valueIdx = outputIndexes[i];
            auto valueAddress = outputAddresses[valueIdx];
            auto vecBatchIdx = DecodeSliceIndex(valueAddress);
            auto rowIdx = DecodePosition(valueAddress);
            auto column = outputColumns[vecBatchIdx];
            if (column->IsNull(rowIdx)) {
                resultColumn->SetNull(i);
            } else {
                RawDataType value;
                if (column->GetEncoding() == OMNI_DICTIONARY) {
                    value = static_cast<Vector<DictionaryContainer<RawDataType>> *>(column)->GetValue(rowIdx);
                } else {
                    value = static_cast<Vector<RawDataType> *>(column)->GetValue(rowIdx);
                }
                resultColumn->SetValue(i, value);
            }
        }
        return resultColumn;
    }
}

template <typename type::DataTypeId typeId>
static BaseVector *ConstructColumnWithNullIndex(BaseVector **outputColumns, uint64_t *outputAddresses,
    int32_t *outputIndexes, int32_t probeRowCount)
{
    if constexpr (typeId == OMNI_VARCHAR) {
        using DictionaryVector = Vector<DictionaryContainer<std::string_view>>;
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto resultColumn = new VarcharVector(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            auto valueIdx = outputIndexes[i];
            if (valueIdx == NULL_INDEX) {
                resultColumn->SetNull(i);
                continue;
            }
            auto valueAddress = outputAddresses[valueIdx];
            auto vecBatchIdx = DecodeSliceIndex(valueAddress);
            auto rowIdx = DecodePosition(valueAddress);
            auto column = outputColumns[vecBatchIdx];
            if (column->IsNull(rowIdx)) {
                resultColumn->SetNull(i);
            } else {
                std::string_view value;
                if (column->GetEncoding() == OMNI_DICTIONARY) {
                    value = static_cast<DictionaryVector *>(column)->GetValue(rowIdx);
                } else {
                    value = static_cast<VarcharVector *>(column)->GetValue(rowIdx);
                }
                resultColumn->SetValue(i, value);
            }
        }
        return resultColumn;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto resultColumn = new Vector<RawDataType>(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            auto valueIdx = outputIndexes[i];
            if (valueIdx == NULL_INDEX) {
                resultColumn->SetNull(i);
                continue;
            }
            auto valueAddress = outputAddresses[valueIdx];
            auto vecBatchIdx = DecodeSliceIndex(valueAddress);
            auto rowIdx = DecodePosition(valueAddress);
            auto column = outputColumns[vecBatchIdx];
            if (column->IsNull(rowIdx)) {
                resultColumn->SetNull(i);
            } else {
                RawDataType value;
                if (column->GetEncoding() == OMNI_DICTIONARY) {
                    value = static_cast<Vector<DictionaryContainer<RawDataType>> *>(column)->GetValue(rowIdx);
                } else {
                    value = static_cast<Vector<RawDataType> *>(column)->GetValue(rowIdx);
                }
                resultColumn->SetValue(i, value);
            }
        }
        return resultColumn;
    }
}

template <typename type::DataTypeId typeId> static BaseVector *ConstructNullColumn(int32_t probeRowCount)
{
    if constexpr (typeId == OMNI_VARCHAR) {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto resultColumn = new VarcharVector(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            resultColumn->SetNull(i);
        }
        return resultColumn;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto resultColumn = new Vector<RawDataType>(probeRowCount);
        for (int32_t i = 0; i < probeRowCount; i++) {
            resultColumn->SetNull(i);
        }
        return resultColumn;
    }
}

void SortMergeJoinOperatorV3::ConfigStreamInfo(const DataTypes &streamTypes, const std::vector<int32_t> &streamJoinCols,
    const std::vector<int32_t> &streamOutputCols, int32_t originalStreamColCount)
{
    this->streamTypes = streamTypes;
    this->streamJoinCols = streamJoinCols;
    this->streamOutputCols = streamOutputCols;
    this->originalStreamColCount = originalStreamColCount;
    this->streamPagesIndex = new PagesIndex(streamTypes);

    this->streamJoinColCount = static_cast<int32_t>(streamJoinCols.size());
    this->streamJoinColTypes = new int32_t[this->streamJoinColCount];
    this->streamSortAscendings = new int32_t[this->streamJoinColCount];
    this->streamSortNullFirsts = new int32_t[this->streamJoinColCount];
    auto streamTypeIds = streamTypes.GetIds();
    for (int32_t i = 0; i < this->streamJoinColCount; i++) {
        this->streamJoinColTypes[i] = streamTypeIds[streamJoinCols[i]];
        this->streamSortAscendings[i] = 1;
        this->streamSortNullFirsts[i] = 1;
    }

    this->streamOutputColCount = static_cast<int32_t>(streamOutputCols.size());
    this->streamOutputColTypeIds = new int32_t[this->streamOutputColCount];
    for (int32_t i = 0; i < this->streamOutputColCount; i++) {
        auto outputCol = streamOutputCols[i];
        this->streamOutputColTypeIds[i] = streamTypeIds[outputCol];
        this->outputTypes.emplace_back(streamTypes.GetType(outputCol));
    }
}

void SortMergeJoinOperatorV3::ConfigBufferInfo(const DataTypes &bufferTypes, const std::vector<int32_t> &bufferJoinCols,
    const std::vector<int32_t> &bufferOutputCols, int32_t originalBufferColCount)
{
    this->bufferTypes = bufferTypes;
    this->bufferJoinCols = bufferJoinCols;
    this->bufferOutputCols = bufferOutputCols;
    this->originalBufferColCount = originalBufferColCount;
    this->bufferPagesIndex = new PagesIndex(bufferTypes);

    this->bufferJoinColCount = static_cast<int32_t>(bufferJoinCols.size());
    this->bufferJoinColTypes = new int32_t[this->bufferJoinColCount];
    this->bufferSortAscendings = new int32_t[this->bufferJoinColCount];
    this->bufferSortNullFirsts = new int32_t[this->bufferJoinColCount];
    auto bufferTypeIds = bufferTypes.GetIds();
    for (int32_t i = 0; i < this->bufferJoinColCount; i++) {
        this->bufferJoinColTypes[i] = bufferTypeIds[bufferJoinCols[i]];
        this->bufferSortAscendings[i] = 1;
        this->bufferSortNullFirsts[i] = 1;
    }

    this->bufferOutputColCount = static_cast<int32_t>(bufferOutputCols.size());
    this->bufferOutputColTypeIds = new int32_t[this->bufferOutputColCount];
    for (int32_t i = 0; i < this->bufferOutputColCount; i++) {
        auto outputCol = bufferOutputCols[i];
        this->bufferOutputColTypeIds[i] = bufferTypeIds[outputCol];
        this->outputTypes.emplace_back(bufferTypes.GetType(outputCol));
    }
}

void SortMergeJoinOperatorV3::JoinFilterCodeGen(OverflowConfig *overflowConfig)
{
    if (filter.empty()) {
        return;
    }

    omniruntime::expressions::Expr *filterExpr = JSONParser::ParseJSON(filter);
    simpleFilter = new SimpleFilter(*filterExpr);
    auto isOk = simpleFilter->Initialize(overflowConfig);
    delete filterExpr;
    if (!isOk) {
        delete simpleFilter;
        simpleFilter = nullptr;
        throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT", "The expression is not supported yet.");
    }

    auto streamTypeIds = streamTypes.GetIds();
    auto bufferTypeIds = bufferTypes.GetIds();
    streamFilterCols = new int32_t[originalStreamColCount]();
    streamFilterColTypes = new int32_t[originalStreamColCount]();
    bufferFilterCols = new int32_t[originalBufferColCount]();
    bufferFilterColTypes = new int32_t[originalBufferColCount]();
    auto colsInFilter = simpleFilter->GetVectorIndexes();
    for (auto col : colsInFilter) {
        if (col < originalStreamColCount) {
            streamFilterCols[streamFilterColCount] = col;
            streamFilterColTypes[streamFilterColCount] = streamTypeIds[col];
            streamFilterColCount++;
        } else {
            bufferFilterCols[bufferFilterColCount] = col;
            bufferFilterColTypes[bufferFilterColCount] = bufferTypeIds[col - originalStreamColCount];
            bufferFilterColCount++;
        }
    }
    auto originalAllColsCount = originalStreamColCount + originalBufferColCount;
    values = new int64_t[originalAllColsCount];
    nulls = new bool[originalAllColsCount];
    lengths = new int32_t[originalAllColsCount];
    memset(values, 0, sizeof(int64_t) * originalAllColsCount);
    memset(nulls, 0, sizeof(bool) * originalAllColsCount);
    memset(lengths, 0, sizeof(int32_t) * originalAllColsCount);
}

int32_t SortMergeJoinOperatorV3::AddStreamInput(VectorBatch *vecBatch)
{
    streamPagesIndex->AddVecBatch(vecBatch);
    return 0;
}

int32_t SortMergeJoinOperatorV3::AddBufferInput(VectorBatch *vecBatch)
{
    bufferPagesIndex->AddVecBatch(vecBatch);
    return 0;
}

OmniStatus SortMergeJoinOperatorV3::GetOutput(VectorBatch **outputVecBatch)
{
    // first sort and get ranges for stream and buffer data
    if (isFirst) {
        Prepare();
        isFirst = false;
    }

    if (!skipNullRange) {
        // second probe null range for stream and buffer
        status = ProbeNullRange();
        if (status == OMNI_STATUS_FINISHED) {
            if (probeRowCount > 0) {
                BuildOutput(outputVecBatch, probeRowCount);
            }
            return OMNI_STATUS_FINISHED;
        }

        // skip null values for stream and buffer
        skipNullRange = true;
        preStreamRangeIdx = 0;
        curStreamRangeIdx = 1;
        preStreamRangeIdx = 0;
        curBufferRangeIdx = 1;
    }

    if (IsFull()) {
        BuildOutput(outputVecBatch, maxRowCount);
        if (status != OMNI_STATUS_FINISHED) {
            return OMNI_STATUS_NORMAL;
        } else if (probeRowCount <= 0) {
            return OMNI_STATUS_FINISHED;
        } else {
            return OMNI_STATUS_NORMAL;
        }
    }
    // third probe non null ranges for stream and buffer
    if (status == OMNI_STATUS_NORMAL) {
        status = ProbeNonNullRanges(simpleFilter != nullptr);
    }

    if (probeRowCount > 0) {
        BuildOutput(outputVecBatch, std::min(probeRowCount, maxRowCount));
    }
    if (probeRowCount == 0 && status == OMNI_STATUS_FINISHED) {
        return OMNI_STATUS_FINISHED;
    }
    return OMNI_STATUS_NORMAL;
}

void SortMergeJoinOperatorV3::Prepare()
{
    int32_t rowSize = OperatorUtil::GetRowSize(outputTypes);
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(
        rowSize == 0 ? DEFAULT_ROW_SIZE : rowSize, executionContext != nullptr ? &executionContext->queryConfigRef() : nullptr);
    streamIndexes.reserve(maxRowCount);
    if (joinType != OMNI_JOIN_TYPE_LEFT_SEMI && joinType != OMNI_JOIN_TYPE_LEFT_ANTI) {
        bufferIndexes.reserve(maxRowCount);
    }

    for (int32_t i = 0; i < bufferJoinColCount; i++) {
        auto joinColType = bufferJoinColTypes[i];
        equalFuncs.emplace_back(equalFromFlatFuncs[joinColType]);
        compareFuncs.emplace_back(compareFromFlatFuncs[joinColType]);
    }
    if (streamPagesIndex->GetRowCount() > 0) {
        PrepareForStream();
    }
    if (bufferPagesIndex->GetRowCount() > 0) {
        PrepareForBuffer();
    }
}

void SortMergeJoinOperatorV3::PrepareForStream()
{
    // first step sort
    streamPagesIndex->Prepare();
    auto streamRowCount = streamPagesIndex->GetRowCount();
    streamPagesIndex->Sort(streamJoinCols.data(), streamSortAscendings, streamSortNullFirsts, streamJoinColCount, 0,
        streamRowCount);

    // second step prepare join columns, filter columns
    auto streamAddresses = streamPagesIndex->GetValueAddresses();
    auto streamColumns = streamPagesIndex->GetColumns();
    streamJoinColumns = new BaseVector **[streamJoinColCount];
    for (int32_t i = 0; i < streamJoinColCount; i++) {
        streamJoinColumns[i] = streamColumns[streamJoinCols[i]];
    }
    if (simpleFilter != nullptr) {
        streamFilterColumns = new BaseVector **[streamFilterColCount];
        for (int32_t i = 0; i < streamFilterColCount; i++) {
            streamFilterColumns[i] = streamColumns[streamFilterCols[i]];
        }
        streamFilterRows = new BaseVector *[streamRowCount * streamFilterColCount];
    }

    streamRowIdxes.resize(streamRowCount);
    streamJoinRows = new BaseVector *[streamRowCount * streamJoinColCount];
    for (int64_t i = 0; i < streamRowCount; i++) {
        auto address = streamAddresses[i];
        auto vecBatchIdx = DecodeSliceIndex(address);
        auto rowIdx = DecodePosition(address);
        streamRowIdxes[i] = rowIdx;

        auto streamJoinRow = streamJoinRows + i * streamJoinColCount;
        for (int32_t colIdx = 0; colIdx < streamJoinColCount; colIdx++) {
            auto vector = streamJoinColumns[colIdx][vecBatchIdx];
            streamJoinRow[colIdx] = vector;
        }
        if (streamFilterColCount > 0) {
            auto streamFilterRow = streamFilterRows + i * streamFilterColCount;
            for (int32_t colIdx = 0; colIdx < streamFilterColCount; colIdx++) {
                auto vector = streamFilterColumns[colIdx][vecBatchIdx];
                streamFilterRow[colIdx] = vector;
            }
        }
    }

    // third step prepare output columns
    streamOutputColumns = new BaseVector **[streamOutputColCount];
    for (int32_t i = 0; i < streamOutputColCount; i++) {
        streamOutputColumns[i] = streamColumns[streamOutputCols[i]];
    }

    // fourth step divide ranges
    DivideRanges(streamJoinRows, streamRowIdxes, streamJoinColCount, streamRanges);
}

void SortMergeJoinOperatorV3::PrepareForBuffer()
{
    // first step sort
    bufferPagesIndex->Prepare();
    auto bufferRowCount = bufferPagesIndex->GetRowCount();
    bufferPagesIndex->Sort(bufferJoinCols.data(), bufferSortAscendings, bufferSortNullFirsts, bufferJoinColCount, 0,
        bufferRowCount);

    // second step prepare join columns, filter columns
    auto bufferAddresses = bufferPagesIndex->GetValueAddresses();
    auto bufferColumns = bufferPagesIndex->GetColumns();
    bufferJoinColumns = new BaseVector **[bufferJoinColCount];
    for (int32_t i = 0; i < bufferJoinColCount; i++) {
        bufferJoinColumns[i] = bufferColumns[bufferJoinCols[i]];
    }
    if (simpleFilter != nullptr) {
        bufferFilterColumns = new BaseVector **[bufferFilterColCount];
        for (int32_t i = 0; i < bufferFilterColCount; i++) {
            bufferFilterColumns[i] = bufferColumns[bufferFilterCols[i] - originalStreamColCount];
        }
        bufferFilterRows = new BaseVector *[bufferRowCount * bufferFilterColCount];
    }

    bufferRowIdxes.resize(bufferRowCount);
    bufferJoinRows = new BaseVector *[bufferRowCount * bufferJoinColCount];
    for (int64_t i = 0; i < bufferRowCount; i++) {
        auto address = bufferAddresses[i];
        auto vecBatchIdx = DecodeSliceIndex(address);
        auto rowIdx = DecodePosition(address);
        bufferRowIdxes[i] = rowIdx;

        auto bufferJoinRow = bufferJoinRows + i * bufferJoinColCount;
        for (int32_t colIdx = 0; colIdx < bufferJoinColCount; colIdx++) {
            auto vector = bufferJoinColumns[colIdx][vecBatchIdx];
            bufferJoinRow[colIdx] = vector;
        }
        if (bufferFilterColCount > 0) {
            auto bufferFilterRow = bufferFilterRows + i * bufferFilterColCount;
            for (int32_t colIdx = 0; colIdx < bufferFilterColCount; colIdx++) {
                auto vector = bufferFilterColumns[colIdx][vecBatchIdx];
                bufferFilterRow[colIdx] = vector;
            }
        }
    }

    // third step prepare output columns
    bufferOutputColumns = new BaseVector **[bufferOutputColCount];
    for (int32_t i = 0; i < bufferOutputColCount; i++) {
        bufferOutputColumns[i] = bufferColumns[bufferOutputCols[i]];
    }

    // fourth step divide ranges
    DivideRanges(bufferJoinRows, bufferRowIdxes, bufferJoinColCount, bufferRanges);
}

void SortMergeJoinOperatorV3::DivideRanges(vec::BaseVector **joinRows, std::vector<int32_t> &rowIdxes,
    int32_t joinColCount, std::vector<IndexRange> &ranges)
{
    auto rowCount = rowIdxes.size();
    size_t pos = 0;
    // find null range
    for (; pos < rowCount; pos++) {
        bool hasNull = false;
        auto joinRow = joinRows + pos * joinColCount;
        auto rowIdx = rowIdxes[pos];
        for (int32_t colIdx = 0; colIdx < joinColCount; colIdx++) {
            auto curVec = joinRow[colIdx];
            if (curVec->IsNull(rowIdx)) {
                hasNull = true;
                break;
            }
        }
        if (!hasNull) {
            // find the first position where all values in the row are not null
            break;
        }
    }
    IndexRange nullRange(0, static_cast<int32_t>(pos));
    ranges.emplace_back(nullRange);
    if (pos == rowCount) {
        return;
    }

    auto prePos = pos;
    auto preJoinRow = joinRows + pos * joinColCount;
    auto preRowIdx = rowIdxes[pos];
    for (pos = prePos + 1; pos < rowCount; pos++) {
        bool isEqual = true;
        auto curJoinRow = joinRows + pos * joinColCount;
        auto curRowIdx = rowIdxes[pos];
        for (int32_t colIdx = 0; colIdx < joinColCount; colIdx++) {
            if (!equalFuncs[colIdx](preJoinRow[colIdx], preRowIdx, curJoinRow[colIdx], curRowIdx)) {
                isEqual = false;
                break;
            }
        }
        if (!isEqual) {
            IndexRange range(prePos, pos);
            ranges.emplace_back(range);
            prePos = pos;
            preJoinRow = curJoinRow;
            preRowIdx = curRowIdx;
        }
    }
    IndexRange range(prePos, pos);
    ranges.emplace_back(range);
}

void SortMergeJoinOperatorV3::DivideRanges(PagesIndex *pagesIndex, BaseVector ***joinColumns, int32_t joinColCount,
    std::vector<IndexRange> &ranges)
{
    auto rowCount = pagesIndex->GetRowCount();
    auto addresses = pagesIndex->GetValueAddresses();
    int64_t pos = 0;
    // find null range
    for (; pos < rowCount; pos++) {
        auto curAddress = addresses[pos];
        auto curVecBatchIdx = DecodeSliceIndex(curAddress);
        auto curRowIdx = DecodePosition(curAddress);
        bool hasNull = false;
        for (int32_t colIdx = 0; colIdx < joinColCount; colIdx++) {
            auto curVec = joinColumns[colIdx][curVecBatchIdx];
            if (curVec->IsNull(curRowIdx)) {
                hasNull = true;
                break;
            }
        }
        if (!hasNull) {
            // find the first position where all values in the row are not null
            break;
        }
    }
    IndexRange nullRange(0, static_cast<int32_t>(pos));
    ranges.emplace_back(nullRange);
    if (pos == rowCount) {
        return;
    }

    auto prePos = pos;
    auto preAddress = addresses[prePos];
    auto preVecBatchIdx = DecodeSliceIndex(preAddress);
    auto preRowIdx = DecodePosition(preAddress);
    for (pos = prePos + 1; pos < rowCount; pos++) {
        bool isEqual = true;
        auto curAddress = addresses[pos];
        auto curVecBatchIdx = DecodeSliceIndex(curAddress);
        auto curRowIdx = DecodePosition(curAddress);
        for (int32_t colIdx = 0; colIdx < joinColCount; colIdx++) {
            auto columns = joinColumns[colIdx];
            if (!equalFuncs[colIdx](columns[preVecBatchIdx], preRowIdx, columns[curVecBatchIdx], curRowIdx)) {
                isEqual = false;
                break;
            }
        }
        if (!isEqual) {
            IndexRange range(prePos, pos);
            ranges.emplace_back(range);
            prePos = pos;
            preVecBatchIdx = curVecBatchIdx;
            preRowIdx = curRowIdx;
        }
    }
    IndexRange range(prePos, pos);
    ranges.emplace_back(range);
}

OmniStatus SortMergeJoinOperatorV3::ProbeNullRange()
{
    auto streamRangeSize = streamRanges.size();
    auto bufferRangeSize = bufferRanges.size();
    OmniStatus resultStatus = OMNI_STATUS_NORMAL;
    switch (joinType) {
        case OMNI_JOIN_TYPE_INNER:
        case OMNI_JOIN_TYPE_LEFT_SEMI: {
            if (streamRangeSize <= 1 || bufferRangeSize <= 1) {
                // no data or all data is null
                resultStatus = OMNI_STATUS_FINISHED;
            }
            break;
        }
        case OMNI_JOIN_TYPE_LEFT: {
            if (streamRangeSize == 0) {
                // no data
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }
            if (bufferRangeSize <= 1) {
                // buffer no data or all data is null
                for (uint32_t i = 0; i < streamRangeSize; i++) {
                    AppendStreamValuesAndBufferNulls(i);
                }
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }

            // handle stream null range
            AppendStreamValuesAndBufferNulls(0);
            if (streamRangeSize == 1) {
                // stream all data is null
                resultStatus = OMNI_STATUS_FINISHED;
            }
            break;
        }
        case OMNI_JOIN_TYPE_LEFT_ANTI: {
            if (streamRangeSize == 0) {
                // no data
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }
            if (bufferRangeSize <= 1) {
                // buffer no data or all data is null
                for (uint32_t i = 0; i < streamRangeSize; i++) {
                    AppendStreamValues(i);
                }
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }

            // handle stream null range
            AppendStreamValues(0);
            if (streamRangeSize == 1) {
                // stream all data is null
                resultStatus = OMNI_STATUS_FINISHED;
            }
            break;
        }
        case OMNI_JOIN_TYPE_RIGHT: {
            if (bufferRangeSize == 0) {
                // no data
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }
            if (streamRangeSize <= 1) {
                // stream no data or all data is null
                for (uint32_t i = 0; i < bufferRangeSize; i++) {
                    AppendBufferValuesAndStreamNulls(i);
                }
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }
            // handle buffer null range
            AppendBufferValuesAndStreamNulls(0);
            if (bufferRangeSize == 1) {
                // buffer all data is null
                resultStatus = OMNI_STATUS_FINISHED;
            }
            break;
        }
        case OMNI_JOIN_TYPE_FULL: {
            if (bufferRangeSize <= 1 || streamRangeSize <= 1) {
                // buffer no data or all data is null
                for (uint32_t i = 0; i < streamRangeSize; i++) {
                    AppendStreamValuesAndBufferNulls(i);
                }
                // stream no data or all data is null
                for (uint32_t i = 0; i < bufferRangeSize; i++) {
                    AppendBufferValuesAndStreamNulls(i);
                }
                resultStatus = OMNI_STATUS_FINISHED;
                break;
            }
            // handle stream null range
            AppendStreamValuesAndBufferNulls(0);
            // handle buffer null range
            AppendBufferValuesAndStreamNulls(0);
            break;
        }
        default:
            throw OmniException("OPERATOR_RUNTIME_ERROR",
                "Unsupported the join type " + std::to_string(joinType) + " currently.");
    }
    return resultStatus;
}

OmniStatus SortMergeJoinOperatorV3::ProbeNonNullRanges(bool hasFilter)
{
    switch (joinType) {
        case OMNI_JOIN_TYPE_INNER: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_INNER, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_INNER, false>();
            }
        }
        case OMNI_JOIN_TYPE_LEFT: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT, false>();
            }
        }
        case OMNI_JOIN_TYPE_RIGHT: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_RIGHT, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_RIGHT, false>();
            }
        }
        case OMNI_JOIN_TYPE_FULL: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_FULL, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_FULL, false>();
            }
        }
        case OMNI_JOIN_TYPE_LEFT_SEMI: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT_SEMI, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT_SEMI, false>();
            }
        }
        case OMNI_JOIN_TYPE_LEFT_ANTI: {
            if (hasFilter) {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT_ANTI, true>();
            } else {
                return ProbeNonNullRanges<OMNI_JOIN_TYPE_LEFT_ANTI, false>();
            }
        }
        default:
            throw OmniException("OPERATOR_RUNTIME_ERROR",
                "Unsupported the join type " + std::to_string(joinType) + " currently.");
    }
}

template <JoinType joinType, bool hasJoinFilter> OmniStatus SortMergeJoinOperatorV3::ProbeNonNullRanges()
{
    int32_t streamRangeSize = streamRanges.size();
    int32_t bufferRangeSize = bufferRanges.size();
    while (curStreamRangeIdx < streamRangeSize && curBufferRangeIdx < bufferRangeSize) {
        if (curStreamRangeIdx != preStreamRangeIdx) {
            UpdateStreamRangeCursor();
            preStreamRangeIdx = curStreamRangeIdx;
        }
        if (curBufferRangeIdx != preBufferRangeIdx) {
            UpdateBufferRangeCursor();
            preBufferRangeIdx = curBufferRangeIdx;
        }
        int32_t result = CompareJoinKeys();
        if (result > 0) {
            // stream join values are larger than buffer join values
            if constexpr (joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) {
                AppendBufferValuesAndStreamNulls(curBufferRangeIdx);
            }
            curBufferRangeIdx++;
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        } else if (result < 0) {
            // stream join values are smaller than buffer join values
            if constexpr (joinType == OMNI_JOIN_TYPE_LEFT || joinType == OMNI_JOIN_TYPE_FULL) {
                AppendStreamValuesAndBufferNulls(curStreamRangeIdx);
            }
            if constexpr (joinType == OMNI_JOIN_TYPE_LEFT_ANTI) {
                AppendStreamValues(curStreamRangeIdx);
            }
            curStreamRangeIdx++;
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        } else {
            // stream join values are equal to buffer join values
            // append batch for stream and buffer
            auto streamAddresses = streamPagesIndex->GetValueAddresses();
            auto bufferAddresses = bufferPagesIndex->GetValueAddresses();
            HandleMatch<joinType, hasJoinFilter>(streamAddresses, bufferAddresses);
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        }
    }

    if constexpr (joinType == OMNI_JOIN_TYPE_LEFT || joinType == OMNI_JOIN_TYPE_FULL) {
        while (curStreamRangeIdx < streamRangeSize) {
            AppendStreamValuesAndBufferNulls(curStreamRangeIdx);
            curStreamRangeIdx++;
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        }
    }
    if constexpr (joinType == OMNI_JOIN_TYPE_LEFT_ANTI) {
        while (curStreamRangeIdx < streamRangeSize) {
            AppendStreamValues(curStreamRangeIdx);
            curStreamRangeIdx++;
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        }
    }
    if constexpr (joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) {
        while (curBufferRangeIdx < bufferRangeSize) {
            AppendBufferValuesAndStreamNulls(curBufferRangeIdx);
            curBufferRangeIdx++;
            if (IsFull()) {
                return OMNI_STATUS_NORMAL;
            }
        }
    }

    return OMNI_STATUS_FINISHED;
}

template <JoinType joinType, bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatch(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    if constexpr (joinType == OMNI_JOIN_TYPE_INNER) {
        HandleMatchForInnerJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else if constexpr (joinType == OMNI_JOIN_TYPE_LEFT) {
        HandleMatchForLeftJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else if constexpr (joinType == OMNI_JOIN_TYPE_RIGHT) {
        HandleMatchForRightJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else if constexpr (joinType == OMNI_JOIN_TYPE_FULL) {
        HandleMatchForFullJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else if constexpr (joinType == OMNI_JOIN_TYPE_LEFT_SEMI) {
        HandleMatchForLeftSemiJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else if constexpr (joinType == OMNI_JOIN_TYPE_LEFT_ANTI) {
        HandleMatchForLeftAntiJoin<hasJoinFilter>(streamAddresses, bufferAddresses);
    } else {
        throw OmniException("OPERATOR_RUNTIME_ERROR",
            "Unsupported the join type " + std::to_string(joinType) + " currently.");
    }
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForInnerJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
    auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;
    for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
        if constexpr (hasJoinFilter) {
            auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
            auto streamRowIdx = streamRowIdxes[streamIdx];
            CopyPositionStreamRow(streamRow, streamRowIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
                auto bufferRowIdx = bufferRowIdxes[bufferIdx];
                CopyPositionBufferRow(bufferRow, bufferRowIdx);
                if (IsJoinPositionEligible()) {
                    streamIndexes.emplace_back(streamIdx);
                    bufferIndexes.emplace_back(bufferIdx);
                    probeRowCount++;
                }
            }
        } else {
            auto bufferRowCount = curBufferRangeEnd - curPosInBufferRange;
            streamIndexes.insert(streamIndexes.end(), bufferRowCount, streamIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                bufferIndexes.emplace_back(bufferIdx);
            }
            probeRowCount += bufferRowCount;
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForLeftJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
    auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;
    for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
        if constexpr (hasJoinFilter) {
            bool hasProduceRow = false;
            auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
            auto streamRowIdx = streamRowIdxes[streamIdx];
            CopyPositionStreamRow(streamRow, streamRowIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
                auto bufferRowIdx = bufferRowIdxes[bufferIdx];
                CopyPositionBufferRow(bufferRow, bufferRowIdx);
                if (IsJoinPositionEligible()) {
                    streamIndexes.emplace_back(streamIdx);
                    bufferIndexes.emplace_back(bufferIdx);
                    probeRowCount++;
                    hasProduceRow = true;
                }
            }
            if (!hasProduceRow) {
                streamIndexes.emplace_back(streamIdx);
                bufferIndexes.emplace_back(NULL_INDEX);
                probeRowCount++;
            }
        } else {
            auto bufferRowCount = curBufferRangeEnd - curPosInBufferRange;
            streamIndexes.insert(streamIndexes.end(), bufferRowCount, streamIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                bufferIndexes.emplace_back(bufferIdx);
            }
            probeRowCount += bufferRowCount;
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForRightJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
    auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;
    if constexpr (!hasJoinFilter) {
        for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
            auto bufferRowCount = curBufferRangeEnd - curPosInBufferRange;
            streamIndexes.insert(streamIndexes.end(), bufferRowCount, streamIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                bufferIndexes.emplace_back(bufferIdx);
            }
            probeRowCount += bufferRowCount;
        }
        curStreamRangeIdx++;
        curBufferRangeIdx++;
        return;
    }

    auto curBufferRowCount = curBufferRangeEnd - curPosInBufferRange;
    bool curBufferVisited[curBufferRowCount];
    memset(curBufferVisited, 0, sizeof(bool) * curBufferRowCount);
    for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
        auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
        auto streamRowIdx = streamRowIdxes[streamIdx];
        CopyPositionStreamRow(streamRow, streamRowIdx);
        int32_t bufferVisitIdx = 0;
        for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
            auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
            auto bufferRowIdx = bufferRowIdxes[bufferIdx];
            CopyPositionBufferRow(bufferRow, bufferRowIdx);
            if (IsJoinPositionEligible()) {
                streamIndexes.emplace_back(streamIdx);
                bufferIndexes.emplace_back(bufferIdx);
                curBufferVisited[bufferVisitIdx++] = true;
                probeRowCount++;
            }
        }
    }
    for (int32_t i = 0; i < curBufferRowCount; i++) {
        if (!curBufferVisited[i]) {
            streamIndexes.emplace_back(NULL_INDEX);
            bufferIndexes.emplace_back(i + curPosInBufferRange);
            probeRowCount++;
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForFullJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
    auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;

    if constexpr (!hasJoinFilter) {
        for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
            auto bufferRowCount = curBufferRangeEnd - curPosInBufferRange;
            streamIndexes.insert(streamIndexes.end(), bufferRowCount, streamIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                bufferIndexes.emplace_back(bufferIdx);
            }
            probeRowCount += bufferRowCount;
        }
        curStreamRangeIdx++;
        curBufferRangeIdx++;
        return;
    }

    auto curBufferRowCount = curBufferRangeEnd - curPosInBufferRange;
    bool curBufferVisited[curBufferRowCount];
    memset(curBufferVisited, 0, sizeof(bool) * curBufferRowCount);
    for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
        bool hasProduceRow = false;
        auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
        auto streamRowIdx = streamRowIdxes[streamIdx];
        CopyPositionStreamRow(streamRow, streamRowIdx);
        int32_t bufferVisitIdx = 0;
        for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
            auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
            auto bufferRowIdx = bufferRowIdxes[bufferIdx];
            CopyPositionBufferRow(bufferRow, bufferRowIdx);
            if (IsJoinPositionEligible()) {
                streamIndexes.emplace_back(streamIdx);
                bufferIndexes.emplace_back(bufferIdx);
                curBufferVisited[bufferVisitIdx++] = true;
                probeRowCount++;
                hasProduceRow = true;
            }
        }
        if (!hasProduceRow) {
            streamIndexes.emplace_back(streamIdx);
            bufferIndexes.emplace_back(NULL_INDEX);
            probeRowCount++;
        }
    }
    for (int32_t i = 0; i < curBufferRowCount; i++) {
        if (!curBufferVisited[i]) {
            streamIndexes.emplace_back(NULL_INDEX);
            bufferIndexes.emplace_back(i + curPosInBufferRange);
            probeRowCount++;
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForLeftSemiJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
    auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;
    for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
        if constexpr (hasJoinFilter) {
            auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
            auto streamRowIdx = streamRowIdxes[streamIdx];
            CopyPositionStreamRow(streamRow, streamRowIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
                auto bufferRowIdx = bufferRowIdxes[bufferIdx];
                CopyPositionBufferRow(bufferRow, bufferRowIdx);
                if (IsJoinPositionEligible()) {
                    streamIndexes.emplace_back(streamIdx);
                    probeRowCount++;
                    break;
                }
            }
        } else {
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                streamIndexes.emplace_back(streamIdx);
                probeRowCount++;
                break;
            }
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

template <bool hasJoinFilter>
void SortMergeJoinOperatorV3::HandleMatchForLeftAntiJoin(uint64_t *streamAddresses, uint64_t *bufferAddresses)
{
    if constexpr (hasJoinFilter) {
        auto curStreamRangeEnd = streamRanges[curStreamRangeIdx].end;
        auto curBufferRangeEnd = bufferRanges[curBufferRangeIdx].end;
        for (int32_t streamIdx = curPosInStreamRange; streamIdx < curStreamRangeEnd; streamIdx++) {
            bool hasProduceRow = false;
            auto streamRow = streamFilterRows + streamIdx * streamFilterColCount;
            auto streamRowIdx = streamRowIdxes[streamIdx];
            CopyPositionStreamRow(streamRow, streamRowIdx);
            for (int32_t bufferIdx = curPosInBufferRange; bufferIdx < curBufferRangeEnd; bufferIdx++) {
                auto bufferRow = bufferFilterRows + bufferIdx * bufferFilterColCount;
                auto bufferRowIdx = bufferRowIdxes[bufferIdx];
                CopyPositionBufferRow(bufferRow, bufferRowIdx);
                if (IsJoinPositionEligible()) {
                    hasProduceRow = true;
                    break;
                }
            }
            if (!hasProduceRow) {
                streamIndexes.emplace_back(streamIdx);
                probeRowCount++;
            }
        }
    }
    curStreamRangeIdx++;
    curBufferRangeIdx++;
}

void SortMergeJoinOperatorV3::BuildOutput(VectorBatch **outputVecBatch, int32_t rowCount)
{
    auto output = std::make_unique<VectorBatch>(rowCount);

    if (streamOutputColCount > 0) {
        ConstructResultColumns(true, output.get(), rowCount);
    }
    if (bufferOutputColCount > 0) {
        ConstructResultColumns(false, output.get(), rowCount);
    }
    *outputVecBatch = output.release();

    probeRowCount -= rowCount;
    probeOffset += rowCount;
    if (probeRowCount == 0) {
        streamIndexes.clear();
        bufferIndexes.clear();
        probeOffset = 0;
    }
}

void SortMergeJoinOperatorV3::ConstructResultColumns(bool isStream, VectorBatch *output, int32_t rowCount)
{
    BaseVector ***outputColumns;
    uint64_t *outputAddresses;
    int32_t outputColCount;
    int32_t *outputTypeIds;
    int32_t *outputIndexes;
    bool needHandleNullIndex = false;
    if (isStream) {
        outputColumns = streamOutputColumns;
        outputAddresses = streamPagesIndex->GetValueAddresses();
        outputIndexes = streamIndexes.data() + probeOffset;
        outputColCount = streamOutputColCount;
        outputTypeIds = streamOutputColTypeIds;
        if (joinType == OMNI_JOIN_TYPE_RIGHT || joinType == OMNI_JOIN_TYPE_FULL) {
            needHandleNullIndex = true;
        }
    } else {
        outputColumns = bufferOutputColumns;
        outputAddresses = bufferPagesIndex->GetValueAddresses();
        outputIndexes = bufferIndexes.data() + probeOffset;
        outputColCount = bufferOutputColCount;
        outputTypeIds = bufferOutputColTypeIds;
        if (joinType == OMNI_JOIN_TYPE_LEFT || joinType == OMNI_JOIN_TYPE_FULL) {
            needHandleNullIndex = true;
        }
    }
    if (outputColumns == nullptr) {
        // the input no data
        for (int32_t colIdx = 0; colIdx < outputColCount; colIdx++) {
            auto resultColumn = DYNAMIC_TYPE_DISPATCH(ConstructNullColumn, outputTypeIds[colIdx], rowCount);
            output->Append(resultColumn);
        }
        return;
    }

    if (needHandleNullIndex) {
        for (int32_t colIdx = 0; colIdx < outputColCount; colIdx++) {
            auto givenColumns = outputColumns[colIdx];
            auto resultColumn = DYNAMIC_TYPE_DISPATCH(ConstructColumnWithNullIndex, outputTypeIds[colIdx], givenColumns,
                outputAddresses, outputIndexes, rowCount);
            output->Append(resultColumn);
        }
    } else {
        for (int32_t colIdx = 0; colIdx < outputColCount; colIdx++) {
            auto givenColumns = outputColumns[colIdx];
            auto resultColumn = DYNAMIC_TYPE_DISPATCH(ConstructColumnWithoutNullIndex, outputTypeIds[colIdx],
                givenColumns, outputAddresses, outputIndexes, rowCount);
            output->Append(resultColumn);
        }
    }
}
}
