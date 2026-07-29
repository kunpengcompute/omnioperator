/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */

#include "vector_marshaller.h"
#include "operator/omni_id_type_vector_traits.h"
#include "vector/unsafe_vector.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vector/map_vector.h"
#include "vector/vector.h"
#include "util/omni_exception.h"
#include <unordered_map>

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
            size_t stringLen = 0;
            switch (rowLenSize) {
            case BYTE_1:
                stringLen = *reinterpret_cast<const uint8_t *>(pos + sizeof(uint8_t));
                break;
            case BYTE_2:
                stringLen = *reinterpret_cast<const uint16_t *>(pos + sizeof(uint8_t));
                break;
            case BYTE_4:
                stringLen = *reinterpret_cast<const uint32_t *>(pos + sizeof(uint8_t));
                break;
            default:
                throw OmniException("DESERIALIZED FAILED", "Invalid String Length");
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

ALWAYS_INLINE bool DoVariableTypeCompare(const std::string_view &inValue, uint8_t *&pos)
{
    uint8_t rowLenSize = *pos;
    if (rowLenSize == 0) {
        pos += sizeof(uint8_t);
        return false;
    }
    size_t stringLen = 0;
    switch (rowLenSize) {
        case BYTE_1:
            stringLen = *reinterpret_cast<const uint8_t *>(pos + sizeof(uint8_t));
            break;
        case BYTE_2:
            stringLen = *reinterpret_cast<const uint16_t *>(pos + sizeof(uint8_t));
            break;
        case BYTE_4:
            stringLen = *reinterpret_cast<const uint32_t *>(pos + sizeof(uint8_t));
            break;
        default:
            throw OmniException("COMPARE FAILED", "Invalid String Length");
    }
    if (inValue.size() != stringLen) return false;
    const char* rowData = reinterpret_cast<const char*>(pos) + sizeof(uint8_t) + rowLenSize;
    pos += sizeof(uint8_t) + rowLenSize + stringLen;
    return std::memcmp(rowData, inValue.data(), stringLen) == 0;
}

template <DataTypeId id> bool VariableTypeComparator(BaseVector &baseVector, size_t rowIdx, uint8_t *&pos)
{
    using RealVector = typename NativeAndVectorType<id>::vector;
    auto realVector = static_cast<RealVector *>(&baseVector);
    std::string_view value = realVector->GetValue(rowIdx);
    uint8_t rowLenSize = *pos;
    if (rowLenSize == 0) {
        pos += sizeof(uint8_t);
        return baseVector.IsNull(rowIdx);
    }
    return DoVariableTypeCompare(value, pos);
}

ALWAYS_INLINE void VariableTypeSerializer(const std::string_view &inValue, mem::SimpleArenaAllocator &arenaAllocator,
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
    memcpy(pos + sizeof(uint8_t), &stringLen, rowLenSize);
    // copy string
    std::copy(inValue.data(), inValue.data() + stringLen, pos + sizeof(uint8_t) + rowLenSize);
    result.size += resLen;
}

void NullVariableTypeSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0; // value is null
    result.size += resSize;
}

void NullArrayVectorSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0;
    result.size += resSize;
}

void NullRowVectorSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0; // value is null
    result.size += resSize;
}

void NullMapVectorSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0;
    result.size += resSize;
}

uint8_t GetCompactLengthSize(uint64_t value)
{
    if (value == 0) {
        return BYTE_1;
    }
    int bitWidth = 64 - __builtin_clzll(value);
    int byteSize = (bitWidth + 7) / 8;

    if (byteSize > 4) {
        return BYTE_8;
    }
    if (byteSize > 2) {
        return BYTE_4;
    }
    if (byteSize > 1) {
        return BYTE_2;
    }
    return BYTE_1;
}

// Hash / join key serialization: respect dictionary- or const-encoded nested columns (e.g. struct fields).
ALWAYS_INLINE VectorSerializer SelectSerializerByEncodingAndType(BaseVector *vec, type::DataTypeId typeId)
{
    const size_t idx = static_cast<size_t>(typeId);
    VectorSerializer serializer = nullptr;
    if (vec->GetEncoding() == Encoding::OMNI_DICTIONARY && idx < dicVectorSerializerCenter.size() &&
        dicVectorSerializerCenter[typeId] != nullptr) {
        serializer = dicVectorSerializerCenter[typeId];
    } else if (vec->GetEncoding() == Encoding::OMNI_ENCODING_CONST && idx < constVectorSerializerCenter.size() &&
        constVectorSerializerCenter[typeId] != nullptr) {
        serializer = constVectorSerializerCenter[typeId];
    }
    if (serializer != nullptr) {
        return serializer;
    }
    if (idx < vectorSerializerCenter.size()) {
        serializer = vectorSerializerCenter[typeId];
    }
    if (serializer == nullptr) {
        auto it = complexVectorSerializerCenter.find(typeId);
        if (it != complexVectorSerializerCenter.end()) {
            serializer = it->second;
        }
    }
    return serializer;
}

void ALWAYS_INLINE ArrayVectorSerializer(ArrayVector &arrayVector, int32_t rowIdx, mem::SimpleArenaAllocator
    &arenaAllocator, StringRef &result) {
    int64_t offset = arrayVector.GetOffset(rowIdx);
    int64_t size = arrayVector.GetSize(rowIdx);

    uint8_t sizeLenSize = GetCompactLengthSize(size);
    auto *&dataRef = result.data;
    auto pos = arenaAllocator.AllocateContinue(sizeof(uint8_t) + sizeLenSize, reinterpret_cast<const uint8_t *&>(dataRef));
    *pos = sizeLenSize;
    memcpy(pos + 1, &size, sizeLenSize);
    result.size += sizeof(uint8_t) + sizeLenSize;

    auto elementVec = arrayVector.GetElementVector().get();
    int64_t start = offset;
    int64_t end = offset + size;
    auto elementTypeId = elementVec->GetTypeId();

    auto serializer = SelectSerializerByEncodingAndType(elementVec, static_cast<type::DataTypeId>(elementTypeId));
    if (serializer == nullptr) {
        auto message = "Finding serializer for ArrayVector element failed.";
        throw OmniException("HashAgg SERIALIZED FAILED : ", message);
    }

    for (int64_t i = start; i < end; i++) {
        serializer(elementVec, static_cast<int32_t>(i), arenaAllocator, result);
    }
}

void ALWAYS_INLINE RowVectorSerializer(RowVector &rowVector, int32_t rowIdx, mem::SimpleArenaAllocator
    &arenaAllocator, StringRef &result) {
    int32_t childCount = rowVector.ChildSize();

    // Serialize field count
    uint8_t countLenSize = GetCompactLengthSize(static_cast<uint64_t>(childCount));
    auto *&dataRef = result.data;
    auto pos = arenaAllocator.AllocateContinue(sizeof(uint8_t) + countLenSize, reinterpret_cast<const uint8_t *&>(dataRef));
    *pos = countLenSize;
    memcpy(pos + 1, &childCount, countLenSize);
    result.size += sizeof(uint8_t) + countLenSize;

    // Serialize each field
    for (int32_t i = 0; i < childCount; i++) {
        auto &childVec = rowVector.ChildAt(i);
        auto childTypeId = static_cast<type::DataTypeId>(childVec->GetTypeId());

        auto serializer = SelectSerializerByEncodingAndType(childVec.get(), childTypeId);
        if (serializer == nullptr) {
            auto message = "Finding serializer for RowVector field failed, typeId=" + std::to_string(childTypeId);
            throw OmniException("SERIALIZED FAILED : ", message);
        }
        serializer(childVec.get(), rowIdx, arenaAllocator, result);
    }
}

