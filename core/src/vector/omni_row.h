/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Row buffer Header
 */

#ifndef OMNI_RUNTIME_OMNI_ROW_H
#define OMNI_RUNTIME_OMNI_ROW_H

/*
 * row format:
 * 
 * FixType: isNull(1bit) + neg(1bit) + sizeLength(4bit) | fixValue |
 * Char/Varchar: isNull(1bit) + sizeLength(4bit) | realLength(n bytes) | varcharValue |
 * Array: isNull(1bit) + real_num_ele_size(4bit) | real_num_ele(n bytes) | element1 | element2 | ... |
 * Map: isNull(1bit) | keyArrayEncoding | mapArrayEncoding |
 * Struct: isNull(1bit) + struct_child_size(4bit) | childArrayEncoding | ... |
 */
#include "omni_row_value.h"
#include "memory/simple_arena_allocator.h"
#include <algorithm>
#include <cstdint>

namespace omniruntime {
namespace vec {
// Last four bit is encoding for meta size.
inline constexpr uint8_t ENCODING_META_SIZE = 0b00001111;

// Little-endian compact int: copy at most sizeof(T) bytes so a corrupt/oversize
// length prefix cannot smash a stack slot (BYTE is 1 byte, prefix low-4-bits is 0..15).
// Always consume `nBytes` from the stream to stay aligned with the writer.
template <typename T>
ALWAYS_INLINE static void ReadCompactLeInt(const uint8_t *src, uint8_t nBytes, T *out)
{
    T value = 0;
    uint8_t n = nBytes;
    if (n > static_cast<uint8_t>(sizeof(T))) {
        n = static_cast<uint8_t>(sizeof(T));
    }
    if (n > 0) {
        std::copy(src, src + n, reinterpret_cast<uint8_t *>(&value));
    }
    *out = value;
}

template <type::DataTypeId id, Encoding encoding>
void TransToValue(BaseVector *vec, int32_t rowIndex, BaseSerialize *value)
{
    using T = typename NativeType<id>::type;
    reinterpret_cast<SerializedValue<T, encoding> *>(value)->TransValue(vec, rowIndex);
}

template <type::DataTypeId id, Encoding encoding>
std::unique_ptr<BaseSerialize> GenerateSerValue()
{
    using T = typename NativeType<id>::type;
    return std::make_unique<SerializedValue<T, encoding>>();
}

template <typename T>
struct VectorTypeTraits {
    using type = Vector<T>;
};

template<>
struct VectorTypeTraits<std::string_view> {
    using type = Vector<LargeStringContainer<std::string_view>>;
};

template<>
struct VectorTypeTraits<BaseVector*> {
    using type = ArrayVector;
};

template<>
struct VectorTypeTraits<std::pair<BaseVector*, BaseVector*>> {
    using type = MapVector;
};

template<>
struct VectorTypeTraits<std::vector<BaseVector*>> {
    using type = RowVector;
};


template <type::DataTypeId id> uint8_t *RowToVec(uint8_t *row, BaseVector *vec, int32_t rowIndex);

// Helper macro to wrap template function call for code style compliance
#define TMP_FUNC_CALL(FUNC, ...) (FUNC<__VA_ARGS__>)

#define FUNC_CENTER_DEF(CENTER_NAME, FUNC_NAME, TMP_FUNC_PTR) \
    static std::vector<FUNC_NAME> CENTER_NAME = { nullptr,    \
        TMP_FUNC_PTR<type::OMNI_INT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG>,                        \
        TMP_FUNC_PTR<type::OMNI_DOUBLE>,                      \
        TMP_FUNC_PTR<type::OMNI_BOOLEAN>,                     \
        TMP_FUNC_PTR<type::OMNI_SHORT>,                       \
        TMP_FUNC_PTR<type::OMNI_LONG>,                        \
        TMP_FUNC_PTR<type::OMNI_DECIMAL128>,                  \
        TMP_FUNC_PTR<type::OMNI_INT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG>,                        \
        TMP_FUNC_PTR<type::OMNI_INT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG>,                        \
        TMP_FUNC_PTR<type::OMNI_LONG>,                        \
        nullptr,                                              \
        nullptr,                                              \
        TMP_FUNC_PTR<type::OMNI_VARCHAR>,                     \
        TMP_FUNC_PTR<type::OMNI_VARCHAR>,                     \
        nullptr,                                              \
        TMP_FUNC_PTR<type::OMNI_BYTE>,                        \
        TMP_FUNC_PTR<type::OMNI_INT>,                         \
        TMP_FUNC_PTR<type::OMNI_VARBINARY>,                   \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        nullptr,                                              \
        TMP_FUNC_PTR<type::OMNI_ARRAY>,                       \
        TMP_FUNC_PTR<type::OMNI_MAP>,                         \
        TMP_FUNC_PTR<type::OMNI_ROW> }

#define FUNC_CENTER_FLAT_DEF(CENTER_NAME, FUNC_NAME, TMP_FUNC_PTR)                 \
    static std::vector<FUNC_NAME> CENTER_NAME = { nullptr,                         \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_FLAT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_FLAT>,                        \
        TMP_FUNC_PTR<type::OMNI_DOUBLE, Encoding::OMNI_FLAT>,                      \
        TMP_FUNC_PTR<type::OMNI_BOOLEAN, Encoding::OMNI_FLAT>,                     \
        TMP_FUNC_PTR<type::OMNI_SHORT, Encoding::OMNI_FLAT>,                       \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_FLAT>,                        \
        TMP_FUNC_PTR<type::OMNI_DECIMAL128, Encoding::OMNI_FLAT>,                  \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_FLAT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_FLAT>,                        \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_FLAT>,                         \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_FLAT>,                        \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_FLAT>,                        \
        nullptr,                                                                   \
        nullptr,                                                                   \
        TMP_FUNC_PTR<type::OMNI_VARCHAR, Encoding::OMNI_FLAT>,                     \
        TMP_FUNC_PTR<type::OMNI_VARCHAR, Encoding::OMNI_FLAT>,                     \
        nullptr,                                                                   \
        TMP_FUNC_PTR<type::OMNI_BYTE, Encoding::OMNI_FLAT>,                        \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_FLAT>,                         \
        TMP_FUNC_PTR<type::OMNI_VARBINARY, Encoding::OMNI_FLAT>,                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        TMP_FUNC_PTR<type::OMNI_ARRAY, Encoding::OMNI_ENCODING_ARRAY>,             \
        TMP_FUNC_PTR<type::OMNI_MAP, Encoding::OMNI_ENCODING_MAP>,                 \
        TMP_FUNC_PTR<type::OMNI_ROW, Encoding::OMNI_ENCODING_STRUCT> }

#define FUNC_CENTER_DICT_DEF(CENTER_NAME, FUNC_NAME, TMP_FUNC_PTR)                 \
    static std::vector<FUNC_NAME> CENTER_NAME = { nullptr,                         \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_DICTIONARY>,                   \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_DICTIONARY>,                  \
        TMP_FUNC_PTR<type::OMNI_DOUBLE, Encoding::OMNI_DICTIONARY>,                \
        TMP_FUNC_PTR<type::OMNI_BOOLEAN, Encoding::OMNI_DICTIONARY>,               \
        TMP_FUNC_PTR<type::OMNI_SHORT, Encoding::OMNI_DICTIONARY>,                 \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_DICTIONARY>,                  \
        TMP_FUNC_PTR<type::OMNI_DECIMAL128, Encoding::OMNI_DICTIONARY>,            \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_DICTIONARY>,                   \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_DICTIONARY>,                  \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_DICTIONARY>,                   \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_DICTIONARY>,                  \
        TMP_FUNC_PTR<type::OMNI_LONG, Encoding::OMNI_DICTIONARY>,                  \
        nullptr,                                                                   \
        nullptr,                                                                   \
        TMP_FUNC_PTR<type::OMNI_VARCHAR, Encoding::OMNI_DICTIONARY>,               \
        TMP_FUNC_PTR<type::OMNI_VARCHAR, Encoding::OMNI_DICTIONARY>,               \
        nullptr,                                                                   \
        TMP_FUNC_PTR<type::OMNI_BYTE, Encoding::OMNI_DICTIONARY>,                  \
        TMP_FUNC_PTR<type::OMNI_INT, Encoding::OMNI_DICTIONARY>,                   \
        TMP_FUNC_PTR<type::OMNI_VARBINARY, Encoding::OMNI_DICTIONARY>,             \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr,                                                                   \
        nullptr }

#define FUNC_CENTER_CONST_DEF(CENTER_NAME, FUNC_NAME, TMP_FUNC_PTR)                               \
    static std::vector<FUNC_NAME> CENTER_NAME = { nullptr,                                        \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_INT, Encoding::OMNI_ENCODING_CONST),               \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_LONG, Encoding::OMNI_ENCODING_CONST),              \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_DOUBLE, Encoding::OMNI_ENCODING_CONST),            \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_BOOLEAN, Encoding::OMNI_ENCODING_CONST),           \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_SHORT, Encoding::OMNI_ENCODING_CONST),             \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_LONG, Encoding::OMNI_ENCODING_CONST),              \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_DECIMAL128, Encoding::OMNI_ENCODING_CONST),        \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_INT, Encoding::OMNI_ENCODING_CONST),               \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_LONG, Encoding::OMNI_ENCODING_CONST),              \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_INT, Encoding::OMNI_ENCODING_CONST),               \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_LONG, Encoding::OMNI_ENCODING_CONST),              \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_LONG, Encoding::OMNI_ENCODING_CONST),              \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_VARCHAR, Encoding::OMNI_ENCODING_CONST),           \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_VARCHAR, Encoding::OMNI_ENCODING_CONST),           \
        nullptr,                                                                                  \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_BYTE, Encoding::OMNI_ENCODING_CONST),              \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_INT, Encoding::OMNI_ENCODING_CONST),               \
        TMP_FUNC_CALL(TMP_FUNC_PTR, type::OMNI_VARBINARY, Encoding::OMNI_ENCODING_CONST),         \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        nullptr,                                                                                  \
        nullptr }

