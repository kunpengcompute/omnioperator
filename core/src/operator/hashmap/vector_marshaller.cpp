/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vector_marshaller.h"
#include "operator/omni_id_type_vector_traits.h"
#include "vector/unsafe_vector.h"

static constexpr int32_t BYTE_1 = 1;
static constexpr int32_t BYTE_2 = 2;
static constexpr int32_t BYTE_4 = 4;
static constexpr int32_t BYTE_8 = 8;
static constexpr int32_t BYTE_16 = 16;
static constexpr int32_t BIT_8 = 8;
static constexpr int32_t BIT_32 = 32;
static constexpr int32_t BIT_64 = 64;

namespace omniruntime {
namespace op {
template <DataTypeId id> const char *VariableTypeDeserializer(BaseVector *baseVector, size_t rowIdx, const char *pos)
{
    using RealVector = typename NativeAndVectorType<id>::vector;
    auto realVector = static_cast<RealVector *>(baseVector);
    auto rowLenSize = *reinterpret_cast<const uint8_t *>(pos);

    if (rowLenSize != 0) {
        // decompress rowLenSize byte to stringLen.
        auto stringLen = 0;
        switch (rowLenSize) {
            case BYTE_1: stringLen = *reinterpret_cast<const int8_t *>(pos + sizeof(uint8_t)); break;
            case BYTE_2: stringLen = *reinterpret_cast<const int16_t *>(pos + sizeof(uint8_t)); break;
            case BYTE_4: stringLen = *reinterpret_cast<const int32_t *>(pos + sizeof(uint8_t)); break;
            default: throw OmniException("DESERIALIZED FAILED", "Invalid String Length");
        }
        pos = pos + sizeof(uint8_t) + rowLenSize;
        std::string_view strView(pos, stringLen);
        realVector->SetValue(rowIdx, strView);
        return pos + stringLen;
    } else {
        realVector->SetNull(rowIdx);
        return pos + sizeof(uint8_t);
    }
}

void ALWAYS_INLINE VariableTypeSerializer(const std::string_view &inValue, mem::SimpleArenaAllocator &arenaAllocator,
                                          StringRef &result)
{
    auto stringLen = static_cast<int32_t>(inValue.size());
    // __builtin_clzll is a built-in function of gcc and clang compiler
    uint8_t rowLenSize = (BIT_64 - __builtin_clzll(stringLen)) / BIT_8;
    // find the value of the closest PowerOfTwo for rowDataSize
    rowLenSize = BYTE_1 << (BIT_32 - __builtin_clz(rowLenSize));

    auto resLen = sizeof(uint8_t) + rowLenSize + stringLen;
    auto *&data = result.data;
    auto pos = arenaAllocator.AllocateContinue(resLen, reinterpret_cast<const uint8_t *&>(data));
    *reinterpret_cast<uint8_t *>(pos) = rowLenSize;

    // compress stringLen from 4 bytes to rowLenSize bytes
    // copy stringLen
    memcpy_s(pos + sizeof(uint8_t), rowLenSize, &stringLen, rowLenSize);
    // copy string
    std::copy(inValue.data(), inValue.data() + stringLen, pos + sizeof(uint8_t) + rowLenSize);
    result.size += resLen;
}

void NullVariableTypeSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resultSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resultSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0; // value is null
    result.size += resultSize;
}

const char *Decimal128Deserializer(BaseVector *baseVector, size_t rowIdx, const char *pos)
{
    auto realVector = reinterpret_cast<vec::Vector<Decimal128> *>(baseVector);
    uint8_t rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    if (rowDataSize != 0) {
        // must copy value
        auto *copyPointer = reinterpret_cast<const Decimal128 *>(pos + sizeof(uint8_t));
        auto value = *copyPointer;
        realVector->SetValue(rowIdx, value);
        return pos + rowDataSize + sizeof(uint8_t);
    } else {
        realVector->SetNull(rowIdx);
        return pos + sizeof(uint8_t);
    }
}

template <typename T> T GetValue(uint8_t rowDataSize, const char *pos)
{
    T value;
    switch (rowDataSize) {
        case BYTE_1: value = static_cast<T>(*reinterpret_cast<const int8_t *>(pos)); break;
        case BYTE_2: value = static_cast<T>(*reinterpret_cast<const int16_t *>(pos)); break;
        case BYTE_4: value = static_cast<T>(*reinterpret_cast<const int32_t *>(pos)); break;
        case BYTE_8:
            if constexpr (std::is_same_v<T, double>) {
                value = *reinterpret_cast<const double *>(pos);
            } else {
                value = static_cast<T>(*reinterpret_cast<const int64_t *>(pos));
            }
            break;
        default:
            auto message = "Invalid Fixed Value Size: " + std::to_string(rowDataSize);
            throw OmniException("DESERIALIZED FAILED", message);
    }
    return value;
}

template <DataTypeId id> const char *FixedLenTypeDeserializer(BaseVector *baseVector, size_t rowIdx, const char *pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    using RealVector = typename NativeAndVectorType<id>::vector;
    auto realVector = static_cast<RealVector *>(baseVector);
    auto rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    if (rowDataSize != 0) {
        // value is not null, must copy value
        auto value = GetValue<RawDataType>(rowDataSize, pos + sizeof(uint8_t));
        realVector->SetValue(rowIdx, value);
        return pos + sizeof(uint8_t) + rowDataSize;
    } else {
        // value is null
        realVector->SetNull(rowIdx);
        return pos + sizeof(uint8_t);
    }
}

void Decimal128Serializer(Decimal128 &value, mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    uint8_t rowDataSize = BYTE_16;
    auto resultSize = sizeof(uint8_t) + rowDataSize;
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resultSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = rowDataSize;
    *reinterpret_cast<Decimal128 *>((pos + sizeof(uint8_t))) = value;
    result.size += resultSize;
}

template <typename RawDataType>
void FixedLenTypeSerializer(RawDataType value, mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    uint8_t rowDataSize;
    if constexpr (std::is_same_v<RawDataType, double>) {
        rowDataSize = BYTE_8;
    } else if constexpr (std::is_same_v<RawDataType, bool>) {
        rowDataSize = BYTE_1;
    } else {
        auto tmp = value;
        if (value < 0) {
            tmp = ~tmp;
        }
        // __builtin_clzll is a built-in function of gcc and clang compiler
        rowDataSize = (BIT_64 - __builtin_clzll(tmp)) / BIT_8;
        // find the value of the closest PowerOfTwo for rowDataSize
        rowDataSize = BYTE_1 << (BIT_32 - __builtin_clz(rowDataSize));
    }

    auto resultSize = sizeof(int8_t) + rowDataSize;
    auto *&data = result.data;

    auto *pos = arenaAllocator.AllocateContinue(resultSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = rowDataSize;
    // copy value
    memcpy_s(pos + sizeof(uint8_t), rowDataSize, &value, rowDataSize);
    result.size += resultSize;
}

void NullDecimal128Serializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resultSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resultSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0;
    result.size += resultSize;
}

void NullFixedLenTypeSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resultSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resultSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0; // value is null
    result.size += resultSize;
}