void ALWAYS_INLINE MapVectorSerializer(MapVector &mapVector, int32_t rowIdx, mem::SimpleArenaAllocator
    &arenaAllocator, StringRef &result) {
    int64_t offset = mapVector.GetOffset(rowIdx);
    int64_t size = mapVector.GetSize(rowIdx);

    uint8_t sizeLenSize = GetCompactLengthSize(size);
    auto *&dataRef = result.data;
    auto pos = arenaAllocator.AllocateContinue(sizeof(uint8_t) + sizeLenSize, reinterpret_cast<const uint8_t *&>(dataRef));
    *pos = sizeLenSize;
    memcpy(pos + 1, &size, sizeLenSize);
    result.size += sizeof(uint8_t) + sizeLenSize;

    auto* keyVec = mapVector.GetKeyVector().get();
    auto* valueVec = mapVector.GetValueVector().get();
    int64_t start = offset;
    int64_t end = offset + size;
    auto keyTypeId = keyVec->GetTypeId();
    auto valueTypeId = valueVec->GetTypeId();

    auto keySerializer = SelectSerializerByEncodingAndType(keyVec, static_cast<type::DataTypeId>(keyTypeId));
    auto valueSerializer = SelectSerializerByEncodingAndType(valueVec, static_cast<type::DataTypeId>(valueTypeId));
    if (keySerializer == nullptr || valueSerializer == nullptr) {
        throw OmniException("HashAgg SERIALIZED FAILED : MapVector key/value serializer not found");
    }

    for (int64_t i = start; i < end; i++) {
        keySerializer(keyVec, static_cast<int32_t>(i), arenaAllocator, result);
        valueSerializer(valueVec, static_cast<int32_t>(i), arenaAllocator, result);
    }
}

inline const char *DeserializeElementByType(type::DataTypeId elementTypeId, BaseVector *elementVector, int32_t rowIdx, const char *begin) {
    if (elementTypeId < 0 || elementTypeId >= vectorDeSerializerCenter.size()) {
        throw OmniException("ArrayVector's ElementVec Deserializer failed : Invalid elementTypeId", std::to_string(elementTypeId));
    }
    auto deser = vectorDeSerializerCenter[elementTypeId];
    if (deser == nullptr) {
        auto it = complexVectorDeSerializerCenter.find(elementTypeId);
        if (it != complexVectorDeSerializerCenter.end()) {
            deser = it->second;
        } else {
            throw OmniException("ArrayVector's ElementVec Deserializer failed : Unsupport elementTypeId",
                std::to_string(elementTypeId));
        }
    }
    return deser(elementVector, rowIdx, begin);
}

const char *ArrayVectorDeserializer(BaseVector *baseVector, int32_t rowIdx, const char * begin) {
    auto arrayVector = dynamic_cast<ArrayVector *>(baseVector);
    arrayVector->Expand(rowIdx + 1);

    uint8_t sizeLenSize = *reinterpret_cast<const uint8_t *>(begin);
    begin += sizeof(uint8_t);

    if (sizeLenSize == 0) {
        arrayVector->SetNull(rowIdx);
        int64_t lastOffset = (rowIdx == 0) ? 0 : arrayVector->GetOffset(rowIdx);
        arrayVector->SetOffset(rowIdx + 1, lastOffset);
        return begin;
    }

    uint64_t size = 0;
    switch (sizeLenSize) {
        case BYTE_1:
            size = *reinterpret_cast<const uint8_t *>(begin);
            break;
        case BYTE_2:
            size = *reinterpret_cast<const uint16_t *>(begin);
            break;
        case BYTE_4:
            size = *reinterpret_cast<const uint32_t *>(begin);
            break;
        default:
            throw OmniException("ArrayVector Deserializer failed: ", "Invalid Array Size");
    }
    begin += sizeLenSize;
    arrayVector->SetNotNull(rowIdx);
    auto elementVecShared = arrayVector->GetElementVector();
    BaseVector *elementVec = elementVecShared.get();
    int64_t start = arrayVector->GetOffset(rowIdx);
    int64_t end = start + size;

    type::DataTypeId elementTypeId = elementVec->GetTypeId();
    elementVec->Expand(end);

    if (rowIdx == 0) {
        arrayVector->SetOffset(0, 0);
    }
    arrayVector->SetOffset(rowIdx + 1, end);

    for (int64_t i = start; i < end; i++) {
        begin = DeserializeElementByType(elementTypeId, elementVec, static_cast<int32_t>(i), begin);
    }

    return begin;
}

const char *MapVectorDeserializer(BaseVector *baseVector, int32_t rowIdx, const char *&begin) {
    auto mapVector = dynamic_cast<MapVector *>(baseVector);
    mapVector->Expand(rowIdx + 1);

    uint8_t sizeLenSize = *reinterpret_cast<const uint8_t *>(begin);
    begin += sizeof(uint8_t);

    if (sizeLenSize == 0) {
        mapVector->SetNull(rowIdx);
        int64_t lastOffset = (rowIdx == 0) ? 0 : mapVector->GetOffset(rowIdx);
        mapVector->SetOffset(rowIdx + 1, lastOffset);
        return begin;
    }

    uint64_t size = 0;
    switch (sizeLenSize) {
        case BYTE_1: size = *reinterpret_cast<const uint8_t *>(begin); break;
        case BYTE_2: size = *reinterpret_cast<const uint16_t *>(begin); break;
        case BYTE_4: size = *reinterpret_cast<const uint32_t *>(begin); break;
        default:
            throw OmniException("MapVector Deserializer failed: ", "Invalid Map Size");
    }
    begin += sizeLenSize;

    mapVector->SetNotNull(rowIdx);
    auto* keyVec = mapVector->GetKeyVector().get();
    auto* valueVec = mapVector->GetValueVector().get();
    int64_t start = mapVector->GetOffset(rowIdx);
    int64_t end = start + size;

    keyVec->Expand(end);
    valueVec->Expand(end);

    if (rowIdx == 0) {
        mapVector->SetOffset(0, 0);
    }
    mapVector->SetOffset(rowIdx + 1, end);

    type::DataTypeId keyTypeId = keyVec->GetTypeId();
    type::DataTypeId valueTypeId = valueVec->GetTypeId();
    for (int64_t i = start; i < end; i++) {
        begin = DeserializeElementByType(keyTypeId, keyVec, static_cast<int32_t>(i), begin);
        begin = DeserializeElementByType(valueTypeId, valueVec, static_cast<int32_t>(i), begin);
    }

    return begin;
}

bool ArrayVectorComparator(BaseVector &baseVector, int32_t rowIdx, uint8_t *&begin) {
    auto arrayVector = dynamic_cast<ArrayVector *>(&baseVector);
    if (UNLIKELY(arrayVector == nullptr)) {
        throw OmniException("ArrayVectorComparator failed: baseVector is not ArrayVector");
    }
    uint8_t sizeLenSize = *reinterpret_cast<const uint8_t *>(begin);
    begin += sizeof(uint8_t);
    int64_t leftOffset = arrayVector->GetOffset(rowIdx);
    int64_t leftSize = arrayVector->GetSize(rowIdx);

    if (sizeLenSize == 0) {
        return leftSize == 0;
    }

    int64_t size = 0;
    switch (sizeLenSize) {
        case BYTE_1:
            size = *reinterpret_cast<const uint8_t *>(begin);
            break;
        case BYTE_2:
            size = *reinterpret_cast<const uint16_t *>(begin);
            break;
        case BYTE_4:
            size = *reinterpret_cast<const uint32_t *>(begin);
            break;
        default:
            throw OmniException("ArrayVector Deserializer failed: ", "Invalid Array Size");
    }
    begin += sizeLenSize;
    if (size != leftSize) {
        return false;
    }
    auto elementVecShared = arrayVector->GetElementVector();
    BaseVector *elementVec = elementVecShared.get();
    int64_t start = leftOffset;
    int64_t end = start + size;

    type::DataTypeId elementTypeId = elementVec->GetTypeId();

    for (int64_t i = start; i < end; i++) {
        if (elementTypeId < 0 || elementTypeId >= vectorDeSerializerCenter.size()) {
            throw OmniException("ArrayVector's ElementVec Deserializer failed : Invalid elementTypeId", std::to_string(elementTypeId));
        }
        auto comparator = vectorComparatorCenter[elementTypeId];
        if (comparator == nullptr) {
            throw OmniException("ArrayVector's ElementVec Deserializer failed : Unsupport elementTypeId", std::to_string(elementTypeId));
        }
        if (!comparator(*elementVec, i, begin)) {
            return false;
        }
    }

    return true;
}

