/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include "topn_sort.h"
#include "operator/util/operator_util.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

template <type::DataTypeId typeId> void *GetValueFromFlat(BaseVector *inputVec, int32_t inputPos, int32_t &length)
{
    if (inputVec->IsNull(inputPos)) {
        length = 0;
        return nullptr;
    }
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
        auto value = static_cast<Vector<LargeStringContainer<std::string_view>> *>(inputVec)->GetValue(inputPos);
        length = value.length();
        return const_cast<char *>(value.data());
    } else {
        length = 0;
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto values = unsafe::UnsafeVector::GetRawValues<RawDataType>(static_cast<Vector<RawDataType> *>(inputVec));
        return values + inputPos;
    }
}

template <type::DataTypeId typeId>
bool EqualValueTemplate(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    auto leftNull = leftVec->IsNull(leftPos);
    auto rightNull = rightVec->IsNull(rightPos);
    if (leftNull && rightNull) {
        return true;
    }
    if (leftNull || rightNull) {
        return false;
    }

    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        std::string_view leftValue;
        std::string_view rightValue;
        leftValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(leftVec)->GetValue(leftPos);
        rightValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec)->GetValue(rightPos);
        auto leftLength = leftValue.length();
        auto rightLength = rightValue.length();
        if (leftLength != rightLength) {
            return false;
        } else {
            return memcmp(leftValue.data(), rightValue.data(), leftLength) == 0;
        }
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        RawDataType leftValue;
        RawDataType rightValue;
        leftValue = static_cast<Vector<RawDataType> *>(leftVec)->GetValue(leftPos);
        rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
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
void SetValueFromFlat(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec, int32_t outputPos)
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    if (inputVec->IsNull(inputPos)) {
        if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
            static_cast<VarcharVector *>(outputVec)->SetNull(outputPos);
        } else {
            outputVec->SetNull(outputPos);
        }
        return;
    }

    using RawDataType = typename NativeAndVectorType<typeId>::type;
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
        auto outputVarcharVec = static_cast<VarcharVector *>(outputVec);
        auto inputVarcharVec = static_cast<VarcharVector *>(inputVec);
        auto value = inputVarcharVec->GetValue(inputPos);
        outputVarcharVec->SetValue(outputPos, value);
    } else {
        auto value = static_cast<Vector<RawDataType> *>(inputVec)->GetValue(inputPos);
        static_cast<Vector<RawDataType> *>(outputVec)->SetValue(outputPos, value);
    }
}

template <type::DataTypeId typeId>
void SetValueFromDictionary(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec, int32_t outputPos)
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    if (inputVec->IsNull(inputPos)) {
        if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
            static_cast<VarcharVector *>(outputVec)->SetNull(outputPos);
        } else {
            outputVec->SetNull(outputPos);
        }
        return;
    }

    using RawDataType = typename NativeAndVectorType<typeId>::type;
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR) {
        auto outputVarcharVec = static_cast<VarcharVector *>(outputVec);
        auto inputVarcharVec = static_cast<Vector<DictionaryContainer<std::string_view>> *>(inputVec);
        auto value = inputVarcharVec->GetValue(inputPos);
        outputVarcharVec->SetValue(outputPos, value);
    } else {
        auto value = static_cast<Vector<DictionaryContainer<RawDataType>> *>(inputVec)->GetValue(inputPos);
        static_cast<Vector<RawDataType> *>(outputVec)->SetValue(outputPos, value);
    }
}

template <type::DataTypeId typeId> static BaseVector *CreateVectorFromFlat(BaseVector *inputVec, int32_t inputPos)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    BaseVector *outputVec;
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        outputVec = new Vector<LargeStringContainer<std::string_view>>(1);
    } else {
        outputVec = new Vector<RawDataType>(1);
    }
    SetValueFromFlat<typeId>(inputVec, inputPos, outputVec, 0);
    return outputVec;
}

template <type::DataTypeId typeId> static BaseVector *CreateVectorFromDictionary(BaseVector *inputVec, int32_t inputPos)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    BaseVector *outputVec;
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        outputVec = new Vector<LargeStringContainer<std::string_view>>(1);
    } else {
        outputVec = new Vector<RawDataType>(1);
    }
    SetValueFromDictionary<typeId>(inputVec, inputPos, outputVec, 0);
    return outputVec;
}