using TransFuncPtr = void (*)(BaseVector *vec, int32_t rowIndex, BaseSerialize *value);
using GenFuncPtr = std::unique_ptr<BaseSerialize> (*)();
using RowToVecFuncPtr = uint8_t *(*)(uint8_t *row, BaseVector *vec, int32_t rowIndex);
using PrintRowFuncPtr = uint8_t *(*)(uint8_t *row);

FUNC_CENTER_DEF(Row2VecFuncCenter, RowToVecFuncPtr, RowToVec);

template <typename IntLikeType>
static uint8_t *FixedRowSetVecValue(uint8_t *row, Vector<IntLikeType> *vec, int32_t rowIndex)
{
    IntLikeType value = 0;
    bool negValue = false;
    if (*row & (1 << BaseSerialize::NEG_POS)) {
        negValue = true;
    }
    auto valueLen = (*row) & (0x0F);
    // point to real value
    ++row;
    ReadCompactLeInt(row, valueLen, &value);
    if (UNLIKELY(negValue)) {
        value = ~value;
    }
    vec->SetValue(rowIndex, value);
#ifdef DEBUG
    std::string out = "fix value parse value is " + std::to_string(value) + ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + valueLen;
}

template <typename IntLikeType> static uint8_t *FixedRowGetValue(uint8_t *row)
{
    bool negValue = false;
    if (*row & (1 << BaseSerialize::NEG_POS)) {
        negValue = true;
    }
    auto valueLen = (*row) & (0x0F);
    // point to real value
    ++row;
#ifdef DEBUG
    IntLikeType value = 0;
    ReadCompactLeInt(row, valueLen, &value);
    std::string out = "fix value parse value is " + std::to_string(negValue ? -value : value) + ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + valueLen;
}