const char *RowVectorDeserializer(BaseVector *baseVector, int32_t rowIdx, const char *begin) {
    auto rowVector = dynamic_cast<RowVector *>(baseVector);
    rowVector->Expand(rowIdx + 1);

    uint8_t countLenSize = *reinterpret_cast<const uint8_t *>(begin);
    begin += sizeof(uint8_t);

    if (countLenSize == 0) {
        rowVector->SetNull(rowIdx);
        return begin;
    }

    int32_t childCount = 0;
    switch (countLenSize) {
        case BYTE_1:
            childCount = *reinterpret_cast<const int8_t *>(begin);
            break;
        case BYTE_2:
            childCount = *reinterpret_cast<const int16_t *>(begin);
            break;
        case BYTE_4:
            childCount = *reinterpret_cast<const int32_t *>(begin);
            break;
        default:
            throw OmniException("RowVector Deserializer failed: ", "Invalid Field Count");
    }
    begin += countLenSize;

    OMNI_CHECK(childCount == rowVector->ChildSize(), "RowVector field count mismatch in deserialization");

    rowVector->SetNotNull(rowIdx);

    // Deserialize each field
    for (int32_t i = 0; i < childCount; i++) {
        auto &childVec = rowVector->ChildAt(i);
        auto childTypeId = static_cast<type::DataTypeId>(childVec->GetTypeId());

        if (childTypeId < 0 || childTypeId >= vectorDeSerializerCenter.size()) {
            throw OmniException("RowVector's field Deserializer failed : Invalid childTypeId", std::to_string(childTypeId));
        }

        auto deser = vectorDeSerializerCenter[childTypeId];
        if (deser == nullptr) {
            // Try complex type deserializers
            auto it = complexVectorDeSerializerCenter.find(childTypeId);
            if (it != complexVectorDeSerializerCenter.end()) {
                deser = it->second;
            } else {
                throw OmniException("RowVector's field Deserializer failed : Unsupport childTypeId", std::to_string(childTypeId));
            }
        }
        begin = deser(childVec.get(), rowIdx, begin);
    }

    return begin;
}

bool RowVectorComparator(BaseVector &baseVector, int32_t rowIdx, uint8_t *&begin) {
    auto rowVector = dynamic_cast<RowVector *>(&baseVector);
    if (UNLIKELY(rowVector == nullptr)) {
        throw OmniException("RowVectorComparator failed: baseVector is not RowVector");
    }
    rowVector->Expand(rowIdx + 1);

    uint8_t countLenSize = *begin;
    begin += sizeof(uint8_t);
    int32_t childNum = rowVector->ChildSize();

    if (countLenSize == 0) {
        return baseVector.IsNull(rowIdx);
    }

    int32_t childCount = 0;
    switch (countLenSize) {
        case BYTE_1:
            childCount = *reinterpret_cast<const int8_t *>(begin);
            break;
        case BYTE_2:
            childCount = *reinterpret_cast<const int16_t *>(begin);
            break;
        case BYTE_4:
            childCount = *reinterpret_cast<const int32_t *>(begin);
            break;
        default:
            throw OmniException("RowVector Deserializer failed: ", "Invalid Field Count");
    }
    if (childCount != childNum) {
        return false;
    }
    begin += countLenSize;

    // Compare each field
    for (int32_t i = 0; i < childCount; i++) {
        auto &childVec = rowVector->ChildAt(i);
        auto childTypeId = childVec->GetTypeId();

        if (childTypeId < 0 || childTypeId >= vectorComparatorCenter.size()) {
            throw OmniException("RowVector's field Comparator failed : Invalid childTypeId", std::to_string(childTypeId));
        }

        auto comparator = vectorComparatorCenter[childTypeId];
        if (comparator == nullptr) {
            throw OmniException("RowVector's field Comparator failed : Unsupport childTypeId", std::to_string(childTypeId));
        }
        if (!comparator(*childVec, rowIdx, begin)) {
            return false;
        }
    }

    return true;
}

const bool MapVectorComparator(BaseVector &baseVector, int32_t rowIdx, uint8_t *&begin) {
    auto mapVector = dynamic_cast<MapVector *>(&baseVector);
    uint8_t sizeLenSize = *reinterpret_cast<const uint8_t *>(begin);
    begin += sizeof(uint8_t);
    int64_t leftOffset = mapVector->GetOffset(rowIdx);
    int64_t leftSize = mapVector->GetSize(rowIdx);

    if (sizeLenSize == 0) {
        return leftSize == 0;
    }

    int64_t size = 0;
    switch (sizeLenSize) {
        case BYTE_1: size = *reinterpret_cast<const uint8_t *>(begin); break;
        case BYTE_2: size = *reinterpret_cast<const uint16_t *>(begin); break;
        case BYTE_4: size = *reinterpret_cast<const uint32_t *>(begin); break;
        default:
            throw OmniException("MapVector Comparator failed: ", "Invalid Map Size");
    }
    begin += sizeLenSize;
    if (size != leftSize) {
        return false;
    }

    auto* keyVec = mapVector->GetKeyVector().get();
    auto* valueVec = mapVector->GetValueVector().get();
    int64_t start = leftOffset;
    int64_t end = start + size;

    auto keyTypeId = keyVec->GetTypeId();
    auto valueTypeId = valueVec->GetTypeId();

    for (int64_t i = start; i < end; i++) {
        auto keyComparator = vectorComparatorCenter[keyTypeId];
        if (keyComparator == nullptr || !keyComparator(*keyVec, static_cast<int32_t>(i), begin)) {
            return false;
        }
        auto valueComparator = vectorComparatorCenter[valueTypeId];
        if (valueComparator == nullptr || !valueComparator(*valueVec, static_cast<int32_t>(i), begin)) {
            return false;
        }
    }

    return true;
}