template <type::DataTypeId typeId>
static int32_t CompareValueOptimizeFromFlat(void *valuePtr, int32_t length, BaseVector *rightVec, int32_t rightPos)
{
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        auto leftValue = (char *)valuePtr;
        auto rightValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec)->GetValue(rightPos);
        auto leftLength = static_cast<uint64_t>(length);
        auto rightLength = rightValue.length();
        int32_t result = memcmp(leftValue, rightValue.data(), std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }

        return leftLength - rightLength;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto leftValue = *((RawDataType *)valuePtr);
        auto rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        return leftValue > rightValue ? 1 : leftValue < rightValue ? -1 : 0;
    }
}

template <type::DataTypeId typeId>
static int32_t CompareValueFromFlat(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        auto leftValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(leftVec)->GetValue(leftPos);
        auto rightValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec)->GetValue(rightPos);
        auto leftLength = leftValue.length();
        auto rightLength = rightValue.length();
        int32_t result = memcmp(leftValue.data(), rightValue.data(), std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }

        return leftLength - rightLength;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto leftValue = static_cast<Vector<RawDataType> *>(leftVec)->GetValue(leftPos);
        auto rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        return leftValue > rightValue ? 1 : leftValue < rightValue ? -1 : 0;
    }
}

template <type::DataTypeId typeId>
static int32_t CompareValueFromDictionary(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        auto leftValue = static_cast<Vector<DictionaryContainer<std::string_view>> *>(leftVec)->GetValue(leftPos);
        auto rightValue = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightVec)->GetValue(rightPos);
        auto leftLength = leftValue.length();
        auto rightLength = rightValue.length();
        int32_t result = memcmp(leftValue.data(), rightValue.data(), std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }

        return leftLength - rightLength;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto leftValue = static_cast<Vector<DictionaryContainer<RawDataType>> *>(leftVec)->GetValue(leftPos);
        auto rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        return leftValue > rightValue ? 1 : leftValue < rightValue ? -1 : 0;
    }
}