static uint8_t *DecimalRowSetVecValue(uint8_t *row, Vector<Decimal128> *vec, int32_t rowIndex)
{
    type::Decimal128 value;
    ++row;
    std::copy(row, row + BaseSerialize::BIT_16, reinterpret_cast<uint8_t *>(&value));
    vec->SetValue(rowIndex, value);
#ifdef DEBUG
    std::string out = "Decimal value parse highBits is " + std::to_string(value.HighBits()) + ", lowBits is " +
        std::to_string(value.LowBits());
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + BaseSerialize::BIT_16;
}

static uint8_t *DecimalRowGetValue(uint8_t *row)
{
    type::Decimal128 value;
    ++row;
#ifdef DEBUG
    std::copy(row, row + BaseSerialize::BIT_16, reinterpret_cast<uint8_t *>(&value));
    std::string out = "Decimal value parse highBits is " + std::to_string(value.HighBits()) + ", lowBits is " +
        std::to_string(value.LowBits()) + ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + BaseSerialize::BIT_16;
}

static uint8_t *BoolRowSetVecValue(uint8_t *row, Vector<bool> *vec, int32_t rowIndex)
{
    bool value = (*row & 0x01);
    vec->SetValue(rowIndex, value);
    return row + BaseSerialize::BIT_1;
}

