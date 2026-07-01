/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * @Description: window group limit operator implementations
 */

#include "window_group_limit.h"
#include "operator/util/operator_util.h"
#include "type/data_type.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace omniruntime {
namespace op {
template <type::DataTypeId typeId>
void *GetPartitionValueFromFlat(BaseVector *inputVec, int32_t inputPos, int32_t &length)
{
    if (inputVec->IsNull(inputPos)) {
        length = 0;
        return nullptr;
    }
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
        auto value = static_cast<VarcharVector *>(inputVec)->GetValue(inputPos);
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
bool EqualPartitionValueTemplate(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    auto leftNull = leftVec->IsNull(leftPos);
    auto rightNull = rightVec->IsNull(rightPos);
    if (leftNull && rightNull) {
        return true;
    }
    if (leftNull || rightNull) {
        return false;
    }

    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR || typeId == OMNI_VARBINARY) {
        std::string_view leftValue;
        std::string_view rightValue;
        leftValue = static_cast<VarcharVector *>(leftVec)->GetValue(leftPos);
        rightValue = static_cast<VarcharVector *>(rightVec)->GetValue(rightPos);
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
            return std::abs(leftValue - rightValue) < DBL_EPSILON;
        } else if constexpr (typeId == OMNI_FLOAT) {
            return std::abs(leftValue - rightValue) < FLT_EPSILON;
        } else {
            return leftValue == rightValue;
        }
    }
}

template <type::DataTypeId typeId>
void SetPartitionValueFromFlat(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos)
{
    if (inputVec->IsNull(inputPos)) {
        if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
            static_cast<VarcharVector *>(outputVec)->SetNull(outputPos);
        } else {
            outputVec->SetNull(outputPos);
        }
        return;
    }

    using RawDataType = typename NativeAndVectorType<typeId>::type;
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
        auto outputVarcharVec = static_cast<VarcharVector *>(outputVec);
        auto inputVarcharVec = static_cast<VarcharVector *>(inputVec);
        auto value = inputVarcharVec->GetValue(inputPos);
        outputVarcharVec->SetValue(outputPos, value);
    } else {
        auto value = static_cast<Vector<RawDataType> *>(inputVec)->GetValue(inputPos);
        static_cast<Vector<RawDataType> *>(outputVec)->SetValue(outputPos, value);
    }
}

static void SetPartitionArrayValueFromFlat(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos)
{
    auto *inputArrayVec = static_cast<ArrayVector *>(inputVec);
    auto *outputArrayVec = static_cast<ArrayVector *>(outputVec);

    if (inputArrayVec->IsNull(inputPos)) {
        outputArrayVec->SetNull(outputPos);
        return;
    }

    BaseVector *arrayValue = inputArrayVec->GetValue(inputPos);
    outputArrayVec->SetValue(outputPos, arrayValue);
    delete arrayValue;
}

// Forward declaration for SetPartitionStructValueFromFlat
static void SetPartitionStructValueFromFlat(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos);

static int32_t CompareArrayFromFlat(vec::BaseVector *leftVec, int32_t leftPos, vec::BaseVector *rightVec,
    int32_t rightPos)
{
    // Top-level null handling for sort columns is done in CompareForSortCols / CompareForSortColsOptimize.
    // Here we only compare non-null array values using the generic array comparison utility.
    return OperatorUtil::CompareArrayValue(leftVec, leftPos, rightVec, rightPos, true);
}

static bool EqualArrayFromFlat(vec::BaseVector *leftVec, int32_t leftPos, vec::BaseVector *rightVec, int32_t rightPos)
{
    return OperatorUtil::CompareArrayValue(leftVec, leftPos, rightVec, rightPos, true)
        == OperatorUtil::COMPARE_STATUS_EQUAL;
}

static int32_t CompareStructFromFlat(vec::BaseVector *leftVec, int32_t leftPos, vec::BaseVector *rightVec,
    int32_t rightPos)
{
    // Top-level null handling for sort columns is done in CompareForSortCols / CompareForSortColsOptimize.
    // Here we only compare non-null struct values using the generic struct comparison utility.
    return OperatorUtil::CompareStructValue(leftVec, leftPos, rightVec, rightPos, true);
}

static bool EqualStructFromFlat(vec::BaseVector *leftVec, int32_t leftPos, vec::BaseVector *rightVec, int32_t rightPos)
{
    return OperatorUtil::CompareStructValue(leftVec, leftPos, rightVec, rightPos, true)
        == OperatorUtil::COMPARE_STATUS_EQUAL;
}

template <type::DataTypeId typeId>
void SetPartitionValueFromDictionary(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos)
{
    if (inputVec->IsNull(inputPos)) {
        if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
            static_cast<VarcharVector *>(outputVec)->SetNull(outputPos);
        } else {
            outputVec->SetNull(outputPos);
        }
        return;
    }

    using RawDataType = typename NativeAndVectorType<typeId>::type;
    if constexpr (typeId == OMNI_VARCHAR || typeId == OMNI_CHAR || typeId == OMNI_VARBINARY) {
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
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR || typeId == OMNI_VARBINARY) {
        outputVec = new VarcharVector(1);
    } else {
        outputVec = new Vector<RawDataType>(1);
    }
    SetPartitionValueFromFlat<typeId>(inputVec, inputPos, outputVec, 0);
    return outputVec;
}