void MapVectorSerializerFunc(BaseVector *baseVector, int32_t rowIdx,
                              mem::SimpleArenaAllocator &arenaAllocator, StringRef &result) {
    if (baseVector->IsNull(rowIdx)) {
        NullMapVectorSerializer(arenaAllocator, result);
        return;
    }
    auto *mapVector = dynamic_cast<MapVector *>(baseVector);
    MapVectorSerializer(*mapVector, rowIdx, arenaAllocator, result);
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
        case BYTE_1:
            value = static_cast<T>(*reinterpret_cast<const int8_t *>(pos));
            break;
        case BYTE_2:
            value = static_cast<T>(*reinterpret_cast<const int16_t *>(pos));
            break;
        case BYTE_4:
            if constexpr (std::is_same_v<T, float>) {
                value = *reinterpret_cast<const float *>(pos);
            } else {
                value = static_cast<T>(*reinterpret_cast<const int32_t *>(pos));
            }
            break;
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

template <typename T> const bool DoFixedLenTypeCompare(T right, uint8_t *&pos)
{
    auto rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    if (rowDataSize == 0) {
        return false;
    }
    auto left = GetValue<T>(rowDataSize, pos + sizeof(uint8_t));
    pos += sizeof(uint8_t) + rowDataSize;
    return left == right;
}

template <DataTypeId id> const bool FixedLenTypeComparator(BaseVector &baseVector, size_t rowIdx, uint8_t *&pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    using RealVector = typename NativeAndVectorType<id>::vector;
    auto realVector = static_cast<RealVector *>(&baseVector);
    auto rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    if (rowDataSize != 0) {
        RawDataType right = realVector->GetValue(rowIdx);
        return DoFixedLenTypeCompare<RawDataType>(right, pos);
    } else {
        pos += sizeof(uint8_t);
        return baseVector.IsNull(rowIdx);
    }
}

void Decimal128Serializer(Decimal128 &value, mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    uint8_t rowDataSize = BYTE_16;
    auto resSize = sizeof(uint8_t) + rowDataSize;
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = rowDataSize;
    *reinterpret_cast<Decimal128 *>((pos + sizeof(uint8_t))) = value;
    result.size += resSize;
}

template <typename RawDataType>
void FixedLenTypeSerializer(RawDataType value, mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    uint8_t rowDataSize;
    if constexpr (std::is_same_v<RawDataType, double>) {
        rowDataSize = BYTE_8;
    } else if constexpr (std::is_same_v<RawDataType, bool>) {
        rowDataSize = BYTE_1;
    } else if constexpr (std::is_same_v<RawDataType, float>) {
        rowDataSize = BYTE_4;
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

    auto resSize = sizeof(int8_t) + rowDataSize;
    auto *&data = result.data;

    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = rowDataSize;
    // copy value
    memcpy(pos + sizeof(uint8_t), &value, rowDataSize);
    result.size += resSize;
}

void NullDecimal128Serializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0;
    result.size += resSize;
}

void NullFixedLenTypeSerializer(mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    auto resSize = sizeof(uint8_t);
    auto *&data = result.data;
    auto *pos = arenaAllocator.AllocateContinue(resSize, reinterpret_cast<const uint8_t *&>(data));
    (*pos) = 0; // value is null
    result.size += resSize;
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

        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            auto value = rawVector->GetValue(rowIdx);
            VariableTypeSerializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            auto value = rawVector->GetValue(rowIdx);
            Decimal128Serializer(value, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, ArrayType>) {
            auto *arrayVector = dynamic_cast<ArrayVector *>(baseVector);
            ArrayVectorSerializer(*arrayVector, rowIdx, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, RowType>) {
            // OMNI_ROW - Struct type
            auto *rowVector = dynamic_cast<RowVector *>(baseVector);
            RowVectorSerializer(*rowVector, rowIdx, arenaAllocator, result);
        } else {
            auto value = rawVector->GetValue(rowIdx);
            FixedLenTypeSerializer<RawDataType>(value, arenaAllocator, result);
        }
    } else {
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            NullVariableTypeSerializer(arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            NullDecimal128Serializer(arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, ArrayType>) {
            NullArrayVectorSerializer(arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, RowType>) {
            // OMNI_ROW - Struct type null
            NullRowVectorSerializer(arenaAllocator, result);
        } else {
            NullFixedLenTypeSerializer(arenaAllocator, result);
        }
    }
}

template <type::DataTypeId id>
void SerializeConstValueIntoArena(BaseVector *baseVector, int32_t rowIdx,
    mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        auto constVector = static_cast<ConstVector<RawDataType> *>(baseVector);
        auto value = constVector->GetConstValue();

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
    } else if constexpr (std::is_same_v<RawDataType, ArrayType>) {
        return ArrayVectorDeserializer(baseVector, rowIdx, begin);
    } else if constexpr (std::is_same_v<RawDataType, RowType>) {
        // OMNI_ROW - Struct type
        return RowVectorDeserializer(baseVector, rowIdx, begin);
    } else {
        return FixedLenTypeDeserializer<id>(baseVector, rowIdx, begin);
    }
}

template <type::DataTypeId id>
const bool DictionaryComparator(BaseVector &baseVector, int32_t rowIdx, uint8_t *&pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    if (!baseVector.IsNull(rowIdx)) {
        auto dictionaryVector = static_cast<Vector<DictionaryContainer<RawDataType>> *>(&baseVector);

        auto value = dictionaryVector->GetValue(rowIdx);
        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            return DoVariableTypeCompare(value, pos);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            // Decimal128Serializer(value, arenaAllocator, result);
        } else {
            return DoFixedLenTypeCompare<RawDataType>(value, pos);
        }
    }
    auto rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    pos += sizeof(uint8_t);
    return rowDataSize == 0;
}

template <type::DataTypeId id>
const bool ConstComparator(BaseVector &baseVector, int32_t rowIdx, uint8_t *&pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    if (!baseVector.IsNull(rowIdx)) {
        auto constVector = static_cast<ConstVector<RawDataType> *>(&baseVector);
        auto value = constVector->GetConstValue();
        // the analysis of const expr  will be in compile stage
        if constexpr (std::is_same_v<RawDataType, std::string_view>) {
            return DoVariableTypeCompare(value, pos);
        } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
            //Decimal128Serializer(value, arenaAllocator, result);
        } else {
            return DoFixedLenTypeCompare<RawDataType>(value, pos);
        }
    }
    auto rowDataSize = *(reinterpret_cast<const uint8_t *>(pos));
    pos += sizeof(uint8_t);
    return rowDataSize == 0;
}

template <type::DataTypeId id>
const bool ComparatorFromPointer(BaseVector &baseVector, int32_t rowIdx, uint8_t *&row)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    // the analysis of const expr  will be in compile stage
    if constexpr (std::is_same_v<RawDataType, std::string_view>) {
        return VariableTypeComparator<id>(baseVector, rowIdx, row);
    } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
        //return Decimal128Deserializer(baseVector, rowIdx, begin);
    } else if constexpr (std::is_same_v<RawDataType, ArrayType>) {
        return ArrayVectorComparator(baseVector, rowIdx, row);
    } else if constexpr (std::is_same_v<RawDataType, RowType>) {
        // OMNI_ROW - Struct type
        return RowVectorComparator(baseVector, rowIdx, row);
    } else {
        return FixedLenTypeComparator<id>(baseVector, rowIdx, row);
    }
}

std::unordered_map<DataTypeId, SerializerFunc> complexVectorSerializerCenter = {
        {type::OMNI_ARRAY, &SerializeValueIntoArena<type::OMNI_ARRAY>},
        {type::OMNI_ROW, &SerializeValueIntoArena<type::OMNI_ROW>},
        {type::OMNI_MAP, &MapVectorSerializerFunc}
};

std::unordered_map<DataTypeId, DeSerializerFunc> complexVectorDeSerializerCenter = {
        {type::OMNI_ARRAY, &DeserializeFromPointer<type::OMNI_ARRAY>},
        {type::OMNI_ROW, &DeserializeFromPointer<type::OMNI_ROW>},
        {type::OMNI_MAP, &MapVectorDeserializer}
};

std::vector<VectorSerializer> vectorSerializerCenter = {
    nullptr,                                        // OMNI_NONE, 0
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeValueIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeValueIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeValueIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeValueIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeValueIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeValueIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                        // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                        // OMNI_INTERVAL_DAY_TIME, 14
    SerializeValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    SerializeValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                        // OMNI_CONTAINER, 17
    SerializeValueIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeValueIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    SerializeValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                        // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                        // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                        // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                        // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                        // OMNI_MULTISET, 25
    nullptr,                                        // 26
    nullptr,                                        // 27
    nullptr,                                        // 28
    nullptr,                                        // 29
    SerializeValueIntoArena<type::OMNI_ARRAY>,      // OMNI_ARRAY, 30
    MapVectorSerializerFunc,                        // OMNI_MAP, 31
    SerializeValueIntoArena<type::OMNI_ROW>,        // OMNI_ROW, 32
    nullptr,                                        // OMNI_UNKNOWN, 33
    nullptr,                                        // OMNI_FUNCTION, 34
    nullptr,                                        // OMNI_OPAQUE, 35
    nullptr                                         // OMNI_INVALID
};