static uint8_t *DoubleRowSetVecValue(uint8_t *row, Vector<double> *vec, int32_t rowIndex)
{
    double value;
    ++row;
    std::copy(row, row + BaseSerialize::BIT_8, reinterpret_cast<uint8_t *>(&value));
    vec->SetValue(rowIndex, value);
#ifdef DEBUG
    std::string out = "double parse value is " + std::to_string(value);
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + BaseSerialize::BIT_8;
}

static uint8_t *DoubleRowGetValue(uint8_t *row)
{
    double value;
    ++row;
#ifdef DEBUG
    std::copy(row, row + BaseSerialize::BIT_8, reinterpret_cast<uint8_t *>(&value));
    std::string out = "double parse value is " + std::to_string(value) + ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + BaseSerialize::BIT_8;
}

static uint8_t *StringRowSetVecValue(uint8_t *row, Vector<LargeStringContainer<std::string_view>> *vec,
    int32_t rowIndex)
{
    auto valueLen = (*row) & ENCODING_META_SIZE;
    // point to strLen value
    ++row;
    int32_t strLen = 0;
    ReadCompactLeInt(row, valueLen, &strLen);
    // point to str
    row += valueLen;
    // row will be copied to value 's string buffer
    std::string_view value{ (char *)(row), strLen };
    vec->SetValue(rowIndex, value);
#ifdef DEBUG
    std::string out = "str parse value is ";
    out.append(value.data(), value.size());
    out += ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + strLen;
}

static uint8_t *StringRowGetValue(uint8_t *row)
{
    auto valueLen = (*row) & ENCODING_META_SIZE;
    // point to strLen value
    ++row;
    int32_t strLen = 0;
    ReadCompactLeInt(row, valueLen, &strLen);
    // point to str
    row += valueLen;
#ifdef DEBUG
    // row will be copied to value 's string buffer
    std::string_view value{ (char *)(row), strLen };
    std::string out = "str parse value is ";
    out.append(value.data(), value.size());
    out += ".";
    LogDebug("Row Handle:%s\n", out.c_str());
#endif
    return row + strLen;
}

static uint8_t *ArrayRowSetVecValue(uint8_t *row, ArrayVector *vec, int32_t rowIndex)
{
    auto rowArraySize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t arraySize = 0;
    ReadCompactLeInt(row, rowArraySize, &arraySize);
    row += rowArraySize;
    vec->SetOffset(rowIndex + 1, vec->GetOffset(rowIndex) + arraySize);
    auto elementVector = vec->GetElementVector();
    elementVector->Expand(vec->GetOffset(rowIndex + 1));
    DataTypeId elementDataTypeId = elementVector->GetTypeId();
    int64_t startOffset = vec->GetOffset(rowIndex);
    for (int32_t i = 0; i < arraySize; i++) {
        row = Row2VecFuncCenter[elementDataTypeId](row, elementVector.get(), startOffset + i);
    }
    return row;
}

static uint8_t *ArrayRowGetValue(uint8_t *row)
{
    auto rowArraySize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t arraySize = 0;
    ReadCompactLeInt(row, rowArraySize, &arraySize);
    row += rowArraySize;
    return row + arraySize;
}