template <type::DataTypeId typeId> static BaseVector *CreateVectorFromDictionary(BaseVector *inputVec, int32_t inputPos)
{
    using RawDataType = typename NativeAndVectorType<typeId>::type;
    BaseVector *outputVec;
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR) {
        outputVec = new VarcharVector(1);
    } else {
        outputVec = new Vector<RawDataType>(1);
    }
    SetPartitionValueFromDictionary<typeId>(inputVec, inputPos, outputVec, 0);
    return outputVec;
}

template <type::DataTypeId typeId>
static int32_t CompareValueOptimizeFromFlat(void *valuePtr, int32_t length, BaseVector *rightVec, int32_t rightPos)
{
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR || typeId == OMNI_VARBINARY) {
        auto leftValue = (char *)valuePtr;
        auto rightValue = static_cast<VarcharVector *>(rightVec)->GetValue(rightPos);
        auto leftLength = static_cast<uint64_t>(length);
        auto rightLength = rightValue.length();
        int32_t result = memcmp(leftValue, rightValue.data(), std::min(leftLength, rightLength));
        if (result != 0) {
            return result;
        }
        return leftLength - rightLength;
    } else {
        using RawDataType = typename NativeAndVectorType<typeId>::type;
        auto leftValue = *static_cast<RawDataType *>(valuePtr);
        auto rightValue = static_cast<Vector<RawDataType> *>(rightVec)->GetValue(rightPos);
        return leftValue > rightValue ? 1 : leftValue < rightValue ? -1 : 0;
    }
}