std::vector<VectorSerializer> dicVectorSerializerCenter = {
    nullptr,                                                  // OMNI_NONE, 0
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeDictionaryValueIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeDictionaryValueIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeDictionaryValueIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeDictionaryValueIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeDictionaryValueIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeDictionaryValueIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                  // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                  // OMNI_INTERVAL_DAY_TIME, 14
    SerializeDictionaryValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    SerializeDictionaryValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                                  // OMNI_CONTAINER, 17
    SerializeDictionaryValueIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeDictionaryValueIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    SerializeDictionaryValueIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                  // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                  // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                  // OMNI_MULTISET, 25
    nullptr,                                                  // 26
    nullptr,                                                  // 27
    nullptr,                                                  // 28
    nullptr,                                                  // 29
    nullptr,                                                  // OMNI_ARRAY, 30
    nullptr,                                                  // OMNI_MAP, 31
    nullptr,                                                  // OMNI_ROW, 32
    nullptr,                                                  // OMNI_UNKNOWN, 33
    nullptr,                                                  // OMNI_FUNCTION, 34
    nullptr,                                                  // OMNI_OPAQUE, 35
    nullptr                                                   // OMNI_INVALID
};

std::vector<VectorComparator> dicVectorComparatorCenter = {
    nullptr,                                                  // OMNI_NONE, 0
    DictionaryComparator<type::OMNI_INT>,                     // OMNI_INT, 1
    DictionaryComparator<type::OMNI_LONG>,                    // OMNI_LONG, 2
    DictionaryComparator<type::OMNI_DOUBLE>,                  // OMNI_DOUBLE, 3
    DictionaryComparator<type::OMNI_BOOLEAN>,                 // OMNI_BOOLEAN, 4
    DictionaryComparator<type::OMNI_SHORT>,                   // OMNI_SHORT, 5
    DictionaryComparator<type::OMNI_LONG>,                    // OMNI_DECIMAL64, 6
    DictionaryComparator<type::OMNI_DECIMAL128>,              // OMNI_DECIMAL128, 7
    DictionaryComparator<type::OMNI_INT>,                     // OMNI_DATE32, 8
    DictionaryComparator<type::OMNI_LONG>,                    // OMNI_DATE64, 9
    DictionaryComparator<type::OMNI_INT>,                     // OMNI_TIME32, 10
    DictionaryComparator<type::OMNI_LONG>,                    // OMNI_TIME64, 11
    DictionaryComparator<type::OMNI_LONG>,                    // OMNI_TIMESTAMP, 12
    nullptr,                                                               // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                               // OMNI_INTERVAL_DAY_TIME, 14
    DictionaryComparator<type::OMNI_VARCHAR>,                 // OMNI_VARCHAR, 15
    DictionaryComparator<type::OMNI_VARCHAR>,                 // OMNI_CHAR, 16
    nullptr,                                                               // OMNI_CONTAINER, 17
    DictionaryComparator<type::OMNI_BYTE>,                    // OMNI_BYTE, 18
    DictionaryComparator<type::OMNI_FLOAT>,                   // OMNI_FLOAT, 19
    DictionaryComparator<type::OMNI_VARCHAR>,                 // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                  // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                  // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                  // OMNI_MULTISET, 25
    nullptr,                                                  // 26
    nullptr,                                                  // 27
    nullptr,                                                  // 28
    nullptr,                                                  // 29
    nullptr,                                                  // OMNI_ARRAY, 30
    nullptr,                                                  // OMNI_MAP, 31
    nullptr,                                                  // OMNI_ROW, 32
    nullptr,                                                  // OMNI_UNKNOWN, 33
    nullptr,                                                  // OMNI_FUNCTION, 34
    nullptr,                                                  // OMNI_OPAQUE, 35
    nullptr                                                   // OMNI_INVALID
};

std::vector<VectorSerializer> constVectorSerializerCenter = {
    nullptr,                                                 // OMNI_NONE, 0
    SerializeConstValueIntoArena<type::OMNI_INT>,            // OMNI_INT, 1
    SerializeConstValueIntoArena<type::OMNI_LONG>,           // OMNI_LONG, 2
    SerializeConstValueIntoArena<type::OMNI_DOUBLE>,         // OMNI_DOUBLE, 3
    SerializeConstValueIntoArena<type::OMNI_BOOLEAN>,        // OMNI_BOOLEAN, 4
    SerializeConstValueIntoArena<type::OMNI_SHORT>,          // OMNI_SHORT, 5
    SerializeConstValueIntoArena<type::OMNI_LONG>,           // OMNI_DECIMAL64, 6
    SerializeConstValueIntoArena<type::OMNI_DECIMAL128>,     // OMNI_DECIMAL128, 7
    SerializeConstValueIntoArena<type::OMNI_INT>,            // OMNI_DATE32, 8
    SerializeConstValueIntoArena<type::OMNI_LONG>,           // OMNI_DATE64, 9
    SerializeConstValueIntoArena<type::OMNI_INT>,            // OMNI_TIME32, 10
    SerializeConstValueIntoArena<type::OMNI_LONG>,           // OMNI_TIME64, 11
    SerializeConstValueIntoArena<type::OMNI_LONG>,           // OMNI_TIMESTAMP, 12
    nullptr,                                                 // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                 // OMNI_INTERVAL_DAY_TIME, 14
    SerializeConstValueIntoArena<type::OMNI_VARCHAR>,        // OMNI_VARCHAR, 15
    SerializeConstValueIntoArena<type::OMNI_VARCHAR>,        // OMNI_CHAR, 16
    nullptr,                                                 // OMNI_CONTAINER, 17
    SerializeConstValueIntoArena<type::OMNI_BYTE>,           // OMNI_BYTE, 18
    SerializeConstValueIntoArena<type::OMNI_FLOAT>,          // OMNI_FLOAT, 19
    SerializeConstValueIntoArena<type::OMNI_VARCHAR>,       // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                 // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                 // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                 // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                 // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                 // OMNI_MULTISET, 25
    nullptr,                                                 // 26
    nullptr,                                                 // 27
    nullptr,                                                 // 28
    nullptr,                                                 // 29
    nullptr,                                                 // OMNI_ARRAY, 30
    nullptr,                                                 // OMNI_MAP, 31
    nullptr,                                                 // OMNI_ROW, 32
    nullptr,                                                 // OMNI_UNKNOWN, 33
    nullptr,                                                 // OMNI_FUNCTION, 34
    nullptr,                                                 // OMNI_OPAQUE, 35
    nullptr                                                  // OMNI_INVALID
};