static uint8_t *MapRowSetVecValue(uint8_t *row, MapVector *vec, int32_t rowIndex)
{
    auto rowMapSize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t mapSize = 0;
    ReadCompactLeInt(row, rowMapSize, &mapSize);
    row += rowMapSize;

    auto rowKeySize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t keySize = 0;
    ReadCompactLeInt(row, rowKeySize, &keySize);
    row += rowKeySize;
    vec->SetOffset(rowIndex + 1, vec->GetOffset(rowIndex) + keySize);
    auto keyVector = vec->GetKeyVector();
    keyVector->Expand(vec->GetOffset(rowIndex + 1));
    DataTypeId keyDataTypeId = keyVector->GetTypeId();
    int64_t startOffset = vec->GetOffset(rowIndex);
    for (int32_t i = 0; i < keySize; i++) {
        row = Row2VecFuncCenter[keyDataTypeId](row, keyVector.get(), startOffset + i);
    }

    auto rowValueSize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t valueSize = 0;
    ReadCompactLeInt(row, rowValueSize, &valueSize);
    row += rowValueSize;
    auto valueVector = vec->GetValueVector();
    valueVector->Expand(vec->GetOffset(rowIndex + 1));
    DataTypeId valueDataTypeId = valueVector->GetTypeId();
    for (int32_t i = 0; i < valueSize; i++) {
        row = Row2VecFuncCenter[valueDataTypeId](row, valueVector.get(), startOffset + i);
    }

    return row;
}
 
static uint8_t *MapRowGetValue(uint8_t *row)
{
    auto rowMapSize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t mapSize = 0;
    ReadCompactLeInt(row, rowMapSize, &mapSize);
    row += rowMapSize;
    return row + mapSize;
}

static uint8_t *StructRowSetVecValue(uint8_t *row, RowVector *vec, int32_t rowIndex)
{
    auto rowStructSize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t structSize = 0;
    ReadCompactLeInt(row, rowStructSize, &structSize);
    row += rowStructSize;

    for (int32_t i = 0; i < structSize; i++) {
        auto childArraySize = (*row) & ENCODING_META_SIZE;
        ++row;
        int32_t arraySize = 0;
        ReadCompactLeInt(row, childArraySize, &arraySize);
        row += childArraySize;

        auto childVector = vec->ChildAt(i);
        DataTypeId childDataTypeId = childVector->GetTypeId();
        for (int32_t i = 0; i < arraySize; i++) {
            row = Row2VecFuncCenter[childDataTypeId](row, childVector.get(), rowIndex + i);
        }
    }
    return row;
}

static uint8_t *StructRowGetValue(uint8_t *row)
{
    auto rowStructSize = (*row) & ENCODING_META_SIZE;
    ++row;
    int32_t structSize = 0;
    ReadCompactLeInt(row, rowStructSize, &structSize);
    row += rowStructSize;
    return row + structSize;
}

template <type::DataTypeId id> uint8_t *RowToVec(uint8_t *row, BaseVector *vec, int32_t rowIndex)
{
    if (row == nullptr || vec == nullptr) {
        return row;
    }

    using Type = typename NativeType<id>::type;
    using VectorType = typename VectorTypeTraits<Type>::type;
    auto *typeVector = reinterpret_cast<VectorType *>(vec);

    // check whether cur value is null, the pos is NULL_POS bit of one first bytes;
    if (*row & (0x1 << BaseSerialize::NULL_POS)) {
        typeVector->SetNull(rowIndex);
        return row + BaseSerialize::PrefixLen;
    } else {
        if constexpr (std::is_same_v<std::string_view, Type>) {
            return StringRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, double>) {
            return DoubleRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, Decimal128>) {
            return DecimalRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, bool>) {
            return BoolRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, BaseVector*>) {
            return ArrayRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, std::pair<BaseVector*, BaseVector*>>) {
            return MapRowSetVecValue(row, typeVector, rowIndex);
        } else if constexpr (std::is_same_v<Type, std::vector<BaseVector*>>) {
            return StructRowSetVecValue(row, typeVector, rowIndex);
        } else {
            return FixedRowSetVecValue<Type>(row, typeVector, rowIndex);
        }
    }
}