static std::vector<GetValueFunc> getValueFromFlatFuncs = {
    nullptr,                           // OMNI_NONE,
    GetValueFromFlat<OMNI_INT>,        // OMNI_INT
    GetValueFromFlat<OMNI_LONG>,       // OMNI_LONG
    GetValueFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    GetValueFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    GetValueFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    GetValueFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    GetValueFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    GetValueFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    GetValueFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    GetValueFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    GetValueFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    GetValueFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                           // OMNI_INTERVAL_MONTHS
    nullptr,                           // OMNI_INTERVAL_DAY_TIME
    GetValueFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    GetValueFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                           // OMNI_CONTAINER,
    GetValueFromFlat<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<CompareOptimizeFunc> compareOptimizeFromFlatFuncs = {
    nullptr,                                       // OMNI_NONE,
    CompareValueOptimizeFromFlat<OMNI_INT>,        // OMNI_INT
    CompareValueOptimizeFromFlat<OMNI_LONG>,       // OMNI_LONG
    CompareValueOptimizeFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    CompareValueOptimizeFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    CompareValueOptimizeFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    CompareValueOptimizeFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    CompareValueOptimizeFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    CompareValueOptimizeFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    CompareValueOptimizeFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    CompareValueOptimizeFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    CompareValueOptimizeFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    CompareValueOptimizeFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                       // OMNI_INTERVAL_MONTHS
    nullptr,                                       // OMNI_INTERVAL_DAY_TIME
    CompareValueOptimizeFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    CompareValueOptimizeFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                       // OMNI_CONTAINER,
    CompareValueOptimizeFromFlat<OMNI_BYTE>        // OMNI_BYTE
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
    nullptr,                               // OMNI_CONTAINER,
    CompareValueFromFlat<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<CompareFunc> compareFromDictionaryFuncs = {
    nullptr,                                     // OMNI_NONE,
    CompareValueFromDictionary<OMNI_INT>,        // OMNI_INT
    CompareValueFromDictionary<OMNI_LONG>,       // OMNI_LONG
    CompareValueFromDictionary<OMNI_DOUBLE>,     // OMNI_DOUBLE
    CompareValueFromDictionary<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    CompareValueFromDictionary<OMNI_SHORT>,      // OMNI_SHORT
    CompareValueFromDictionary<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    CompareValueFromDictionary<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    CompareValueFromDictionary<OMNI_DATE32>,     // OMNI_DATE32
    CompareValueFromDictionary<OMNI_DATE64>,     // OMNI_DATE64
    CompareValueFromDictionary<OMNI_TIME32>,     // OMNI_TIME32
    CompareValueFromDictionary<OMNI_TIME64>,     // OMNI_TIME64
    CompareValueFromDictionary<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                     // OMNI_INTERVAL_MONTHS
    nullptr,                                     // OMNI_INTERVAL_DAY_TIME
    CompareValueFromDictionary<OMNI_VARCHAR>,    // OMNI_VARCHAR
    CompareValueFromDictionary<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                     // OMNI_CONTAINER,
    CompareValueFromDictionary<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<EqualFunc> equalFromFlatFuncs = {
    nullptr,                             // OMNI_NONE,
    EqualValueTemplate<OMNI_INT>,        // OMNI_INT
    EqualValueTemplate<OMNI_LONG>,       // OMNI_LONG
    EqualValueTemplate<OMNI_DOUBLE>,     // OMNI_DOUBLE
    EqualValueTemplate<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    EqualValueTemplate<OMNI_SHORT>,      // OMNI_SHORT
    EqualValueTemplate<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    EqualValueTemplate<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    EqualValueTemplate<OMNI_DATE32>,     // OMNI_DATE32
    EqualValueTemplate<OMNI_DATE64>,     // OMNI_DATE64
    EqualValueTemplate<OMNI_TIME32>,     // OMNI_TIME32
    EqualValueTemplate<OMNI_TIME64>,     // OMNI_TIME64
    EqualValueTemplate<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                             // OMNI_INTERVAL_MONTHS
    nullptr,                             // OMNI_INTERVAL_DAY_TIME
    EqualValueTemplate<OMNI_VARCHAR>,    // OMNI_VARCHAR
    EqualValueTemplate<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                             // OMNI_CONTAINER,
    EqualValueTemplate<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<CreateVectorFunc> createVectorFromFlatFuncs = {
    nullptr,                               // OMNI_NONE,
    CreateVectorFromFlat<OMNI_INT>,        // OMNI_INT
    CreateVectorFromFlat<OMNI_LONG>,       // OMNI_LONG
    CreateVectorFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    CreateVectorFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    CreateVectorFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    CreateVectorFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    CreateVectorFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    CreateVectorFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    CreateVectorFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    CreateVectorFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    CreateVectorFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    CreateVectorFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                               // OMNI_INTERVAL_MONTHS
    nullptr,                               // OMNI_INTERVAL_DAY_TIME
    CreateVectorFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    CreateVectorFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                               // OMNI_CONTAINER,
    CreateVectorFromFlat<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<CreateVectorFunc> createVectorFromDictionaryFuncs = {
    nullptr,                                     // OMNI_NONE,
    CreateVectorFromDictionary<OMNI_INT>,        // OMNI_INT
    CreateVectorFromDictionary<OMNI_LONG>,       // OMNI_LONG
    CreateVectorFromDictionary<OMNI_DOUBLE>,     // OMNI_DOUBLE
    CreateVectorFromDictionary<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    CreateVectorFromDictionary<OMNI_SHORT>,      // OMNI_SHORT
    CreateVectorFromDictionary<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    CreateVectorFromDictionary<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    CreateVectorFromDictionary<OMNI_DATE32>,     // OMNI_DATE32
    CreateVectorFromDictionary<OMNI_DATE64>,     // OMNI_DATE64
    CreateVectorFromDictionary<OMNI_TIME32>,     // OMNI_TIME32
    CreateVectorFromDictionary<OMNI_TIME64>,     // OMNI_TIME64
    CreateVectorFromDictionary<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                     // OMNI_INTERVAL_MONTHS
    nullptr,                                     // OMNI_INTERVAL_DAY_TIME
    CreateVectorFromDictionary<OMNI_VARCHAR>,    // OMNI_VARCHAR
    CreateVectorFromDictionary<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                     // OMNI_CONTAINER,
    CreateVectorFromDictionary<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<SetValueFunc> setValueFromFlatFuncs = {
    nullptr,                           // OMNI_NONE,
    SetValueFromFlat<OMNI_INT>,        // OMNI_INT
    SetValueFromFlat<OMNI_LONG>,       // OMNI_LONG
    SetValueFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    SetValueFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SetValueFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    SetValueFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    SetValueFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SetValueFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    SetValueFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    SetValueFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    SetValueFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    SetValueFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                           // OMNI_INTERVAL_MONTHS
    nullptr,                           // OMNI_INTERVAL_DAY_TIME
    SetValueFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    SetValueFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                           // OMNI_CONTAINER,
    SetValueFromFlat<OMNI_BYTE>        // OMNI_BYTE
};

static std::vector<SetValueFunc> setValueFromDictionaryFuncs = {
    nullptr,                                 // OMNI_NONE,
    SetValueFromDictionary<OMNI_INT>,        // OMNI_INT
    SetValueFromDictionary<OMNI_LONG>,       // OMNI_LONG
    SetValueFromDictionary<OMNI_DOUBLE>,     // OMNI_DOUBLE
    SetValueFromDictionary<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SetValueFromDictionary<OMNI_SHORT>,      // OMNI_SHORT
    SetValueFromDictionary<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    SetValueFromDictionary<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SetValueFromDictionary<OMNI_DATE32>,     // OMNI_DATE32
    SetValueFromDictionary<OMNI_DATE64>,     // OMNI_DATE64
    SetValueFromDictionary<OMNI_TIME32>,     // OMNI_TIME32
    SetValueFromDictionary<OMNI_TIME64>,     // OMNI_TIME64
    SetValueFromDictionary<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                 // OMNI_INTERVAL_MONTHS
    nullptr,                                 // OMNI_INTERVAL_DAY_TIME
    SetValueFromDictionary<OMNI_VARCHAR>,    // OMNI_VARCHAR
    SetValueFromDictionary<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                 // OMNI_CONTAINER,
    SetValueFromDictionary<OMNI_BYTE>        // OMNI_BYTE
};

TopNSortOperator::TopNSortOperator(const type::DataTypes &sourceTypes, int32_t n, bool isStrictTopN,
    const std::vector<int32_t> &partitionCols, const std::vector<int32_t> &sortCols,
    const std::vector<int32_t> &sortAscendings, const std::vector<int32_t> &sortNullFirsts,
    const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes),
      n(n),
      isStrictTopN(isStrictTopN),
      partitionCols(partitionCols),
      partitionColNum(static_cast<int32_t>(partitionCols.size())),
      sortCols(sortCols),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts),
      sortColNum(static_cast<int32_t>(sortCols.size()))
{
    executionContext->SetConfig(queryConfig);
    auto sourceTypeIds = sourceTypes.GetIds();
    for (int32_t i = 0; i < sortColNum; i++) {
        auto type = sourceTypeIds[sortCols[i]];
        sortColTypes.emplace_back(type);
    }
    if (sortColNum > 1) {
        sortCompareFuncs.resize(sortColNum);
    } else {
        sortGetValueFuncs.resize(sortColNum);
        sortCompareOptimizeFuncs.resize(sortColNum);
    }
    serializers.resize(partitionColNum);

    auto inputColNum = sourceTypes.GetSize();
    equalFuncs.resize(inputColNum);
    createVectorFuncs.resize(inputColNum);
    setOutputValueFuncs.resize(inputColNum);
    for (int32_t i = 0; i < inputColNum; i++) {
        setOutputValueFuncs[i] = setValueFromFlatFuncs[sourceTypeIds[i]];
    }

    int32_t eachRowSize = OperatorUtil::GetRowSize(sourceTypes.Get());
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(eachRowSize, &executionContext->queryConfigRef());
}

type::StringRef TopNSortOperator::GeneratePartitionKey(BaseVector **partitionVectors, int32_t partitionColNum,
    int32_t rowIdx, mem::SimpleArenaAllocator &arenaAllocator)
{
    type::StringRef key;
    for (int32_t colIdx = 0; colIdx < partitionColNum; colIdx++) {
        auto curVector = partitionVectors[colIdx];
        auto &curFunc = serializers[colIdx];
        curFunc(curVector, rowIdx, arenaAllocator, key);
    }
    return key;
}

int32_t TopNSortOperator::FindInsertPositionOptimize(void *ptr, int32_t length, std::vector<VectorBatch *> &vecBatches,
    std::vector<int32_t> &rowIndexes, int32_t position)
{
    while (position >= 0) {
        auto left = vecBatches[position];
        auto leftRowIdx = rowIndexes[position];

        // the value to bo inserted is less than the current position value, then go on
        int32_t result = CompareForSortColsOptimize(ptr, length, left, leftRowIdx);
        if (result >= 0) {
            break;
        }
        position--;
    }
    return position + 1;
}

int32_t TopNSortOperator::FindInsertPosition(BaseVector **insertSortVectors, int32_t insertRowIdx,
    std::vector<VectorBatch *> &vecBatches, std::vector<int32_t> &rowIndexes, int32_t position)
{
    while (position >= 0) {
        auto left = vecBatches[position];
        auto leftRowIdx = rowIndexes[position];

        // the value to bo inserted is less than the current position value, then go on
        int32_t result = CompareForSortCols(insertSortVectors, insertRowIdx, left, leftRowIdx);
        if (result >= 0) {
            break;
        }
        position--;
    }
    return position + 1;
}

void TopNSortOperator::Prepare(BaseVector **inputVectors)
{
    auto sourceTypeIds = sourceTypes.GetIds();
    for (int32_t i = 0; i < partitionColNum; ++i) {
        auto partitionCol = partitionCols[i];
        auto curTypeId = sourceTypeIds[partitionCol];
        serializers[i] = vectorSerializerCenter[curTypeId];
    }

    if (sortColNum == 1) {
        auto sortCol = sortCols[0];
        auto curTypeId = sourceTypeIds[sortCol];
        sortGetValueFuncs[0] = getValueFromFlatFuncs[curTypeId];
        sortCompareOptimizeFuncs[0] = compareOptimizeFromFlatFuncs[curTypeId];
        equalFuncs[0] = equalFromFlatFuncs[curTypeId];
        return;
    }

    for (int32_t i = 0; i < sortColNum; i++) {
        auto sortCol = sortCols[i];
        auto curTypeId = sourceTypeIds[sortCol];
        sortCompareFuncs[i] = compareFromFlatFuncs[curTypeId];
        equalFuncs[i] = equalFromFlatFuncs[curTypeId];
    }
}

void TopNSortOperator::InsertNewPartition(StringRef &key, VectorBatch *inputVecBatch, int32_t inputRowIdx)
{
    auto value = new PartitionValue(n);
    value->vecBatches[0] = inputVecBatch;
    value->rowIndexes[0] = inputRowIdx;
    value->nextIndex = 1;
    partitionedMap[key] = value;
}

void TopNSortOperator::InsertNewValueOptimize(PartitionValue &value, vec::VectorBatch *inputVecBatch,
    vec::BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto sortVector = sortVectors[0];
    int32_t length;
    auto valuePtr = sortGetValueFuncs[0](sortVector, inputRowIdx, length);

    auto& vecBatches = value.vecBatches;
    auto& rowIndexes = value.rowIndexes;
    auto vecBatchSize = value.nextIndex;
    auto lastPosition = vecBatchSize - 1;

    auto insertPos = FindInsertPositionOptimize(valuePtr, length, vecBatches, rowIndexes, lastPosition);
    for (int32_t pos = vecBatchSize; pos > insertPos; pos--) {
        vecBatches[pos] = vecBatches[pos - 1];
        rowIndexes[pos] = rowIndexes[pos - 1];
    }

    vecBatches[insertPos] = inputVecBatch;
    rowIndexes[insertPos] = inputRowIdx;
    value.nextIndex++;
}

void TopNSortOperator::InsertNewValue(PartitionValue &value, VectorBatch *inputVecBatch, BaseVector **sortVectors,
    int32_t inputRowIdx)
{
    auto& vecBatches = value.vecBatches;
    auto& rowIndexes = value.rowIndexes;
    auto vecBatchSize = value.nextIndex;
    auto lastPosition = vecBatchSize - 1;

    // find insert position
    auto insertPos = FindInsertPosition(sortVectors, inputRowIdx, vecBatches, rowIndexes, lastPosition);
    for (int32_t pos = vecBatchSize; pos > insertPos; pos--) {
        vecBatches[pos] = vecBatches[pos - 1];
        rowIndexes[pos] = rowIndexes[pos - 1];
    }

    vecBatches[insertPos] = inputVecBatch;
    rowIndexes[insertPos] = inputRowIdx;
    value.nextIndex++;
}

void TopNSortOperator::UpdatePartitionValueOptimize(PartitionValue &value, VectorBatch *inputVecBatch,
    BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto sortVector = sortVectors[0];
    int32_t length;
    auto valuePtr = sortGetValueFuncs[0](sortVector, inputRowIdx, length);

    auto& vecBatches = value.vecBatches;
    auto& rowIndexes = value.rowIndexes;
    auto lastPosition = n - 1;

    // compare the last value
    int32_t result = CompareForSortColsOptimize(valuePtr, length, vecBatches[lastPosition], rowIndexes[lastPosition]);
    if (result > 0) {
        // if the value to be inserted is greater or equals to the last, then skip
        return;
    }
    if (result == 0) {
        value.Enlarge();
        vecBatches[value.nextIndex] = inputVecBatch;
        rowIndexes[value.nextIndex] = inputRowIdx;
        value.nextIndex++;
    } else {
        auto insertPos = FindInsertPositionOptimize(valuePtr, length, vecBatches, rowIndexes, lastPosition - 1);
        value.Enlarge();
        for (int32_t pos = value.nextIndex; pos > insertPos; pos--) {
            vecBatches[pos] = vecBatches[pos - 1];
            rowIndexes[pos] = rowIndexes[pos - 1];
        }
        vecBatches[insertPos] = inputVecBatch;
        rowIndexes[insertPos] = inputRowIdx;

        if (insertPos == lastPosition) {
            value.nextIndex = n;
        } else {
            // check the nth and (n-1)th values are equal
            auto isDistinct =
                CheckDistinctForLast(vecBatches[lastPosition], rowIndexes[lastPosition], vecBatches[n], rowIndexes[n]);
            if (isDistinct) {
                value.nextIndex = n;
            } else {
                value.nextIndex++;
            }
        }
    }
}

void TopNSortOperator::UpdatePartitionValue(PartitionValue &value, VectorBatch *inputVecBatch, BaseVector **sortVectors,
    int32_t inputRowIdx)
{
    auto& vecBatches = value.vecBatches;
    auto& rowIndexes = value.rowIndexes;
    auto lastPosition = n - 1;

    // compare the last value
    int32_t result = CompareForSortCols(sortVectors, inputRowIdx, vecBatches[lastPosition], rowIndexes[lastPosition]);
    if (result > 0) {
        // if the value to be inserted is greater or equals to the last, then skip
        return;
    }
    if (result == 0) {
        value.Enlarge();
        vecBatches[value.nextIndex] = inputVecBatch;
        rowIndexes[value.nextIndex] = inputRowIdx;
        value.nextIndex++;
    } else {
        auto insertPos = FindInsertPosition(sortVectors, inputRowIdx, vecBatches, rowIndexes, lastPosition - 1);
        value.Enlarge();
        for (int32_t pos = value.nextIndex; pos > insertPos; pos--) {
            vecBatches[pos] = vecBatches[pos - 1];
            rowIndexes[pos] = rowIndexes[pos - 1];
        }
        vecBatches[insertPos] = inputVecBatch;
        rowIndexes[insertPos] = inputRowIdx;

        if (insertPos == lastPosition) {
            value.nextIndex = n;
        } else {
            // check the nth and (n-1)th values are equal
            auto isDistinct =
                CheckDistinctForLast(vecBatches[lastPosition], rowIndexes[lastPosition], vecBatches[n], rowIndexes[n]);
            if (isDistinct) {
                value.nextIndex = n;
            } else {
                value.nextIndex++;
            }
        }
    }
}

int32_t TopNSortOperator::AddInput(omniruntime::vec::VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        this->ResetInputVecBatch();
        return 0;
    }

    inputs.emplace_back(inputVecBatch);
    ResetInputVecBatch();
    auto inputVectors = inputVecBatch->GetVectors();
    auto inputColNum = sourceTypes.GetSize();
    for (int32_t i = 0; i < inputColNum; i++) {
        auto inputVector = inputVectors[i];
        if (inputVector->GetEncoding() == OMNI_DICTIONARY) {
            inputVectors[i] = VectorHelper::DecodeDictionaryVector(inputVector);
            delete inputVector;
            inputVector = nullptr;
        }
    }
    Prepare(inputVectors);

    BaseVector *partitionVectors[partitionColNum];
    for (int32_t i = 0; i < partitionColNum; ++i) {
        partitionVectors[i] = inputVectors[partitionCols[i]];
    }
    BaseVector *sortVectors[sortColNum];
    for (int32_t i = 0; i < sortColNum; i++) {
        sortVectors[i] = inputVectors[sortCols[i]];
    }

    auto &arenaAllocator = *(executionContext->GetArena());
    auto inputRowCount = inputVecBatch->GetRowCount();

    if (sortColNum == 1) {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            // first, serialize partition row
            type::StringRef key = GeneratePartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);

            // second, check whether the key is inserted
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                // this is a new partition
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();

                // this is an existing partition
                auto value = keyPos->second;
                if (value->nextIndex < n) {
                    // case 1, the vecBatches is not full
                    InsertNewValueOptimize(*value, inputVecBatch, sortVectors, rowIdx);
                } else {
                    // case 2, the vecBatches is full
                    UpdatePartitionValueOptimize(*value, inputVecBatch, sortVectors, rowIdx);
                }
            }
        }
    } else {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            // first, serialize partition row
            type::StringRef key = GeneratePartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);

            // second, check whether the key is inserted
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                // this is a new partition
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();

                // this is an existing partition
                auto value = keyPos->second;
                if (value->nextIndex < n) {
                    // case 1, the vecBatches is not full
                    InsertNewValue(*value, inputVecBatch, sortVectors, rowIdx);
                } else {
                    // case 2, the vecBatches is full
                    UpdatePartitionValue(*value, inputVecBatch, sortVectors, rowIdx);
                }
            }
        }
    }
    currentIter = partitionedMap.begin();

    return 0;
}