std::vector<VectorComparator> constVectorComparatorCenter = {
    nullptr,                                                 // OMNI_NONE, 0
    ConstComparator<type::OMNI_INT>,            // OMNI_INT, 1
    ConstComparator<type::OMNI_LONG>,           // OMNI_LONG, 2
    ConstComparator<type::OMNI_DOUBLE>,         // OMNI_DOUBLE, 3
    ConstComparator<type::OMNI_BOOLEAN>,        // OMNI_BOOLEAN, 4
    ConstComparator<type::OMNI_SHORT>,          // OMNI_SHORT, 5
    ConstComparator<type::OMNI_LONG>,           // OMNI_DECIMAL64, 6
    ConstComparator<type::OMNI_DECIMAL128>,     // OMNI_DECIMAL128, 7
    ConstComparator<type::OMNI_INT>,            // OMNI_DATE32, 8
    ConstComparator<type::OMNI_LONG>,           // OMNI_DATE64, 9
    ConstComparator<type::OMNI_INT>,            // OMNI_TIME32, 10
    ConstComparator<type::OMNI_LONG>,           // OMNI_TIME64, 11
    ConstComparator<type::OMNI_LONG>,           // OMNI_TIMESTAMP, 12
    nullptr,                                                 // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                 // OMNI_INTERVAL_DAY_TIME, 14
    ConstComparator<type::OMNI_VARCHAR>,        // OMNI_VARCHAR, 15
    ConstComparator<type::OMNI_VARCHAR>,        // OMNI_CHAR, 16
    nullptr,                                                 // OMNI_CONTAINER, 17
    ConstComparator<type::OMNI_BYTE>,           // OMNI_BYTE, 18
    ConstComparator<type::OMNI_FLOAT>,          // OMNI_FLOAT, 19
    ConstComparator<type::OMNI_VARCHAR>,       // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                 // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                 // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                 // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                 // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                 // OMNI_MULTISET, 25
    nullptr,                                                 // 26
    nullptr,                                                 // 27
    nullptr,                                                 // 28
    nullptr,                                                 // 29
    nullptr,                                                 // OMNI_ARRAY, 30
    nullptr,                                                 // OMNI_MAP, 31
    nullptr,                                                 // OMNI_ROW, 32
    nullptr,                                                 // OMNI_UNKNOWN, 33
    nullptr,                                                 // OMNI_FUNCTION, 34
    nullptr,                                                 // OMNI_OPAQUE, 35
    nullptr                                                  // OMNI_INVALID
};

std::vector<VectorDeSerializer> vectorDeSerializerCenter = {
    nullptr,                                       // OMNI_NONE, 0
    DeserializeFromPointer<type::OMNI_INT>,        // OMNI_INT, 1
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_LONG, 2
    DeserializeFromPointer<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    DeserializeFromPointer<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    DeserializeFromPointer<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    DeserializeFromPointer<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    DeserializeFromPointer<type::OMNI_INT>,        // OMNI_DATE32, 8
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_DATE64, 9
    DeserializeFromPointer<type::OMNI_INT>,        // OMNI_TIME32, 10
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_TIME64, 11
    DeserializeFromPointer<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                       // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                       // OMNI_INTERVAL_DAY_TIME, 14
    DeserializeFromPointer<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    DeserializeFromPointer<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                       // OMNI_CONTAINER, 17
    DeserializeFromPointer<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    DeserializeFromPointer<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    DeserializeFromPointer<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                       // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                       // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                       // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                       // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                       // OMNI_MULTISET, 25
    nullptr,                                       // 26
    nullptr,                                       // 27
    nullptr,                                       // 28
    nullptr,                                       // 29
    nullptr,                                       // OMNI_ARRAY, 30
    MapVectorDeserializer,                          // OMNI_MAP, 31
    nullptr,                                       // OMNI_ROW, 32
    nullptr,                                       // OMNI_UNKNOWN, 33
    nullptr,                                       // OMNI_FUNCTION, 34
    nullptr,                                       // OMNI_OPAQUE, 35
    nullptr                                        // OMNI_INVALID
};

std::vector<VectorComparator> vectorComparatorCenter = {
    nullptr,                                       // OMNI_NONE, 0
    ComparatorFromPointer<type::OMNI_INT>,        // OMNI_INT, 1
    ComparatorFromPointer<type::OMNI_LONG>,       // OMNI_LONG, 2
    ComparatorFromPointer<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    ComparatorFromPointer<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    ComparatorFromPointer<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    ComparatorFromPointer<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    ComparatorFromPointer<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    ComparatorFromPointer<type::OMNI_INT>,        // OMNI_DATE32, 8
    ComparatorFromPointer<type::OMNI_LONG>,       // OMNI_DATE64, 9
    ComparatorFromPointer<type::OMNI_INT>,        // OMNI_TIME32, 10
    ComparatorFromPointer<type::OMNI_LONG>,       // OMNI_TIME64, 11
    ComparatorFromPointer<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                       // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                       // OMNI_INTERVAL_DAY_TIME, 14
    ComparatorFromPointer<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    ComparatorFromPointer<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                       // OMNI_CONTAINER, 17
    ComparatorFromPointer<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    ComparatorFromPointer<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    ComparatorFromPointer<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                       // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                       // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                       // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                       // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                       // OMNI_MULTISET, 25
    nullptr,                                       // 26
    nullptr,                                       // 27
    nullptr,                                       // 28
    nullptr,                                       // 29
    ComparatorFromPointer<type::OMNI_ARRAY>,       // OMNI_ARRAY, 30
    MapVectorComparator,                            // OMNI_MAP, 31
    ComparatorFromPointer<type::OMNI_ROW>,         // OMNI_ROW, 32
    nullptr,                                       // OMNI_UNKNOWN, 33
    nullptr,                                       // OMNI_FUNCTION, 34
    nullptr,                                       // OMNI_OPAQUE, 35
    nullptr                                        // OMNI_INVALID
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
        } else if constexpr (std::is_same_v<RawDataType, ArrayType>) {
            auto *arrayVector = dynamic_cast<ArrayVector *>(baseVector);
            ArrayVectorSerializer(*arrayVector, rowIdx, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, RowType>) {
            auto *rowVector = dynamic_cast<RowVector *>(baseVector);
            if (UNLIKELY(rowVector == nullptr)) {
                throw OmniException("SERIALIZED FAILED : ", "Expected RowVector for OMNI_ROW join key");
            }
            RowVectorSerializer(*rowVector, rowIdx, arenaAllocator, result);
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
        if constexpr (std::is_same_v<RawDataType, ArrayType>) {
            auto *arrayVector = dynamic_cast<ArrayVector *>(baseVector);
            ArrayVectorSerializer(*arrayVector, rowIdx, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, RowType>) {
            auto *rowVector = dynamic_cast<RowVector *>(baseVector);
            if (UNLIKELY(rowVector == nullptr)) {
                throw OmniException("SERIALIZED FAILED : ", "Expected RowVector for OMNI_ROW dictionary join key");
            }
            RowVectorSerializer(*rowVector, rowIdx, arenaAllocator, result);
        } else {
            auto dictionaryVector = reinterpret_cast<Vector<DictionaryContainer<RawDataType>> *>(baseVector);
            auto value = dictionaryVector->GetValue(rowIdx);
            if constexpr (std::is_same_v<RawDataType, std::string_view>) {
                VariableTypeSerializer(value, arenaAllocator, result);
            } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
                Decimal128SerializerForJoin(value, arenaAllocator, result);
            } else {
                FixedLenTypeSerializerForJoin<RawDataType>(value, arenaAllocator, result);
            }
        }
        return true;
    }

    return false;
}