template <type::DataTypeId id>
void SerializeValueIntoArena(BaseVector *baseVector, int32_t rowIdx, mem::SimpleArenaAllocator &arenaAllocator,
                             StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        using RawVectorType = typename NativeAndVectorType<id>::vector;

        // not dictionary,just use cast to RawVector
        auto rawVector = static_cast<RawVectorType *>(baseVector);
        auto value = rawVector->GetValue(rowIdx);

        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            VariableTypeSerializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            Decimal128Serializer(value, arenaAllocator, result);
        } else {
            FixedLenTypeSerializer<RawDataType>(value, arenaAllocator, result);
        }
    } else {
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            NullVariableTypeSerializer(arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            NullDecimal128Serializer(arenaAllocator, result);
        } else {
            NullFixedLenTypeSerializer(arenaAllocator, result);
        }
    }
}

template <type::DataTypeId id>
void SerializeDictionaryValueIntoArena(BaseVector *baseVector, int32_t rowIdx,
                                       mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    if (!baseVector->IsNull(rowIdx)) {
        auto dictionaryVector = static_cast<Vector<DictionaryContainer<RawDataType>> *>(baseVector);

        auto value = dictionaryVector->GetValue(rowIdx);
        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            VariableTypeSerializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            Decimal128Serializer(value, arenaAllocator, result);
        } else {
            FixedLenTypeSerializer<RawDataType>(value, arenaAllocator, result);
        }
        return;
    }

    if constexpr (std::is_same_v<RawDataType, std::string_view>) {
        NullVariableTypeSerializer(arenaAllocator, result);
    } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
        NullDecimal128Serializer(arenaAllocator, result);
    } else {
        NullFixedLenTypeSerializer(arenaAllocator, result);
    }
}

