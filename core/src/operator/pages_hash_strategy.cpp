/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: hash strategy implementations
 */
#include "pages_hash_strategy.h"
#include <cmath>
#include <memory>
#include "vector/vector.h"
#include "util/operator_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace omniruntime {
namespace op {
PagesHashStrategy::PagesHashStrategy(BaseVector ***columns, const DataTypes &columnTypes, int32_t *hashCols,
    int32_t hashColsCount)
    : buildColumns(columns), buildColumnCount(columnTypes.GetSize()), buildHashColsCount(hashColsCount)
{
    if (hashColsCount > 0) {
        this->buildHashColTypes = new int32_t[hashColsCount];
        this->buildHashColumns = new BaseVector **[hashColsCount];
        int32_t hashColumn = 0;
        for (int32_t i = 0; i < hashColsCount; i++) {
            hashColumn = hashCols[i];
            buildHashColumns[i] = columns[hashColumn];
            buildHashColTypes[i] = (columnTypes.GetType(hashColumn))->GetId();
        }
    } else {
        this->buildHashColTypes = nullptr;
        this->buildHashColumns = nullptr;
    }
}

PagesHashStrategy::~PagesHashStrategy()
{
    if (buildHashColTypes != nullptr) {
        delete[] buildHashColTypes;
    }
    if (buildHashColumns != nullptr) {
        delete[] buildHashColumns;
    }
}

template <typename T>
static ALWAYS_INLINE bool ValueEqualsValueIgnoreNulls(omniruntime::vec::BaseVector *leftVector, uint32_t leftIndex,
    omniruntime::vec::BaseVector *rightVector, uint32_t rightIndex)
{
    using DictionaryVector = Vector<DictionaryContainer<T>>;
    using FlatVector = Vector<T>;
    T leftValue;
    T rightValue;
    if (leftVector->GetEncoding() == OMNI_DICTIONARY) {
        leftValue = static_cast<DictionaryVector *>(leftVector)->GetValue(leftIndex);
    } else {
        leftValue = static_cast<FlatVector *>(leftVector)->GetValue(leftIndex);
    }
    if (rightVector->GetEncoding() == OMNI_DICTIONARY) {
        rightValue = static_cast<DictionaryVector *>(rightVector)->GetValue(rightIndex);
    } else {
        rightValue = static_cast<FlatVector *>(rightVector)->GetValue(rightIndex);
    }
    return leftValue == rightValue;
}

static ALWAYS_INLINE bool FloatValueEqualsValueIgnoreNulls(omniruntime::vec::BaseVector *leftVector,
    uint32_t leftIndex, omniruntime::vec::BaseVector *rightVector, uint32_t rightIndex)
{
    using DictionaryVector = Vector<DictionaryContainer<float>>;
    using FlatVector = Vector<float>;
    float leftValue;
    float rightValue;
    if (leftVector->GetEncoding() == OMNI_DICTIONARY) {
        leftValue = static_cast<DictionaryVector *>(leftVector)->GetValue(leftIndex);
    } else {
        leftValue = static_cast<FlatVector *>(leftVector)->GetValue(leftIndex);
    }
    if (rightVector->GetEncoding() == OMNI_DICTIONARY) {
        rightValue = static_cast<DictionaryVector *>(rightVector)->GetValue(rightIndex);
    } else {
        rightValue = static_cast<FlatVector *>(rightVector)->GetValue(rightIndex);
    }
    if (leftValue == rightValue) {
        return true;
    }
    return std::isnan(leftValue) && std::isnan(rightValue);
}

static ALWAYS_INLINE bool DoubleValueEqualsValueIgnoreNulls(omniruntime::vec::BaseVector *leftVector,
    uint32_t leftIndex, omniruntime::vec::BaseVector *rightVector, uint32_t rightIndex)
{
    using DictionaryVector = Vector<DictionaryContainer<double>>;
    using FlatVector = Vector<double>;
    double leftValue;
    double rightValue;
    if (leftVector->GetEncoding() == OMNI_DICTIONARY) {
        leftValue = static_cast<DictionaryVector *>(leftVector)->GetValue(leftIndex);
    } else {
        leftValue = static_cast<FlatVector *>(leftVector)->GetValue(leftIndex);
    }
    if (rightVector->GetEncoding() == OMNI_DICTIONARY) {
        rightValue = static_cast<DictionaryVector *>(rightVector)->GetValue(rightIndex);
    } else {
        rightValue = static_cast<FlatVector *>(rightVector)->GetValue(rightIndex);
    }

    if (std::abs(leftValue - rightValue) < __DBL_EPSILON__) {
        return true;
    } else {
        return false;
    }
}

static ALWAYS_INLINE bool VarcharValueEqualsValueIgnoreNulls(omniruntime::vec::BaseVector *leftVector,
    uint32_t leftIndex, omniruntime::vec::BaseVector *rightVector, uint32_t rightIndex)
{
    using DictionaryVector = Vector<DictionaryContainer<std::string_view>>;
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    std::string_view leftValue;
    std::string_view rightValue;
    if (leftVector->GetEncoding() == OMNI_DICTIONARY) {
        leftValue = static_cast<DictionaryVector *>(leftVector)->GetValue(leftIndex);
    } else {
        leftValue = static_cast<VarcharVector *>(leftVector)->GetValue(leftIndex);
    }
    if (rightVector->GetEncoding() == OMNI_DICTIONARY) {
        rightValue = static_cast<DictionaryVector *>(rightVector)->GetValue(rightIndex);
    } else {
        rightValue = static_cast<VarcharVector *>(rightVector)->GetValue(rightIndex);
    }

    auto leftLength = leftValue.length();
    if (leftLength != rightValue.length()) {
        return false;
    }
    if (memcmp(leftValue.data(), rightValue.data(), leftLength) == 0) {
        return true;
    } else {
        return false;
    }
}

bool ValueEqualsValueIgnoreNulls(int32_t dataType, BaseVector *leftVector, uint32_t leftRowIndex,
    BaseVector *rightVector, uint32_t rightRowIndex)
{
    switch (dataType) {
        case OMNI_BYTE:
            return ValueEqualsValueIgnoreNulls<int8_t>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_INT:
        case OMNI_DATE32:
            return ValueEqualsValueIgnoreNulls<int32_t>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_SHORT:
            return ValueEqualsValueIgnoreNulls<int16_t>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return ValueEqualsValueIgnoreNulls<int64_t>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_DOUBLE:
            return DoubleValueEqualsValueIgnoreNulls(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_FLOAT:
            return FloatValueEqualsValueIgnoreNulls(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_BOOLEAN:
            return ValueEqualsValueIgnoreNulls<bool>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            return VarcharValueEqualsValueIgnoreNulls(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_DECIMAL128:
            return ValueEqualsValueIgnoreNulls<Decimal128>(leftVector, leftRowIndex, rightVector, rightRowIndex);
        case OMNI_ARRAY:
            return OperatorUtil::CompareArrayValue(leftVector, static_cast<int32_t>(leftRowIndex),
                rightVector, static_cast<int32_t>(rightRowIndex)) == OperatorUtil::COMPARE_STATUS_EQUAL;
        case OMNI_ROW:
            return OperatorUtil::CompareStructValue(leftVector, static_cast<int32_t>(leftRowIndex),
                rightVector, static_cast<int32_t>(rightRowIndex)) == OperatorUtil::COMPARE_STATUS_EQUAL;
        default:
            return false;
    }
}

bool PagesHashStrategy::PositionEqualsPosition(int32_t leftTableIndex, int32_t leftRowIndex, int32_t rightTableIndex,
    int32_t rightRowIndex) const
{
    for (uint32_t columnIdx = 0; columnIdx < buildHashColsCount; columnIdx++) {
        auto leftColumn = buildHashColumns[columnIdx][leftTableIndex];
        auto rightColumn = buildHashColumns[columnIdx][rightTableIndex];
        auto leftIsNull = leftColumn->IsNull(leftRowIndex);
        auto rightIsNull = rightColumn->IsNull(rightRowIndex);
        if (leftIsNull && rightIsNull) {
            continue;
        }
        if (leftIsNull || rightIsNull) {
            return false;
        }

        auto result = ValueEqualsValueIgnoreNulls(buildHashColTypes[columnIdx], leftColumn, leftRowIndex,
            rightColumn, rightRowIndex);
        if (!result) {
            return false;
        }
    }
    return true;
}
}
}