int32_t TopNSortOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    int32_t outputRowCount = 0;
    std::unordered_map<type::StringRef, PartitionValue *, PartitionHash>::iterator mapPos;
    auto mapEnd = partitionedMap.end();
    for (mapPos = currentIter; mapPos != mapEnd; ++mapPos) {
        auto value = mapPos->second;
        outputRowCount += value->nextIndex;
        if (outputRowCount >= maxRowCount) {
            break;
        }
    }
    if (outputRowCount == 0) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }

    std::unordered_map<type::StringRef, PartitionValue *, PartitionHash>::iterator end;
    if (mapPos == mapEnd) {
        end = mapEnd;
        SetStatus(OMNI_STATUS_FINISHED);
    } else {
        end = mapPos++;
    }

    auto outputColNum = sourceTypes.GetSize();
    auto result = std::make_unique<VectorBatch>(outputRowCount);

    int32_t resultRowIdx = 0;
    VectorHelper::AppendVectors(result.get(), sourceTypes, outputRowCount);
    auto resultVectors = result->GetVectors();
    for (auto iter = currentIter; iter != end; ++iter) {
        auto mapValue = iter->second;
        auto vecBatches = mapValue->vecBatches;
        auto rowIndexes = mapValue->rowIndexes;
        auto vecBatchSize = mapValue->nextIndex;
        for (int32_t i = 0; i < vecBatchSize; i++) {
            auto vecBatch = vecBatches[i];
            auto rowIndex = rowIndexes[i];
            for (int32_t j = 0; j < outputColNum; j++) {
                setOutputValueFuncs[j](vecBatch->Get(j), rowIndex, resultVectors[j], resultRowIdx);
            }
            resultRowIdx++;
        }
    }
    *outputVecBatch = result.release();
    currentIter = end;
    return 0;
}

OmniStatus TopNSortOperator::Close()
{
    auto end = partitionedMap.end();
    for (auto iter = partitionedMap.begin(); iter != end; iter++) {
        auto mapValue = iter->second;
        delete mapValue;
    }
    VectorHelper::FreeVecBatches(inputs);
    return OMNI_STATUS_NORMAL;
}