template <type::DataTypeId id>
const char *DeserializeFromPointer(BaseVector *baseVector, int32_t rowIdx, const char *&begin)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    // the analysis of const expr  will be in compile stage
    if constexpr (std::is_same_v<RawDataType, std::string_view>) {
        return VariableTypeDeserializer<id>(baseVector, rowIdx, begin);
    } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
        return Decimal128Deserializer(baseVector, rowIdx, begin);
    } else {
        return FixedLenTypeDeserializer<id>(baseVector, rowIdx, begin);
    }
}

std::vector<VectorSerializer> vectorSerializerCenter = {
    nullptr,                                        // OMNI_NONE,
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeValueIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeValueIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeValueIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeValueIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                        // OMNI_INTERVAL_MONTHS
    nullptr,                                        // OMNI_INTERVAL_DAY_TIME
    SerializeValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR
    SerializeValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR,
    nullptr,                                        // OMNI_CONTAINER,
    SerializeValueIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<VectorSerializer> dicVectorSerializerCenter = {
    nullptr,                                                  // OMNI_NONE,
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeDictionaryValueIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeDictionaryValueIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeDictionaryValueIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeDictionaryValueIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                  // OMNI_INTERVAL_MONTHS
    nullptr,                                                  // OMNI_INTERVAL_DAY_TIME
    SerializeDictionaryValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR
    SerializeDictionaryValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR,
    nullptr,                                                  // OMNI_CONTAINER,
    SerializeDictionaryValueIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<VectorDeSerializer> vectorDeSerializerCenter = {
    nullptr,                                       // OMNI_NONE,
    DeserializeFromPointer<type::OMNI_INT>,        // OMNI_INT
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_LONG
    DeserializeFromPointer<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    DeserializeFromPointer<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    DeserializeFromPointer<type::OMNI_SHORT>,      // OMNI_SHORT
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    DeserializeFromPointer<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    DeserializeFromPointer<type::OMNI_INT>,        // OMNI_DATE32
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_DATE64
    DeserializeFromPointer<type::OMNI_INT>,        // /OMNI_TIME32
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_TIME64
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                       // OMNI_INTERVAL_MONTHS
    nullptr,                                       // OMNI_INTERVAL_DAY_TIME
    DeserializeFromPointer<type::OMNI_VARCHAR>,    // OMNI_VARCHAR
    DeserializeFromPointer<type::OMNI_VARCHAR>,    // OMNI_CHAR,
    nullptr,                                       // OMNI_CONTAINER,
    DeserializeFromPointer<type::OMNI_BYTE>        // OMNI_BYTE
};

template <typename RawDataType>
void ALWAYS_INLINE FixedLenTypeSerializerForJoin(RawDataType value, mem::SimpleArenaAllocator &arenaAllocator,
                                                 StringRef &result)
{
    static constexpr uint8_t RawDataSize = sizeof(RawDataType);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(RawDataSize, reinterpret_cast<const uint8_t *&>(data));
    *reinterpret_cast<RawDataType *>(pos) = value;
    result.size += RawDataSize;
}

void ALWAYS_INLINE Decimal128SerializerForJoin(Decimal128 &value, mem::SimpleArenaAllocator &arenaAllocator,
                                               StringRef &result)
{
    static constexpr uint8_t RawDataSize = sizeof(Decimal128);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(RawDataSize, reinterpret_cast<const uint8_t *&>(data));
    *reinterpret_cast<Decimal128 *>(pos) = value;
    result.size += RawDataSize;
}

template <type::DataTypeId id>
bool SerializeValueIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx,
                                       mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        using RawVectorType = typename NativeAndVectorType<id>::vector;
        auto rawVector = reinterpret_cast<RawVectorType *>(baseVector);
        auto value = rawVector->GetValue(rowIdx);

        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            VariableTypeSerializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            Decimal128SerializerForJoin(value, arenaAllocator, result);
        } else {
            FixedLenTypeSerializerForJoin<RawDataType>(value, arenaAllocator, result);
        }
        return true;
    } else {
        return false;
    }
}