template <type::DataTypeId id> uint8_t *PrintRow(uint8_t *row)
{
    using Type = typename NativeType<id>::type;
    if (*row & (0x1 << BaseSerialize::NULL_POS)) {
#ifdef DEBUG
        std::string out = "the value is null. \n";
        LogDebug("Row Handle:%s\n", out.c_str());
#endif
    } else {
        if constexpr (std::is_same_v<std::string_view, Type>) {
            return StringRowGetValue(row);
        } else if constexpr (std::is_same_v<Type, double>) {
            return DoubleRowGetValue(row);
        } else if constexpr (std::is_same_v<Type, Decimal128>) {
            return DecimalRowGetValue(row);
        } else if constexpr (std::is_same_v<Type, BaseVector*>) {
            return ArrayRowGetValue(row);
        } else if constexpr (std::is_same_v<Type, std::pair<BaseVector*, BaseVector*>>) {
            return MapRowGetValue(row);
        } else if constexpr (std::is_same_v<Type, std::vector<BaseVector*>>) {
            return StructRowGetValue(row);
        } else {
            return FixedRowGetValue<Type>(row);
        }
    }
}

FUNC_CENTER_DEF(PrintRowFuncCenter, PrintRowFuncPtr, PrintRow);
FUNC_CENTER_FLAT_DEF(GenerateFlatValueFuncCenter, GenFuncPtr, GenerateSerValue);
FUNC_CENTER_FLAT_DEF(TransFlatValueFuncCenter, TransFuncPtr, TransToValue);
FUNC_CENTER_DICT_DEF(GenerateDictValueFuncCenter, GenFuncPtr, GenerateSerValue);
FUNC_CENTER_DICT_DEF(TransDictValueFuncCenter, TransFuncPtr, TransToValue);
FUNC_CENTER_CONST_DEF(GenerateConstValueFuncCenter, GenFuncPtr, GenerateSerValue);
FUNC_CENTER_CONST_DEF(TransConstValueFuncCenter, TransFuncPtr, TransToValue);
/*
 * Row buffer is used to trans one row of VectorBatch to row
 * the basic usage of RowBuffer:
 * Traverse All Rows of vb and call as following:
 * 1 RowBuffer.TransValue(vb, i);
 * 2 RowBuffer.FillBuffer();
 * 3 auto *buffer = RowBuffer.TakeRowBuffer();
 * 4 rowBatch.SetRow(index, buffer);
 */
class RowBuffer {
public:
    // used for test
    explicit RowBuffer(std::vector<DataTypePtr> &types, int32_t keyColumnSize = 0)
        : columnSize(types.size()), keyColumn(keyColumnSize)
    {
        oneRow.resize(columnSize);
        transFuns.resize(columnSize);
        for (int i = 0; i < types.size(); i++) {
            ConstructFuncById(i, types[i]->GetId());
        }
    }

    RowBuffer(std::vector<type::DataTypeId> &types, std::vector<Encoding> &encodings, int32_t keyColumnSize = 0)
        : columnSize(types.size()), keyColumn(keyColumnSize)
    {
        oneRow.resize(columnSize);
        transFuns.resize(columnSize);
        for (int i = 0; i < types.size(); i++) {
            ConstructFuncById(i, types[i], encodings[i]);
        }
    }

    ~RowBuffer() {}

    void TransValue(BaseVector **vecs, int32_t rowIndex)
    {
        for (int32_t i = 0; i < columnSize; ++i) {
            transFuns[i](vecs[i], rowIndex, oneRow[i].get());
        }
    }

    void TransValueFromVectorBatch(vec::VectorBatch *vb, int32_t index)
    {
        for (int32_t i = 0; i < columnSize; ++i) {
            transFuns[i](vb->Get(i), index, oneRow[i].get());
        }
    }

    int32_t CalculateCompactLength(int32_t endColumn)
    {
        int32_t ret = 0;
        for (int32_t i = 0; i < endColumn; ++i) {
            auto len = oneRow[i]->CompactLength();
            ret += len;
        }
        return ret;
    }