template <type::DataTypeId id>
bool SerializeConstValueIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx,
    mem::SimpleArenaAllocator &arenaAllocator, StringRef &result)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        if constexpr (std::is_same_v<RawDataType, ArrayType>) {
            auto *arrayVector = dynamic_cast<ArrayVector *>(baseVector);
            ArrayVectorSerializer(*arrayVector, rowIdx, arenaAllocator, result);
        } else if constexpr (std::is_same_v<RawDataType, RowType>) {
            auto *rowVector = dynamic_cast<RowVector *>(baseVector);
            if (UNLIKELY(rowVector == nullptr)) {
                throw OmniException("SERIALIZED FAILED : ", "Expected RowVector for OMNI_ROW const join key");
            }
            RowVectorSerializer(*rowVector, rowIdx, arenaAllocator, result);
        } else {
            auto constVector = static_cast<ConstVector<RawDataType> *>(baseVector);
            auto value = constVector->GetConstValue();
            if constexpr (std::is_same_v<RawDataType, std::string_view>) {
                VariableTypeSerializer(value, arenaAllocator, result);
            } else if constexpr (std::is_same_v<RawDataType, Decimal128>) {
                Decimal128SerializerForJoin(value, arenaAllocator, result);
            } else {
                FixedLenTypeSerializerForJoin<RawDataType>(value, arenaAllocator, result);
            }
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
    nullptr,                                                  // OMNI_NONE, 0
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeValueIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeValueIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeValueIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeValueIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                  // OMNI_INTERVAL_MONTHS
    nullptr,                                                  // OMNI_INTERVAL_DAY_TI
    SerializeValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    SerializeValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                                  // OMNI_CONTAINER, 17
    SerializeValueIgnoreNullIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeValueIgnoreNullIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    SerializeValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                  // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                  // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                  // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                  // OMNI_MULTISET, 25
    nullptr,                                                  // 26
    nullptr,                                                  // 27
    nullptr,                                                  // 28
    nullptr,                                                  // 29
    SerializeValueIgnoreNullIntoArena<type::OMNI_ARRAY>,      // OMNI_ARRAY, 30
    nullptr,                                                  // OMNI_MAP, 31
    SerializeValueIgnoreNullIntoArena<type::OMNI_ROW>,        // OMNI_ROW, 32
    nullptr,                                                  // OMNI_UNKNOWN, 33
    nullptr,                                                  // OMNI_FUNCTION, 34
    nullptr,                                                  // OMNI_OPAQUE, 35
    nullptr                                                   // OMNI_INVALID
};

std::vector<VectorSerializerIgnoreNull> dicVectorSerializerIgnoreNullCenter = {
    nullptr,                                                            // OMNI_NONE, 0
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                            // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                            // OMNI_INTERVAL_DAY_TIME, 14
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARCHAR, 15
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_CHAR, 16
    nullptr,                                                            // OMNI_CONTAINER, 17
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,    // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                            // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                            // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                            // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                            // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                            // OMNI_MULTISET, 25
    nullptr,                                                            // 26
    nullptr,                                                            // 27
    nullptr,                                                            // 28
    nullptr,                                                            // 29
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_ARRAY>,      // OMNI_ARRAY, 30
    nullptr,                                                            // OMNI_MAP, 31
    SerializeDictionaryValueIgnoreNullIntoArena<type::OMNI_ROW>,        // OMNI_ROW, 32
    nullptr,                                                            // OMNI_UNKNOWN, 33
    nullptr,                                                            // OMNI_FUNCTION, 34
    nullptr,                                                            // OMNI_OPAQUE, 35
    nullptr                                                             // OMNI_INVALID
};

