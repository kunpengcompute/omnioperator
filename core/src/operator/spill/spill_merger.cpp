/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: spill merger implementation
 */

#include <unistd.h>
#include "spill_merger.h"

namespace omniruntime {
namespace op {
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

static constexpr size_t NUMBER_OF_ELEMENTS = 1;

SpillReader::~SpillReader()
{
    // the spill file can be deleted if there is exception
    remove(filePath.c_str());
}

ErrorCode SpillReader::ReadVecBatch(std::unique_ptr<vec::VectorBatch> &vectorBatch, bool &isEnd)
{
    if (rowOffset >= totalRowCount) {
        isEnd = true;
        fclose(file);
        // the spill file can be deleted if all data has been read
        remove(filePath.c_str());
        return ErrorCode::SUCCESS;
    }

    if (file == nullptr) {
        file = fopen(filePath.c_str(), "r");
        if (file == nullptr) {
            auto errorNum = errno;
            char errorBuf[ERROR_BUFFER_SIZE];
            GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
            LogError("Open file %s when read failed since %s.", filePath.c_str(), errorBuf);
            isEnd = true;
            return ErrorCode::READ_FAILED;
        }
    }

    int32_t rowCount = 0;
    if (Read(&rowCount, sizeof(int32_t)) != ErrorCode::SUCCESS) {
        isEnd = true;
        fclose(file);
        LogError("Read row count from %s failed.", filePath.c_str());
        return ErrorCode::READ_FAILED;
    }

    auto vectorBatchPtr = vectorBatch.get();
    if (vectorBatchPtr == nullptr || rowCount > maxRowCount) {
        vectorBatch = std::make_unique<VectorBatch>(rowCount);
        VectorHelper::AppendVectors(vectorBatch.get(), dataTypes, rowCount);
        vectorBatchPtr = vectorBatch.get();
        maxRowCount = rowCount;
    } else {
        vectorBatchPtr->Resize(rowCount);
    }

    int32_t vecCount = vectorBatchPtr->GetVectorCount();
    auto vecTypeIds = dataTypes.GetIds();
    for (int32_t vecIndex = 0; vecIndex < vecCount; vecIndex++) {
        ErrorCode result = ErrorCode::SUCCESS;
        BaseVector *vector = vectorBatchPtr->Get(vecIndex);
        int32_t typeId = vecTypeIds[vecIndex];
        switch (typeId) {
            case OMNI_BOOLEAN: result = ReadVector<bool>(vector, rowCount); break;
            case OMNI_INT:
            case OMNI_DATE32: result = ReadVector<int32_t>(vector, rowCount); break;
            case OMNI_SHORT: result = ReadVector<int16_t>(vector, rowCount); break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
            case OMNI_TIMESTAMP: result = ReadVector<int64_t>(vector, rowCount); break;
            case OMNI_DOUBLE: result = ReadVector<double>(vector, rowCount); break;
            case OMNI_VARCHAR:
            case OMNI_CHAR: result = ReadVector<std::string_view>(vector, rowCount); break;
            case OMNI_DECIMAL128: result = ReadVector<Decimal128>(vector, rowCount); break;
            case OMNI_BYTE: result = ReadVector<int8_t>(vector, rowCount); break;
            default: result = ErrorCode::READ_FAILED; break;
        }
        if (result != ErrorCode::SUCCESS) {
            isEnd = true;
            fclose(file);
            return result;
        }
    }
    rowOffset += rowCount;
    isEnd = false;
    return ErrorCode::SUCCESS;
}

template <typename T> ErrorCode SpillReader::ReadVector(BaseVector *vector, int32_t rowCount)
{
    if constexpr (std::is_same_v<T, std::string_view>) {
        /* *
         * vector format stored in file column by column, {nulls meta column, offsets meta column, values}
         * -nulls--offsets--values
         * 0        0      "aab"
         * 1        3        -
         * 0        3      "bbcd"
         * 7
         */
        auto resultVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);

        // read nulls
        uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(resultVector);
        int32_t nullsSize = BitUtil::Nbytes(rowCount);
        if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
            LogError("Read value nulls from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        // read offsets
        auto offsetSize = (static_cast<ssize_t>(rowCount) + 1) * sizeof(int32_t);
        auto offsets = reinterpret_cast<int32_t *>(VectorHelper::UnsafeGetOffsetsAddr(resultVector));
        if (Read(offsets, offsetSize) != ErrorCode::SUCCESS) {
            LogError("Read value offsets from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        auto length = offsets[rowCount] - offsets[0];
        if (length > 0) {
            // read values
            char *valuesBuffer;
            if (length <= INITIAL_STRING_SIZE) {
                valuesBuffer = unsafe::UnsafeStringVector::GetValues(resultVector);
            } else {
                valuesBuffer = unsafe::UnsafeStringVector::ExpandStringBuffer(resultVector, length);
            }
            if (Read(valuesBuffer, length) != ErrorCode::SUCCESS) {
                LogError("Read values from %s failed.", filePath.c_str());
                return ErrorCode::READ_FAILED;
            }
        }
    } else {
        auto resultVector = static_cast<Vector<T> *>(vector);

        // read valueNulls
        uint8_t *nulls = unsafe::UnsafeBaseVector::GetNulls(resultVector);
        int32_t nullsSize = BitUtil::Nbytes(rowCount);
        if (Read(nulls, nullsSize) != ErrorCode::SUCCESS) {
            LogError("Read value nulls from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }

        // read values
        T *values = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto length = static_cast<ssize_t>(rowCount * sizeof(T));
        if (Read(values, length) != ErrorCode::SUCCESS) {
            LogError("Read values from %s failed.", filePath.c_str());
            return ErrorCode::READ_FAILED;
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode SpillReader::Read(void *buf, size_t bufSize)
{
    if (fread(buf, bufSize, NUMBER_OF_ELEMENTS, file) < NUMBER_OF_ELEMENTS) {
        auto errorNum = errno;
        char errorBuf[ERROR_BUFFER_SIZE];
        GetErrorMsg(errorNum, errorBuf, ERROR_BUFFER_SIZE);
        LogError("Read from %s failed since %s.", filePath.c_str(), errorBuf);
        return ErrorCode::READ_FAILED;
    }
    return ErrorCode::SUCCESS;
}

int32_t SpillMergeStream::CompareTo(const SpillMergeStream &other)
{
    auto leftVectorBatch = currentBatchPtr;
    auto rightVectorBatch = other.currentBatchPtr;
    auto leftPosition = currentRowIdx;
    auto rightPosition = other.currentRowIdx;
    auto sortColsCount = sortCols.size();
    for (size_t i = 0; i < sortColsCount; i++) {
        int32_t sortCol = sortCols[i];
        BaseVector *leftVector = leftVectorBatch->Get(sortCol);
        BaseVector *rightVector = rightVectorBatch->Get(sortCol);
        int32_t compare = sortCompareFuncs[i](leftVector, leftPosition, rightVector, rightPosition);
        if (compare != 0) {
            return compare;
        }
    }
    return 0;
}

void SpillMerger::SetCompareFunctions(const type::DataTypes &dataTypes, const std::vector<int32_t> &sortCols,
                                      const std::vector<SortOrder> &sortOrders,
                                      std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs)
{
    auto dataTypeIds = dataTypes.GetIds();
    auto sortColSize = sortCols.size();
    for (size_t i = 0; i < sortColSize; i++) {
        auto typeId = dataTypeIds[sortCols[i]];
        bool isAscending = sortOrders[i].IsAscending();
        bool isNullsFirst = sortOrders[i].IsNullsFirst();
        switch (typeId) {
            case OMNI_INT:
            case OMNI_DATE32: SetCompareFunction<int32_t>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_SHORT: SetCompareFunction<int16_t>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_DECIMAL64:
            case OMNI_LONG:
            case OMNI_TIMESTAMP: SetCompareFunction<int64_t>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_BOOLEAN: SetCompareFunction<bool>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_DOUBLE: SetCompareFunction<double>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_CHAR:
            case OMNI_VARCHAR: SetCompareFunction<std::string_view>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_DECIMAL128: SetCompareFunction<Decimal128>(isAscending, isNullsFirst, sortCompareFuncs); break;
            case OMNI_BYTE: SetCompareFunction<int8_t>(isAscending, isNullsFirst, sortCompareFuncs); break;
            default: break;
        }
    }
}

template <typename T>
void SpillMerger::SetCompareFunction(bool isAscending, bool isNullsFirst,
                                     std::vector<OperatorUtil::CompareFunc> &sortCompareFuncs)
{
    if (isAscending && isNullsFirst) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, true, true, true>);
    } else if (isAscending && !isNullsFirst) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, true, true, false>);
    } else if (!isAscending && isNullsFirst) {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, false, true, true>);
    } else {
        sortCompareFuncs.emplace_back(OperatorUtil::CompareFlatTemplate<T, false, true, false>);
    }
}
} // namespace op
} // namespace omniruntime