    int32_t FillBuffer(SimpleArenaAllocator &arena)
    {
        oneRowLen = CalculateCompactLength(columnSize);
        originBuffer = (uint8_t *)(arena.Allocate(oneRowLen));
        writeBuffer = originBuffer;
        for (int32_t i = 0; i < columnSize; ++i) {
            writeBuffer = oneRow[i]->WriteBuffer(writeBuffer);
        }
#ifdef DEBUG
        LogDebug("originBuffer + oneRowLen - writeBuffer = %d\n", originBuffer + oneRowLen - writeBuffer);
#endif
        return oneRowLen;
    }

    int32_t FillBuffer()
    {
        oneRowLen = CalculateCompactLength(columnSize);
        originBuffer = (uint8_t *)(mem::Allocator::GetAllocator()->Alloc(oneRowLen));
        writeBuffer = originBuffer;
        for (int32_t i = 0; i < columnSize; ++i) {
            writeBuffer = oneRow[i]->WriteBuffer(writeBuffer);
        }
#ifdef DEBUG
        LogDebug("originBuffer + oneRowLen - writeBuffer = %d\n", originBuffer + oneRowLen - writeBuffer);
#endif
        return oneRowLen;
    }

    int32_t CalculateHashPos()
    {
        return CalculateCompactLength(keyColumn);
    }

    uint8_t *GetRowBuffer()
    {
        return originBuffer;
    }

    uint8_t *TakeRowBuffer()
    {
        auto *retBuffer = originBuffer;
        originBuffer = nullptr;
        return retBuffer;
    }

    BaseSerialize *GetOneOfRow(int32_t index)
    {
        return oneRow[index].get();
    }

    // this api can be called after FillBuffer()
    int32_t GetCurRowLen()
    {
        return oneRowLen;
    }

private:
    void ConstructFuncById(int32_t i, DataTypeId id, Encoding encoding = OMNI_FLAT)
    {
        if (LIKELY(encoding == OMNI_FLAT)) {
            oneRow[i] = GenerateFlatValueFuncCenter[id]();
            transFuns[i] = TransFlatValueFuncCenter[id];
        } else if (encoding == OMNI_DICTIONARY) {
            oneRow[i] = GenerateDictValueFuncCenter[id]();
            transFuns[i] = TransDictValueFuncCenter[id];
        } else if (encoding == OMNI_ENCODING_ARRAY) {
            oneRow[i] = GenerateFlatValueFuncCenter[id]();
            transFuns[i] = TransFlatValueFuncCenter[id];
        } else if (encoding == OMNI_ENCODING_MAP) {
            oneRow[i] = GenerateFlatValueFuncCenter[id]();
            transFuns[i] = TransFlatValueFuncCenter[id];
        } else if (encoding == OMNI_ENCODING_STRUCT) {
            oneRow[i] = GenerateFlatValueFuncCenter[id]();
            transFuns[i] = TransFlatValueFuncCenter[id];
        } else if (encoding == OMNI_ENCODING_CONST) {
            oneRow[i] = GenerateConstValueFuncCenter[id]();
            transFuns[i] = TransConstValueFuncCenter[id];
        } else {
            // OMNI_ENCODING_CONTAINER is only used for the agg avg partial in olk. row shuffle is not supported.
            std::string message = "encoding type " + std::to_string(static_cast<int>(encoding)) + " is not supported for omni row";
            throw omniruntime::exception::OmniException("Encoding Unsupported", message);
        }
    }

    std::vector<std::unique_ptr<BaseSerialize>> oneRow;
    std::vector<TransFuncPtr> transFuns;
    int32_t oneRowLen;
    // it is necessaries for shuffle when calculate partition id
    // key column is 0 -> keyColumn
    int32_t keyColumn;
    uint8_t *originBuffer;
    uint8_t *writeBuffer;
    int32_t columnSize;
};

class RowParser {
public:
    RowParser(const std::vector<DataTypeId> &types)
    {
        ResizeFuncs(types.size());
        for (int i = 0; i < types.size(); ++i) {
            PushBackFunc(types[i]);
        }
    }