template <type::DataTypeId typeId>
static int32_t CompareValueFromFlat(BaseVector *leftVec, int32_t leftPos, BaseVector *rightVec, int32_t rightPos)
{
    if constexpr (typeId == OMNI_CHAR || typeId == OMNI_VARCHAR || typeId == OMNI_VARBINARY) {
        auto leftValue = static_cast<VarcharVector *>(leftVec)->GetValue(leftPos);
        auto rightValue = static_cast<VarcharVector *>(rightVec)->GetValue(rightPos);
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
        auto rightValue = static_cast<VarcharVector *>(rightVec)->GetValue(rightPos);
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
    nullptr,                                    // OMNI_NONE,
    GetPartitionValueFromFlat<OMNI_INT>,        // OMNI_INT
    GetPartitionValueFromFlat<OMNI_LONG>,       // OMNI_LONG
    GetPartitionValueFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE
    GetPartitionValueFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    GetPartitionValueFromFlat<OMNI_SHORT>,      // OMNI_SHORT
    GetPartitionValueFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    GetPartitionValueFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    GetPartitionValueFromFlat<OMNI_DATE32>,     // OMNI_DATE32
    GetPartitionValueFromFlat<OMNI_DATE64>,     // OMNI_DATE64
    GetPartitionValueFromFlat<OMNI_TIME32>,     // OMNI_TIME32
    GetPartitionValueFromFlat<OMNI_TIME64>,     // OMNI_TIME64
    GetPartitionValueFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                    // OMNI_INTERVAL_MONTHS
    nullptr,                                    // OMNI_INTERVAL_DAY_TIME
    GetPartitionValueFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR
    GetPartitionValueFromFlat<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                    // OMNI_CONTAINER,
    GetPartitionValueFromFlat<OMNI_BYTE>,       // OMNI_BYTE
    GetPartitionValueFromFlat<OMNI_FLOAT>,      // OMNI_FLOAT
    GetPartitionValueFromFlat<OMNI_VARBINARY>,  // OMNI_VARBINARY = 20
    nullptr,                                    // OMNI_TIME_WITHOUT_TIME_ZONE = 21
    nullptr,                                    // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE = 22
    nullptr,                                    // OMNI_TIMESTAMP_WITH_TIME_ZONE = 23
    nullptr,                                    // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE = 24
    nullptr,                                    // OMNI_MULTISET = 25
    nullptr,                                    // reserved
    nullptr,                                    // reserved
    nullptr,                                    // reserved
    nullptr,                                    // reserved
    nullptr,                                    // OMNI_ARRAY = 30
    nullptr,                                    // OMNI_MAP = 31
    nullptr,                                    // OMNI_ROW = 32
    nullptr, nullptr, nullptr, nullptr          // OMNI_UNKNOWN..OMNI_INVALID = 36
};

static std::vector<CompareOptimizeFunc> compareOptimizeFromFlatFuncs = {
    nullptr,                                       // OMNI_NONE = 0
    CompareValueOptimizeFromFlat<OMNI_INT>,        // OMNI_INT = 1
    CompareValueOptimizeFromFlat<OMNI_LONG>,       // OMNI_LONG = 2
    CompareValueOptimizeFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE = 3
    CompareValueOptimizeFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN = 4
    CompareValueOptimizeFromFlat<OMNI_SHORT>,      // OMNI_SHORT = 5
    CompareValueOptimizeFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64 = 6
    CompareValueOptimizeFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128 = 7
    CompareValueOptimizeFromFlat<OMNI_DATE32>,     // OMNI_DATE32 = 8
    CompareValueOptimizeFromFlat<OMNI_DATE64>,     // OMNI_DATE64 = 9
    CompareValueOptimizeFromFlat<OMNI_TIME32>,     // OMNI_TIME32 = 10
    CompareValueOptimizeFromFlat<OMNI_TIME64>,     // OMNI_TIME64 = 11
    CompareValueOptimizeFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP = 12
    nullptr,                                       // OMNI_INTERVAL_MONTHS = 13
    nullptr,                                       // OMNI_INTERVAL_DAY_TIME = 14
    CompareValueOptimizeFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR = 15
    CompareValueOptimizeFromFlat<OMNI_CHAR>,       // OMNI_CHAR = 16
    nullptr,                                       // OMNI_CONTAINER = 17
    CompareValueOptimizeFromFlat<OMNI_BYTE>,       // OMNI_BYTE = 18
    CompareValueOptimizeFromFlat<OMNI_FLOAT>,     // OMNI_FLOAT = 19
    CompareValueOptimizeFromFlat<OMNI_VARBINARY>, // OMNI_VARBINARY = 20
    nullptr,                                       // OMNI_TIME_WITHOUT_TIME_ZONE = 21
    nullptr,                                       // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE = 22
    nullptr,                                       // OMNI_TIMESTAMP_WITH_TIME_ZONE = 23
    nullptr,                                       // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE = 24
    nullptr,                                       // OMNI_MULTISET = 25
    nullptr, nullptr, nullptr, nullptr,           // reserved (26-29)
    nullptr,                                      // OMNI_ARRAY = 30
    nullptr,                                      // OMNI_MAP = 31
    nullptr,                                      // OMNI_ROW = 32
    nullptr, nullptr, nullptr, nullptr            // OMNI_UNKNOWN..OMNI_INVALID = 36
};

static std::vector<CompareFunc> compareFromFlatFuncs = {
    nullptr,                                 // OMNI_NONE = 0
    CompareValueFromFlat<OMNI_INT>,        // OMNI_INT = 1
    CompareValueFromFlat<OMNI_LONG>,       // OMNI_LONG = 2
    CompareValueFromFlat<OMNI_DOUBLE>,     // OMNI_DOUBLE = 3
    CompareValueFromFlat<OMNI_BOOLEAN>,    // OMNI_BOOLEAN = 4
    CompareValueFromFlat<OMNI_SHORT>,      // OMNI_SHORT = 5
    CompareValueFromFlat<OMNI_DECIMAL64>,  // OMNI_DECIMAL64 = 6
    CompareValueFromFlat<OMNI_DECIMAL128>, // OMNI_DECIMAL128 = 7
    CompareValueFromFlat<OMNI_DATE32>,     // OMNI_DATE32 = 8
    CompareValueFromFlat<OMNI_DATE64>,     // OMNI_DATE64 = 9
    CompareValueFromFlat<OMNI_TIME32>,     // OMNI_TIME32 = 10
    CompareValueFromFlat<OMNI_TIME64>,     // OMNI_TIME64 = 11
    CompareValueFromFlat<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP = 12
    nullptr,                               // OMNI_INTERVAL_MONTHS = 13
    nullptr,                               // OMNI_INTERVAL_DAY_TIME = 14
    CompareValueFromFlat<OMNI_VARCHAR>,    // OMNI_VARCHAR = 15
    CompareValueFromFlat<OMNI_CHAR>,       // OMNI_CHAR = 16
    nullptr,                               // OMNI_CONTAINER = 17
    CompareValueFromFlat<OMNI_BYTE>,       // OMNI_BYTE = 18
    CompareValueFromFlat<OMNI_FLOAT>,      // OMNI_FLOAT = 19
    CompareValueFromFlat<OMNI_VARBINARY>,  // OMNI_VARBINARY = 20
    nullptr,                               // OMNI_TIME_WITHOUT_TIME_ZONE = 21
    nullptr,                               // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE = 22
    nullptr,                               // OMNI_TIMESTAMP_WITH_TIME_ZONE = 23
    nullptr,                               // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE = 24
    nullptr,                               // OMNI_MULTISET = 25
    nullptr, nullptr, nullptr, nullptr,   // reserved (26-29)
    CompareArrayFromFlat,                  // OMNI_ARRAY = 30
    nullptr,                              // OMNI_MAP = 31
    CompareStructFromFlat,                  // OMNI_ROW = 32
    nullptr, nullptr, nullptr, nullptr      // OMNI_UNKNOWN..OMNI_INVALID = 36
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
    CompareValueFromDictionary<OMNI_BYTE>     // OMNI_VARCHAR
};

static std::vector<EqualFunc> equalFromFlatFuncs = {
    nullptr,                                      // OMNI_NONE = 0
    EqualPartitionValueTemplate<OMNI_INT>,        // OMNI_INT = 1
    EqualPartitionValueTemplate<OMNI_LONG>,       // OMNI_LONG = 2
    EqualPartitionValueTemplate<OMNI_DOUBLE>,     // OMNI_DOUBLE = 3
    EqualPartitionValueTemplate<OMNI_BOOLEAN>,    // OMNI_BOOLEAN = 4
    EqualPartitionValueTemplate<OMNI_SHORT>,     // OMNI_SHORT = 5
    EqualPartitionValueTemplate<OMNI_DECIMAL64>, // OMNI_DECIMAL64 = 6
    EqualPartitionValueTemplate<OMNI_DECIMAL128>, // OMNI_DECIMAL128 = 7
    EqualPartitionValueTemplate<OMNI_DATE32>,    // OMNI_DATE32 = 8
    EqualPartitionValueTemplate<OMNI_DATE64>,    // OMNI_DATE64 = 9
    EqualPartitionValueTemplate<OMNI_TIME32>,    // OMNI_TIME32 = 10
    EqualPartitionValueTemplate<OMNI_TIME64>,    // OMNI_TIME64 = 11
    EqualPartitionValueTemplate<OMNI_TIMESTAMP>, // OMNI_TIMESTAMP = 12
    nullptr,                                      // OMNI_INTERVAL_MONTHS = 13
    nullptr,                                      // OMNI_INTERVAL_DAY_TIME = 14
    EqualPartitionValueTemplate<OMNI_VARCHAR>,   // OMNI_VARCHAR = 15
    EqualPartitionValueTemplate<OMNI_CHAR>,      // OMNI_CHAR = 16
    nullptr,                                      // OMNI_CONTAINER = 17
    EqualPartitionValueTemplate<OMNI_BYTE>,      // OMNI_BYTE = 18
    EqualPartitionValueTemplate<OMNI_FLOAT>,     // OMNI_FLOAT = 19
    EqualPartitionValueTemplate<OMNI_VARBINARY>, // OMNI_VARBINARY = 20
    nullptr,                                      // OMNI_TIME_WITHOUT_TIME_ZONE = 21
    nullptr,                                      // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE = 22
    nullptr,                                      // OMNI_TIMESTAMP_WITH_TIME_ZONE = 23
    nullptr,                                      // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE = 24
    nullptr,                                      // OMNI_MULTISET = 25
    nullptr, nullptr, nullptr, nullptr,          // reserved (26-29)
    EqualArrayFromFlat,                           // OMNI_ARRAY = 30
    nullptr,                                      // OMNI_MAP = 31
    EqualStructFromFlat,                          // OMNI_ROW = 32
    nullptr, nullptr, nullptr, nullptr            // OMNI_UNKNOWN..OMNI_INVALID = 36
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
    CreateVectorFromFlat<OMNI_BYTE>,       // OMNI_BYTE
    nullptr,                               // OMNI_VARBINARY
    nullptr,                               // OMNI_TIME_WITHOUT_TIME_ZONE
    nullptr,                               // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE
    nullptr,                               // OMNI_TIMESTAMP_WITH_TIME_ZONE
    nullptr,                               // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE
    nullptr,                               // OMNI_MULTISET
    nullptr, nullptr, nullptr, nullptr,      // reserved (26-29)
    nullptr,                               // OMNI_ARRAY = 30
    nullptr,                               // OMNI_MAP = 31
    nullptr                                // OMNI_ROW = 32
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
    CreateVectorFromDictionary<OMNI_BYTE>,       // OMNI_BYTE
    nullptr,                                     // OMNI_VARBINARY
    nullptr,                                     // OMNI_TIME_WITHOUT_TIME_ZONE
    nullptr,                                     // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE
    nullptr,                                     // OMNI_TIMESTAMP_WITH_TIME_ZONE
    nullptr,                                     // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE
    nullptr,                                     // OMNI_MULTISET
    nullptr, nullptr, nullptr, nullptr,          // reserved (26-29)
    nullptr,                                     // OMNI_ARRAY = 30
    nullptr,                                     // OMNI_MAP = 31
    nullptr                                      // OMNI_ROW = 32
};

static std::vector<SetValueFunc> InitSetValueFromFlatFuncs()
{
    // Support scalar types plus array and struct types; other complex types remain unsupported (nullptr).
    std::vector<SetValueFunc> funcs(OMNI_ROW + 1, nullptr);

    funcs[OMNI_INT] = SetPartitionValueFromFlat<OMNI_INT>;
    funcs[OMNI_LONG] = SetPartitionValueFromFlat<OMNI_LONG>;
    funcs[OMNI_DOUBLE] = SetPartitionValueFromFlat<OMNI_DOUBLE>;
    funcs[OMNI_BOOLEAN] = SetPartitionValueFromFlat<OMNI_BOOLEAN>;
    funcs[OMNI_SHORT] = SetPartitionValueFromFlat<OMNI_SHORT>;
    funcs[OMNI_DECIMAL64] = SetPartitionValueFromFlat<OMNI_DECIMAL64>;
    funcs[OMNI_DECIMAL128] = SetPartitionValueFromFlat<OMNI_DECIMAL128>;
    funcs[OMNI_DATE32] = SetPartitionValueFromFlat<OMNI_DATE32>;
    funcs[OMNI_DATE64] = SetPartitionValueFromFlat<OMNI_DATE64>;
    funcs[OMNI_TIME32] = SetPartitionValueFromFlat<OMNI_TIME32>;
    funcs[OMNI_TIME64] = SetPartitionValueFromFlat<OMNI_TIME64>;
    funcs[OMNI_TIMESTAMP] = SetPartitionValueFromFlat<OMNI_TIMESTAMP>;
    funcs[OMNI_VARCHAR] = SetPartitionValueFromFlat<OMNI_VARCHAR>;
    funcs[OMNI_CHAR] = SetPartitionValueFromFlat<OMNI_CHAR>;
    funcs[OMNI_BYTE] = SetPartitionValueFromFlat<OMNI_BYTE>;
    funcs[OMNI_FLOAT] = SetPartitionValueFromFlat<OMNI_FLOAT>;
    funcs[OMNI_VARBINARY] = SetPartitionValueFromFlat<OMNI_VARBINARY>;
    funcs[OMNI_ARRAY] = SetPartitionArrayValueFromFlat;
    funcs[OMNI_ROW] = SetPartitionStructValueFromFlat;

    return funcs;
}

static std::vector<SetValueFunc> setValueFromFlatFuncs = InitSetValueFromFlatFuncs();

static std::vector<SetValueFunc> setValueFromDictionaryFuncs = {
    nullptr,                                          // OMNI_NONE,
    SetPartitionValueFromDictionary<OMNI_INT>,        // OMNI_INT
    SetPartitionValueFromDictionary<OMNI_LONG>,       // OMNI_LONG
    SetPartitionValueFromDictionary<OMNI_DOUBLE>,     // OMNI_DOUBLE
    SetPartitionValueFromDictionary<OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SetPartitionValueFromDictionary<OMNI_SHORT>,      // OMNI_SHORT
    SetPartitionValueFromDictionary<OMNI_DECIMAL64>,  // OMNI_DECIMAL64,
    SetPartitionValueFromDictionary<OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SetPartitionValueFromDictionary<OMNI_DATE32>,     // OMNI_DATE32
    SetPartitionValueFromDictionary<OMNI_DATE64>,     // OMNI_DATE64
    SetPartitionValueFromDictionary<OMNI_TIME32>,     // OMNI_TIME32
    SetPartitionValueFromDictionary<OMNI_TIME64>,     // OMNI_TIME64
    SetPartitionValueFromDictionary<OMNI_TIMESTAMP>,  // OMNI_TIMESTAMP
    nullptr,                                          // OMNI_INTERVAL_MONTHS
    nullptr,                                          // OMNI_INTERVAL_DAY_TIME
    SetPartitionValueFromDictionary<OMNI_VARCHAR>,    // OMNI_VARCHAR
    SetPartitionValueFromDictionary<OMNI_CHAR>,       // OMNI_CHAR,
    nullptr,                                          // OMNI_CONTAINER,
    SetPartitionValueFromDictionary<OMNI_BYTE>,       // OMNI_BYTE
    nullptr,                                          // OMNI_VARBINARY
    nullptr,                                          // OMNI_TIME_WITHOUT_TIME_ZONE
    nullptr,                                          // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE
    nullptr,                                          // OMNI_TIMESTAMP_WITH_TIME_ZONE
    nullptr,                                          // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE
    nullptr,                                          // OMNI_MULTISET
    nullptr, nullptr, nullptr, nullptr,               // reserved (26-29)
    nullptr,                                          // OMNI_ARRAY = 30
    nullptr,                                          // OMNI_MAP = 31
    nullptr                                           // OMNI_ROW = 32
};

// Actual implementation of SetPartitionStructValueFromFlat (after setValueFromFlatFuncs is initialized)
static void SetPartitionStructValueFromFlat(vec::BaseVector *inputVec, int32_t inputPos, vec::BaseVector *outputVec,
    int32_t outputPos)
{
    auto *inputRowVec = static_cast<RowVector *>(inputVec);
    auto *outputRowVec = static_cast<RowVector *>(outputVec);

    if (inputRowVec->IsNull(inputPos)) {
        outputRowVec->SetNull(outputPos);
        return;
    }

    int32_t childCount = inputRowVec->ChildSize();
    for (int32_t i = 0; i < childCount; ++i) {
        auto &inputChild = inputRowVec->ChildAt(i);
        auto &outputChild = outputRowVec->ChildAt(i);
        auto childTypeId = static_cast<type::DataTypeId>(inputChild->GetTypeId());

        if (inputChild->IsNull(inputPos)) {
            outputChild->SetNull(outputPos);
        } else if (childTypeId == type::OMNI_VARCHAR || childTypeId == type::OMNI_CHAR ||
                   childTypeId == type::OMNI_VARBINARY) {
            auto value = static_cast<VarcharVector *>(inputChild.get())->GetValue(inputPos);
            static_cast<VarcharVector *>(outputChild.get())->SetValue(outputPos, value);
        } else {
            setValueFromFlatFuncs[childTypeId](inputChild.get(), inputPos, outputChild.get(), outputPos);
        }
    }
}

WindowGroupLimitOperator::WindowGroupLimitOperator(const type::DataTypes &sourceTypes, int32_t n,
    const std::string funcName, const std::vector<int32_t> &partitionCols, const std::vector<int32_t> &sortCols,
    const std::vector<int32_t> &sortAscendings, const std::vector<int32_t> &sortNullFirsts,
    const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes),
      n(n),
      funcName(funcName),
      partitionCols(partitionCols),
      partitionColNum(static_cast<int32_t>(partitionCols.size())),
      sortCols(sortCols),
      sortColNum(static_cast<int32_t>(sortCols.size())),
      sortAscendings(sortAscendings),
      sortNullFirsts(sortNullFirsts)
{
    executionContext->SetConfig(queryConfig);
    auto sourceTypeIds = sourceTypes.GetIds();
    for (int32_t i = 0; i < sortColNum; i++) {
        auto type = sourceTypeIds[sortCols[i]];
        sortColTypes.emplace_back(type);
    }
    // Always allocate compare function slots for sort columns.
    if (sortColNum >= 1) {
        sortCompareFuncs.resize(sortColNum);
    }
    // For single-column sort we may use fast-path optimization.
    if (sortColNum == 1) {
        sortGetValueFuncs.resize(sortColNum);
        sortCompareOptimizeFuncs.resize(sortColNum);
    }
    serializers.resize(partitionColNum);

    auto inputColNum = sourceTypes.GetSize();
    equalFuncs.resize(inputColNum);
    createVectorFuncs.resize(inputColNum);
    setOutputValueFuncs.resize(inputColNum);
    for (int32_t i = 0; i < inputColNum; i++) {
        auto typeId = sourceTypeIds[i];
        if (typeId < 0 || typeId >= static_cast<int32_t>(setValueFromFlatFuncs.size())
            || setValueFromFlatFuncs[typeId] == nullptr) {
            OMNI_THROW("WindowGroupLimit Error", "Unsupported data type for windowGroupLimit output column.");
        }
        setOutputValueFuncs[i] = setValueFromFlatFuncs[typeId];
    }

    int32_t eachRowSize = OperatorUtil::GetRowSize(sourceTypes.Get());
    maxRowCount = OperatorUtil::GetConfiguredMaxRowCount(eachRowSize, &executionContext->queryConfigRef());
}

type::StringRef WindowGroupLimitOperator::GenerateWindowPartitionKey(BaseVector **partitionVectors,
    int32_t partitionColNum, int32_t rowIdx, mem::SimpleArenaAllocator &arenaAllocator)
{
    type::StringRef key;
    for (int32_t colIdx = 0; colIdx < partitionColNum; colIdx++) {
        auto curVector = partitionVectors[colIdx];
        auto &curFunc = serializers[colIdx];
        curFunc(curVector, rowIdx, arenaAllocator, key);
    }
    return key;
}

int32_t WindowGroupLimitOperator::FindInsertPositionOptimize(void *ptr, int32_t length,
    std::vector<VectorBatch *> &vecBatches, std::vector<int32_t> &rowIndexes, int32_t position)
{
    while (position >= 0) {
        auto left = vecBatches[position];
        auto leftRowIdx = rowIndexes[position];

        int32_t result = CompareForSortColsOptimize(ptr, length, left, leftRowIdx);
        if (result >= 0) {
            break;
        }
        position--;
    }
    return position + 1;
}

int32_t WindowGroupLimitOperator::FindInsertPosition(BaseVector **insertSortVectors, int32_t insertRowIdx,
    std::vector<VectorBatch *> &vecBatches, std::vector<int32_t> &rowIndexes, int32_t position)
{
    while (position >= 0) {
        auto left = vecBatches[position];
        auto leftRowIdx = rowIndexes[position];

        int32_t result = CompareForSortCols(insertSortVectors, insertRowIdx, left, leftRowIdx);
        if (result >= 0) {
            break;
        }
        position--;
    }
    return position + 1;
}

void WindowGroupLimitOperator::Prepare(BaseVector **inputVectors)
{
    auto sourceTypeIds = sourceTypes.GetIds();
    for (int32_t i = 0; i < partitionColNum; ++i) {
        auto partitionCol = partitionCols[i];
        auto curTypeId = sourceTypeIds[partitionCol];
        serializers[i] = vectorSerializerCenter[curTypeId];
    }

    useOptimizedPath = (sortColNum == 1);
    if (useOptimizedPath) {
        auto sortCol = sortCols[0];
        auto curTypeId = sourceTypeIds[sortCol];
        if (curTypeId == type::OMNI_ARRAY || curTypeId == type::OMNI_ROW) {
            useOptimizedPath = false;
        }
    }

    if (useOptimizedPath) {
        auto sortCol = sortCols[0];
        auto curTypeId = sourceTypeIds[sortCol];
        if (curTypeId < 0 || curTypeId >= static_cast<int32_t>(getValueFromFlatFuncs.size())
            || getValueFromFlatFuncs[curTypeId] == nullptr
            || curTypeId >= static_cast<int32_t>(compareOptimizeFromFlatFuncs.size())
            || compareOptimizeFromFlatFuncs[curTypeId] == nullptr
            || curTypeId >= static_cast<int32_t>(equalFromFlatFuncs.size())
            || equalFromFlatFuncs[curTypeId] == nullptr) {
            OMNI_THROW("WindowGroupLimit Error", "Unsupported data type for windowGroupLimit sort column.");
        }
        sortGetValueFuncs[0] = getValueFromFlatFuncs[curTypeId];
        sortCompareOptimizeFuncs[0] = compareOptimizeFromFlatFuncs[curTypeId];
        equalFuncs[0] = equalFromFlatFuncs[curTypeId];
        return;
    }

    for (int32_t i = 0; i < sortColNum; i++) {
        auto sortCol = sortCols[i];
        auto curTypeId = sourceTypeIds[sortCol];
        if (curTypeId < 0 || curTypeId >= static_cast<int32_t>(compareFromFlatFuncs.size())
            || compareFromFlatFuncs[curTypeId] == nullptr
            || curTypeId >= static_cast<int32_t>(equalFromFlatFuncs.size())
            || equalFromFlatFuncs[curTypeId] == nullptr) {
            OMNI_THROW("WindowGroupLimit Error", "Unsupported data type for windowGroupLimit sort column.");
        }
        sortCompareFuncs[i] = compareFromFlatFuncs[curTypeId];
        equalFuncs[i] = equalFromFlatFuncs[curTypeId];
    }
}

void WindowGroupLimitOperator::InsertNewPartition(StringRef &key, VectorBatch *inputVecBatch, int32_t inputRowIdx)
{
    auto value = new LimitPartitionValue(n);
    value->vecBatches[0] = inputVecBatch;
    value->rowIndexes[0] = inputRowIdx;
    value->nextIndex = 1;
    partitionedMap[key] = value;
}

void WindowGroupLimitOperator::InsertNewValueOptimize(LimitPartitionValue &value, vec::VectorBatch *inputVecBatch,
    vec::BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto sortVector = sortVectors[0];
    int32_t length;
    auto valuePtr = sortGetValueFuncs[0](sortVector, inputRowIdx, length);

    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
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

void WindowGroupLimitOperator::InsertNewValue(LimitPartitionValue &value, VectorBatch *inputVecBatch,
    BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
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

void WindowGroupLimitOperator::UpdatePartitionValueOptimizeRank(LimitPartitionValue &value, VectorBatch *inputVecBatch,
    BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto sortVector = sortVectors[0];
    int32_t length;
    auto valuePtr = sortGetValueFuncs[0](sortVector, inputRowIdx, length);

    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
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

void WindowGroupLimitOperator::UpdatePartitionValueOptimizeRowNumber(LimitPartitionValue &value,
    VectorBatch *inputVecBatch, BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto sortVector = sortVectors[0];
    int32_t length;
    auto valuePtr = sortGetValueFuncs[0](sortVector, inputRowIdx, length);

    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
    auto lastPosition = n - 1;

    // compare the last value
    int32_t result = CompareForSortColsOptimize(valuePtr, length, vecBatches[lastPosition], rowIndexes[lastPosition]);
    if (result >= 0) {
        // if the value to be inserted is greater or equals to the last, then skip
        return;
    }
    auto insertPos = FindInsertPositionOptimize(valuePtr, length, vecBatches, rowIndexes, lastPosition - 1);

    for (int32_t pos = value.nextIndex - 1; pos > insertPos; pos--) {
        vecBatches[pos] = vecBatches[pos - 1];
        rowIndexes[pos] = rowIndexes[pos - 1];
    }
    vecBatches[insertPos] = inputVecBatch;
    rowIndexes[insertPos] = inputRowIdx;
}

void WindowGroupLimitOperator::UpdatePartitionValueRank(LimitPartitionValue &value, VectorBatch *inputVecBatch,
    BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
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

void WindowGroupLimitOperator::UpdatePartitionValueRowNumber(LimitPartitionValue &value, VectorBatch *inputVecBatch,
    BaseVector **sortVectors, int32_t inputRowIdx)
{
    auto &vecBatches = value.vecBatches;
    auto &rowIndexes = value.rowIndexes;
    auto lastPosition = n - 1;

    // compare the last value
    int32_t result = CompareForSortCols(sortVectors, inputRowIdx, vecBatches[lastPosition], rowIndexes[lastPosition]);
    if (result >= 0) {
        // if the value to be inserted is greater or equals to the last, then skip
        return;
    }
    auto insertPos = FindInsertPosition(sortVectors, inputRowIdx, vecBatches, rowIndexes, lastPosition - 1);

    for (int32_t pos = value.nextIndex - 1; pos > insertPos; pos--) {
        vecBatches[pos] = vecBatches[pos - 1];
        rowIndexes[pos] = rowIndexes[pos - 1];
    }
    vecBatches[insertPos] = inputVecBatch;
    rowIndexes[insertPos] = inputRowIdx;
}

int32_t WindowGroupLimitOperator::AddInput(omniruntime::vec::VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        this->ResetInputVecBatch();
        return 0;
    }

    UpdateAddInputInfo(inputVecBatch->GetRowCount());
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

    if (funcName == "rank" && useOptimizedPath) {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            type::StringRef key = GenerateWindowPartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else if (keyPos->second->nextIndex < n) {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                InsertNewValueOptimize(*value, inputVecBatch, sortVectors, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                UpdatePartitionValueOptimizeRank(*value, inputVecBatch, sortVectors, rowIdx);
            }
        }
    } else if (funcName == "rank" && !useOptimizedPath) {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            type::StringRef key = GenerateWindowPartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else if (keyPos->second->nextIndex < n) {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                InsertNewValue(*value, inputVecBatch, sortVectors, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                UpdatePartitionValueRank(*value, inputVecBatch, sortVectors, rowIdx);
            }
        }
    } else if (funcName == "row_number" && useOptimizedPath) {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            type::StringRef key = GenerateWindowPartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else if (keyPos->second->nextIndex < n) {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                InsertNewValueOptimize(*value, inputVecBatch, sortVectors, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                UpdatePartitionValueOptimizeRowNumber(*value, inputVecBatch, sortVectors, rowIdx);
            }
        }
    } else {
        for (int32_t rowIdx = 0; rowIdx < inputRowCount; rowIdx++) {
            type::StringRef key = GenerateWindowPartitionKey(partitionVectors, partitionColNum, rowIdx, arenaAllocator);
            auto keyPos = partitionedMap.find(key);
            if (keyPos == partitionedMap.end()) {
                InsertNewPartition(key, inputVecBatch, rowIdx);
            } else if (keyPos->second->nextIndex < n) {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                InsertNewValue(*value, inputVecBatch, sortVectors, rowIdx);
            } else {
                arenaAllocator.RollBackContinualMem();
                auto value = keyPos->second;
                UpdatePartitionValueRowNumber(*value, inputVecBatch, sortVectors, rowIdx);
            }
        }
    }

    currentIter = partitionedMap.begin();

    return 0;
}

int32_t WindowGroupLimitOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    int32_t outputRowCount = 0;
    std::unordered_map<type::StringRef, LimitPartitionValue *, WindowGroupLimitPartitionHash>::iterator mapPos;
    auto mapEnd = partitionedMap.end();
    for (mapPos = currentIter; mapPos != mapEnd; ++mapPos) {
        auto value = mapPos->second;
        auto rowsToAdd = value->nextIndex;
        if (outputRowCount + rowsToAdd > maxRowCount) {
            break;
        }
        outputRowCount += rowsToAdd;
    }
    if (outputRowCount == 0) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }

    std::unordered_map<type::StringRef, LimitPartitionValue *, WindowGroupLimitPartitionHash>::iterator end;
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
    bool stopOutput = false;
    for (auto iter = currentIter; iter != end && !stopOutput; ++iter) {
        auto mapValue = iter->second;
        auto vecBatches = mapValue->vecBatches;
        auto rowIndexes = mapValue->rowIndexes;
        auto vecBatchSize = mapValue->nextIndex;
        for (int32_t i = 0; i < vecBatchSize; i++) {
            if (resultRowIdx >= maxRowCount) {
                stopOutput = true;
                break;
            }
            auto vecBatch = vecBatches[i];
            auto rowIndex = rowIndexes[i];
            for (int32_t j = 0; j < outputColNum; j++) {
                setOutputValueFuncs[j](vecBatch->Get(j), rowIndex, resultVectors[j], resultRowIdx);
            }
            resultRowIdx++;
        }
    }
    UpdateGetOutputInfo(outputRowCount);
    *outputVecBatch = result.release();
    currentIter = end;
    return 0;
}

OmniStatus WindowGroupLimitOperator::Close()
{
    auto end = partitionedMap.end();
    for (auto iter = partitionedMap.begin(); iter != end; iter++) {
        auto mapValue = iter->second;
        delete mapValue;
    }
    VectorHelper::FreeVecBatches(inputs);
    return OMNI_STATUS_NORMAL;
}
} // namespace op
} // namespace omniruntime