template <type::DataTypeId id>
bool SerializeDictionaryValueIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx,
                                                 mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    if (!baseVector->IsNull(rowIdx)) {
        auto dictionaryVector = reinterpret_cast<Vector<DictionaryContainer<RawDataType>> *>(baseVector);

        auto value = dictionaryVector->GetValue(rowIdx);
        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            VariableTypeSerializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            Decimal128SerializerForJoin(value, arenaAllocator, result);
        } else {
            FixedLenTypeSerializerForJoin<RawDataType>(value, arenaAllocator, result);
        }
        return true;
    }

    return false;
}

template <type::DataTypeId id> std::string DeserializeKeyFromPointer(const char *&pos)
{
    std::string str;
    using RawDataType = typename NativeAndVectorType<id>::type;
    if constexpr (std::is_same_v<RawDataType, std::string_view>) {
        auto stringSize = *reinterpret_cast<const int32_t *>(pos);
        pos += sizeof(int32_t);
        std::string strView(pos, stringSize);
        str = strView;
        pos += stringSize;
    } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
        uint8_t RawDataSize = sizeof(RawDataType);
        str = (*reinterpret_cast<const RawDataType *>(pos)).ToString();
        pos += RawDataSize;
    } else {
        uint8_t RawDataSize = sizeof(RawDataType);
        str = std::to_string(*reinterpret_cast<const RawDataType *>(pos));
        pos += RawDataSize;
    }
    return str;
}

std::vector<VectorSerializerIgnoreNull> vectorSerializerIgnoreNullCenter = {
    nullptr,                                                  // OMNI_NONE,
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeValueIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeValueIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeValueIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeValueIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                  // OMNI_INTERVAL_MONTHS
    nullptr,                                                  // OMNI_INTERVAL_DAY_TIME
    SerializeValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR
    SerializeValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR,
    nullptr,                                                  // OMNI_CONTAINER,
    SerializeValueIgnoreNullIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<VectorSerializerIgnoreNull> dicVectorSerializerIgnoreNullCenter = {
    nullptr,                                                            // OMNI_NONE,
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                            // OMNI_INTERVAL_MONTHS
    nullptr,                                                            // OMNI_INTERVAL_DAY_TIME
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR,
    nullptr,                                                            // OMNI_CONTAINER,
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

template <typename RawDataType>
void ALWAYS_INLINE FixedKeysSerializerForJoin(RawDataType value, StringRef &result, size_t &pos)
{
    static constexpr uint8_t rawDataSize = sizeof(RawDataType);
    *reinterpret_cast<RawDataType *>(const_cast<char *>(result.data) + pos) = value;
    pos += rawDataSize;
}

template <type::DataTypeId id>
bool SerializeFixedKeysIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx, StringRef &result, size_t &pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        auto rawVector = reinterpret_cast<Vector<RawDataType> *>(baseVector);
        auto value = rawVector->GetValue(rowIdx);
        FixedKeysSerializerForJoin<RawDataType>(value, result, pos);
        return true;
    }
    return false;
}

template <type::DataTypeId id>
bool SerializeDictionaryFixedKeysIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx, StringRef &result,
                                                     size_t &pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    if (!baseVector->IsNull(rowIdx)) {
        auto dictionaryVector = reinterpret_cast<Vector<DictionaryContainer<RawDataType>> *>(baseVector);
        auto value = dictionaryVector->GetValue(rowIdx);
        FixedKeysSerializerForJoin<RawDataType>(value, result, pos);
        return true;
    }

    return false;
}

template <typename RawDataType>
void ALWAYS_INLINE FixedKeysSerializerForJoinSimd(RawDataType value, StringRef &result, size_t &pos)
{
    static constexpr uint8_t rawDataSize = sizeof(RawDataType);
    *reinterpret_cast<RawDataType *>(const_cast<char *>(result.data) + pos) = value;
}