std::vector<VectorSerializerIgnoreNull> constVectorSerializerIgnoreNullCenter = {
    nullptr,                                                            // OMNI_NONE, 0
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_INT>,             // OMNI_INT, 1
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_LONG>,            // OMNI_LONG, 2
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_DOUBLE>,          // OMNI_DOUBLE, 3
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_BOOLEAN>,         // OMNI_BOOLEAN, 4
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_SHORT>,           // OMNI_SHORT, 5
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_LONG>,            // OMNI_DECIMAL64, 6
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_DECIMAL128>,      // OMNI_DECIMAL128, 7
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_INT>,             // OMNI_DATE32, 8
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_LONG>,            // OMNI_DATE64, 9
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_INT>,             // OMNI_TIME32, 10
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_LONG>,            // OMNI_TIME64, 11
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_LONG>,            // OMNI_TIMESTAMP, 12
    nullptr,                                                            // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                            // OMNI_INTERVAL_DAY_TIME, 14
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,         // OMNI_VARCHAR, 15
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,         // OMNI_CHAR, 16
    nullptr,                                                            // OMNI_CONTAINER, 17
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_BYTE>,            // OMNI_BYTE, 18
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_FLOAT>,           // OMNI_FLOAT, 19
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_VARCHAR>,        // OMNI_VARBINARY, 20 (same as VARCHAR)
    nullptr,                                                            // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                            // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                            // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                            // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                            // OMNI_MULTISET, 25
    nullptr,                                                            // 26
    nullptr,                                                            // 27
    nullptr,                                                            // 28
    nullptr,                                                            // 29
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_ARRAY>,           // OMNI_ARRAY, 30
    nullptr,                                                            // OMNI_MAP, 31
    SerializeConstValueIgnoreNullIntoArena<type::OMNI_ROW>,             // OMNI_ROW, 32
    nullptr,                                                            // OMNI_UNKNOWN, 33
    nullptr,                                                            // OMNI_FUNCTION, 34
    nullptr,                                                            // OMNI_OPAQUE, 35
    nullptr                                                             // OMNI_INVALID
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
bool SerializeConstFixedKeysIgnoreNullIntoArena(BaseVector *baseVector, int32_t rowIdx, StringRef &result, size_t &pos)
{
    using RawDataType = typename NativeAndVectorType<id>::type;

    if (!baseVector->IsNull(rowIdx)) {
        auto constVector = static_cast<ConstVector<RawDataType> *>(baseVector);
        auto value = constVector->GetConstValue();
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
bool SerializeFixedKeysIgnoreNullIntoArenaSimd(BaseVector *baseVector, int32_t rowid,
    std::vector<StringRef> &result, size_t &pos, int32_t joinRownum)
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
bool SerializeConstFixedKeysIgnoreNullIntoArenaSimd(BaseVector *baseVector, int32_t rowid,
    std::vector<StringRef> &result, size_t &pos, int32_t joinRownum)
{
    using RawDataType = typename NativeAndVectorType<id>::type;
    static constexpr uint8_t rawDataSize = sizeof(RawDataType);

    if (!baseVector->IsNull(rowid)) {
        auto constVector = static_cast<ConstVector<RawDataType> *>(baseVector);
        auto value = constVector->GetConstValue();
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
    std::vector<StringRef> &result, size_t &pos, int32_t joinRownum)
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
    nullptr,                                                      // OMNI_NONE, 0
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                      // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                      // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                      // OMNI_VARCHAR, 15
    nullptr,                                                      // OMNI_CHAR, 16
    nullptr,                                                      // OMNI_CONTAINER, 17
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeFixedKeysIgnoreNullIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    nullptr,                                                      // OMNI_VARBINARY, 20
    nullptr,                                                      // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                      // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                      // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                      // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                      // OMNI_MULTISET, 25
    nullptr,                                                      // 26
    nullptr,                                                      // 27
    nullptr,                                                      // 28
    nullptr,                                                      // 29
    nullptr,                                                      // OMNI_ARRAY, 30
    nullptr,                                                      // OMNI_MAP, 31
    nullptr,                                                      // OMNI_ROW, 32
    nullptr,                                                      // OMNI_UNKNOWN, 33
    nullptr,                                                      // OMNI_FUNCTION, 34
    nullptr,                                                      // OMNI_OPAQUE, 35
    nullptr                                                       // OMNI_INVALID
};

std::vector<FixedKeyVectorSerializerIgnoreNull> dicVectorSerializerFixedKeysIgnoreNullCenter = {
    nullptr,                                                                // OMNI_NONE, 0
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                                // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                                // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                                // OMNI_VARCHAR, 15
    nullptr,                                                                // OMNI_CHAR, 16
    nullptr,                                                                // OMNI_CONTAINER, 17
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeDictionaryFixedKeysIgnoreNullIntoArena<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    nullptr,                                                                // OMNI_VARBINARY, 20
    nullptr,                                                                // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                                // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                                // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                                // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                                // OMNI_MULTISET, 25
    nullptr,                                                                // 26
    nullptr,                                                                // 27
    nullptr,                                                                // 28
    nullptr,                                                                // 29
    nullptr,                                                                // OMNI_ARRAY, 30
    nullptr,                                                                // OMNI_MAP, 31
    nullptr,                                                                // OMNI_ROW, 32
    nullptr,                                                                // OMNI_UNKNOWN, 33
    nullptr,                                                                // OMNI_FUNCTION, 34
    nullptr,                                                                // OMNI_OPAQUE, 35
    nullptr                                                                 // OMNI_INVALID
};

std::vector<FixedKeyVectorSerializerIgnoreNull> constVectorSerializerFixedKeysIgnoreNullCenter = {
    nullptr,                                                              // OMNI_NONE, 0
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,           // OMNI_INT, 1
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,          // OMNI_LONG, 2
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_DOUBLE>,        // OMNI_DOUBLE, 3
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_BOOLEAN>,       // OMNI_BOOLEAN, 4
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_SHORT>,         // OMNI_SHORT, 5
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,          // OMNI_DECIMAL64, 6
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_DECIMAL128>,    // OMNI_DECIMAL128, 7
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,           // OMNI_DATE32, 8
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,          // OMNI_DATE64, 9
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_INT>,           // OMNI_TIME32, 10
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,          // OMNI_TIME64, 11
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_LONG>,          // OMNI_TIMESTAMP, 12
    nullptr,                                                              // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                              // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                              // OMNI_VARCHAR, 15
    nullptr,                                                              // OMNI_CHAR, 16
    nullptr,                                                              // OMNI_CONTAINER, 17
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_BYTE>,          // OMNI_BYTE, 18
    SerializeConstFixedKeysIgnoreNullIntoArena<type::OMNI_FLOAT>,         // OMNI_FLOAT, 19
    nullptr,                                                              // OMNI_VARBINARY, 20
    nullptr,                                                              // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                              // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                              // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                              // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                              // OMNI_MULTISET, 25
    nullptr,                                                              // 26
    nullptr,                                                              // 27
    nullptr,                                                              // 28
    nullptr,                                                              // 29
    nullptr,                                                              // OMNI_ARRAY, 30
    nullptr,                                                              // OMNI_MAP, 31
    nullptr,                                                              // OMNI_ROW, 32
    nullptr,                                                              // OMNI_UNKNOWN, 33
    nullptr,                                                              // OMNI_FUNCTION, 34
    nullptr,                                                              // OMNI_OPAQUE, 35
    nullptr                                                               // OMNI_INVALID
};

std::vector<FixedKeyVectorSerializerIgnoreNullSimd> constVectorSerializerFixedKeysIgnoreNullCenterSimd = {
    nullptr,                                                                  // OMNI_NONE, 0
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,           // OMNI_INT, 1
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,          // OMNI_LONG, 2
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DOUBLE>,        // OMNI_DOUBLE, 3
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BOOLEAN>,       // OMNI_BOOLEAN, 4
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_SHORT>,         // OMNI_SHORT, 5
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,          // OMNI_DECIMAL64, 6
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DECIMAL128>,    // OMNI_DECIMAL128, 7
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,           // OMNI_DATE32, 8
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,          // OMNI_DATE64, 9
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,           // OMNI_TIME32, 10
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,          // OMNI_TIME64, 11
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,          // OMNI_TIMESTAMP, 12
    nullptr,                                                                  // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                                  // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                                  // OMNI_VARCHAR, 15
    nullptr,                                                                  // OMNI_CHAR, 16
    nullptr,                                                                  // OMNI_CONTAINER, 17
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BYTE>,          // OMNI_BYTE, 18
    SerializeConstFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_FLOAT>,         // OMNI_FLOAT, 19
    nullptr,                                                                  // OMNI_VARBINARY, 20
    nullptr,                                                                  // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                                  // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                                  // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                                  // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                                  // OMNI_MULTISET, 25
    nullptr,                                                                  // 26
    nullptr,                                                                  // 27
    nullptr,                                                                  // 28
    nullptr,                                                                  // 29
    nullptr,                                                                  // OMNI_ARRAY, 30
    nullptr,                                                                  // OMNI_MAP, 31
    nullptr,                                                                  // OMNI_ROW, 32
    nullptr,                                                                  // OMNI_UNKNOWN, 33
    nullptr,                                                                  // OMNI_FUNCTION, 34
    nullptr,                                                                  // OMNI_OPAQUE, 35
    nullptr                                                                   // OMNI_INVALID
};

std::vector<FixedKeyVectorSerializerIgnoreNullSimd> vectorSerializerFixedKeysIgnoreNullCenterSimd = {
    nullptr,                                                          // OMNI_NONE, 0
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                          // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                          // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                          // OMNI_VARCHAR, 15
    nullptr,                                                          // OMNI_CHAR, 16
    nullptr,                                                          // OMNI_CONTAINER, 17
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    nullptr,                                                          // OMNI_VARBINARY, 20
    nullptr,                                                          // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                          // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                          // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                          // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                          // OMNI_MULTISET, 25
    nullptr,                                                          // 26
    nullptr,                                                          // 27
    nullptr,                                                          // 28
    nullptr,                                                          // 29
    nullptr,                                                          // OMNI_ARRAY, 30
    nullptr,                                                          // OMNI_MAP, 31
    nullptr,                                                          // OMNI_ROW, 32
    nullptr,                                                          // OMNI_UNKNOWN, 33
    nullptr,                                                          // OMNI_FUNCTION, 34
    nullptr,                                                          // OMNI_OPAQUE, 35
    nullptr                                                           // OMNI_INVALID
};

std::vector<FixedKeyVectorSerializerIgnoreNullSimd> dicVectorSerializerFixedKeysIgnoreNullCenterSimd = {
    nullptr,                                                                    // OMNI_NONE, 0
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_INT, 1
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_LONG, 2
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DOUBLE>,     // OMNI_DOUBLE, 3
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BOOLEAN>,    // OMNI_BOOLEAN, 4
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_SHORT>,      // OMNI_SHORT, 5
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DECIMAL64, 6
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_DECIMAL128>, // OMNI_DECIMAL128, 7
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_DATE32, 8
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_DATE64, 9
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_INT>,        // OMNI_TIME32, 10
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIME64, 11
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_LONG>,       // OMNI_TIMESTAMP, 12
    nullptr,                                                                    // OMNI_INTERVAL_MONTHS, 13
    nullptr,                                                                    // OMNI_INTERVAL_DAY_TIME, 14
    nullptr,                                                                    // OMNI_VARCHAR, 15
    nullptr,                                                                    // OMNI_CHAR, 16
    nullptr,                                                                    // OMNI_CONTAINER, 17
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_BYTE>,       // OMNI_BYTE, 18
    SerializeDictionaryFixedKeysIgnoreNullIntoArenaSimd<type::OMNI_FLOAT>,      // OMNI_FLOAT, 19
    nullptr,                                                                    // OMNI_VARBINARY, 20
    nullptr,                                                                    // OMNI_TIME_WITHOUT_TIME_ZONE, 21
    nullptr,                                                                    // OMNI_TIMESTAMP_WITHOUT_TIME_ZONE, 22
    nullptr,                                                                    // OMNI_TIMESTAMP_WITH_TIME_ZONE, 23
    nullptr,                                                                    // OMNI_TIMESTAMP_WITH_LOCAL_TIME_ZONE, 24
    nullptr,                                                                    // OMNI_MULTISET, 25
    nullptr,                                                                    // 26
    nullptr,                                                                    // 27
    nullptr,                                                                    // 28
    nullptr,                                                                    // 29
    nullptr,                                                                    // OMNI_ARRAY, 30
    nullptr,                                                                    // OMNI_MAP, 31
    nullptr,                                                                    // OMNI_ROW, 32
    nullptr,                                                                    // OMNI_UNKNOWN, 33
    nullptr,                                                                    // OMNI_FUNCTION, 34
    nullptr,                                                                    // OMNI_OPAQUE, 35
    nullptr                                                                     // OMNI_INVALID
};
}
}