    RowParser(DataTypeId *types, long *vecs, int32_t len)
    {
        ResizeFuncs(len);
        for (int i = 0; i < len; ++i) {
            PushBackFunc(types[i]);
        }
        this->vecs = vecs;
    }

    RowParser(std::vector<DataTypePtr> &typeIds)
    {
        ResizeFuncs(typeIds.size());
        for (int i = 0; i < typeIds.size(); ++i) {
            PushBackFunc(typeIds[i]->GetId());
        }
    }

    void ParseOneRow(uint8_t *row, BaseVector **vec, int32_t rowIndex)
    {
        for (int i = 0; i < typeParser.size(); ++i) {
            auto parseFunc = typeParser[i];
            row = parseFunc(row, vec[i], rowIndex);
        }
    }

    // friendly to jni
    void ParseOnRow(uint8_t *row, int32_t rowIndex)
    {
        for (int i = 0; i < typeParser.size(); ++i) {
            auto parseFunc = typeParser[i];
            row = parseFunc(row, (BaseVector *)(vecs[i]), rowIndex);
        }
    }

    void PrintOneRow(uint8_t *row)
    {
        for (int i = 0; i < typeParser.size(); ++i) {
            auto printFunc = printParser[i];
            row = printFunc(row);
        }
    }

    ~RowParser()
    {
        delete[] vecs;
    }

private:
    void ResizeFuncs(int32_t size)
    {
        typeParser.reserve(size);
        printParser.reserve(size);
    }

    void PushBackFunc(DataTypeId id)
    {
        typeParser.push_back(Row2VecFuncCenter[id]);
        printParser.push_back(PrintRowFuncCenter[id]);
    }

    std::vector<RowToVecFuncPtr> typeParser;
    std::vector<PrintRowFuncPtr> printParser;
    long *vecs = nullptr;
};

class RowInfo {
public:
    uint8_t *row;
    int32_t length;

    RowInfo(uint8_t *row, int32_t length) : row(row), length(length) {}

    RowInfo() = default;

    RowInfo(const RowInfo &rowInfo)
    {
        this->row = rowInfo.row;
        this->length = rowInfo.length;
    }

    ~RowInfo()
    {
        mem::Allocator::GetAllocator()->Free(row, length);
    }
};

// friend for shuffle
class RowBatch {
public:
    RowBatch(int32_t rowCount)
    {
        rowArray.resize(rowCount);
    }

    RowBatch(int32_t rowCount, const std::vector<DataTypeId> &typeIds)
    {
        rowArray.resize(rowCount);
        this->types.reserve(typeIds.size());
        for (int i = 0; i < typeIds.size(); ++i) {
            types[i] = typeIds[i];
        }
    }

    void Resize(int32_t rowCount)
    {
        rowArray.resize(rowCount);
    }

    int32_t GetRowCount()
    {
        return rowArray.size();
    }

    void SetTypes(std::vector<DataTypeId> typeIds)
    {
        types.reserve(typeIds.size());
        for (int i = 0; i < typeIds.size(); ++i) {
            types[i] = typeIds[i];
        }
    }

    void SetRow(int32_t index, uint8_t *buffer)
    {
        rowArray[index]->row = buffer;
    }

    void SetRow(int32_t index, RowInfo *info)
    {
        rowArray[index] = info;
    }

    /*
     * get one row
     */
    RowInfo *Get(int32_t index)
    {
        return rowArray[index];
    }

    ~RowBatch()
    {
        // we cant free rowBatch now
        // for adaptor, memory in rowArray will be reuse
        rowArray.clear();
        types.clear();
    }

    void PrintRowBatch(std::vector<DataTypeId> types)
    {
        auto parser = std::make_unique<RowParser>(types);
        for (int i = 0; i < rowArray.size(); ++i) {
            std::string out = "--------row info--------";
            LogDebug("Row Handle:%s\n", out.c_str());
            parser->PrintOneRow(rowArray[i]->row);
        }
    }

private:
    std::vector<RowInfo *> rowArray;
    std::vector<DataTypeId> types;
};
}
}
#endif // OMNI_RUNTIME_OMNI_ROW_H