template <type::DataTypeId id>
bool SerializeFixedKeysIgnoreNullIntoArenaSimd(BaseVector *baseVector, int32_t rowid, std::vector<StringRef> &result,
                                               size_t &pos, int32_t joinRownum)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    static constexpr uint8_t rawDataSize = sizeof(RawDataType);

    if (!baseVector->IsNull(rowid)) {
        auto rawVector = reinterpret_cast<Vector<RawDataType> *>(baseVector);
        auto value = rawVector->GetValue(rowid);
        FixedKeysSerializerForJoinSimd<RawDataType>(value, result[rowid], pos);
        if (rowid == joinRownum - 1) {
            pos += rawDataSize;
        }
        return true;
    }
    if (rowid == joinRownum - 1) {
        pos += rawDataSize;
    }

    return false;
}

template <type::DataTypeId id>
bool SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd(BaseVector *baseVector, int32_t rowid,
                                                         std::vector<StringRef> &result, size_t &pos,
                                                         int32_t joinRownum)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    static constexpr uint8_t rawDataSize = sizeof(RawDataType);

    if (!baseVector->IsNull(rowid)) {
        auto dictionaryVector = reinterpret_cast<Vector<DictionaryContainer<RawDataType>> *>(baseVector);
        auto value = dictionaryVector->GetValue(rowid);
        FixedKeysSerializerForJoinSimd<RawDataType>(value, result[rowid], pos);
        if (rowid == joinRownum - 1) {
            pos += rawDataSize;
        }
        return true;
    }
    if (rowid == joinRownum - 1) {
        pos += rawDataSize;
    }
    return false;
}

std::vector<FixedKeyVectorSerializerIgnoreNull> vectorSerializerFixedKeysIgnoreNullCenter = {
    nullptr,                                                      // OMNI_NONE,
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                      // OMNI_INTERVAL_MONTHS
    nullptr,                                                      // OMNI_INTERVAL_DAY_TIME
    nullptr,                                                      // OMNI_VARCHAR
    nullptr,                                                      // OMNI_CHAR,
    nullptr,                                                      // OMNI_CONTAINER,
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<FixedKeyVectorSerializerIgnoreNull> dicVectorSerializerFixedKeysIgnoreNullCenter = {
    nullptr,                                                                // OMNI_NONE,
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                                // OMNI_INTERVAL_MONTHS
    nullptr,                                                                // OMNI_INTERVAL_DAY_TIME
    nullptr,                                                                // OMNI_VARCHAR
    nullptr,                                                                // OMNI_CHAR,
    nullptr,                                                                // OMNI_CONTAINER,
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<FixedKeyVectorSerializerIgnoreNullSimd> vectorSerializerFixedKeysIgnoreNullCenterSimd = {
    nullptr,                                                          // OMNI_NONE,
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_INT
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_LONG
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_DATE32
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_TIME32
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                          // OMNI_INTERVAL_MONTHS
    nullptr,                                                          // OMNI_INTERVAL_DAY_TIME
    nullptr,                                                          // OMNI_VARCHAR
    nullptr,                                                          // OMNI_CHAR,
    nullptr,                                                          // OMNI_CONTAINER,
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BYTE>        // OMNI_BYTE
};

std::vector<FixedKeyVectorSerializerIgnoreNullSimd> dicVectorSerializerFixedKeysIgnoreNullCenterSimd = {
    nullptr,                                                                    // OMNI_NONE,
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_INT
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_LONG
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DOUBLE>,     // OMNI_DOUBLE
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_SHORT>,      // OMNI_SHORT
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DECIMAL64,
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_DATE32
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DATE64
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_TIME32
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIME64
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIMESTAMP
    nullptr,                                                                    // OMNI_INTERVAL_MONTHS
    nullptr,                                                                    // OMNI_INTERVAL_DAY_TIME
    nullptr,                                                                    // OMNI_VARCHAR
    nullptr,                                                                    // OMNI_CHAR,
    nullptr,                                                                    // OMNI_CONTAINER,
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BYTE>        // OMNI_BYTE
};
} // namespace op
} // namespace omniruntime
