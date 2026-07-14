/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "vector_helper.h"

namespace omniruntime::vec {

RowBatch *VectorHelper::TransRowBatchFromVectorBatch(VectorBatch *vecBatch)
{
    int32_t vecCount = vecBatch->GetVectorCount();
    std::vector<type::DataTypeId> typeIds;
    std::vector<Encoding> encodings;
    for (int i = 0; i < vecCount; i++) {
        typeIds.push_back(vecBatch->Get(i)->GetTypeId());
        encodings.push_back(vecBatch->Get(i)->GetEncoding());
    }

    auto rowBuffer = std::make_unique<RowBuffer>(typeIds, encodings, typeIds.size() - 1);

    auto rowBatch = new RowBatch(vecBatch->GetRowCount());
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        // 1.get value from vector batch
        rowBuffer->TransValueFromVectorBatch(vecBatch, i);

        // 2.generate one buffer of one row
        auto oneRowLen = rowBuffer->FillBuffer();

        // 3.set one row
        rowBatch->SetRow(i, new RowInfo(rowBuffer->TakeRowBuffer(), oneRowLen));
    }
    return rowBatch;
}

BaseVector *VectorHelper::CreateStringDictionary(const int32_t *values, int32_t valueSize,
    Vector<LargeStringContainer<std::string_view>> *vector, bool valueAsNullifyIndices)
{
    auto nullsBuffer = std::make_unique<NullsBuffer>(valueSize);
    for (int i = 0; i < valueSize; i++) {
        if (UNLIKELY(vector->IsNull(values[i]) || (valueAsNullifyIndices && values[i] == -1))) {
            nullsBuffer->SetNull(i);
        }
    }

    // todo:: handing other types of container
    auto dictionary = std::make_shared<DictionaryContainer<std::string_view>>(values, valueSize,
        unsafe::UnsafeStringVector::GetContainer(
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)),
        vector->GetSize(), vector->GetOffset());
    return new Vector<DictionaryContainer<std::string_view>>(valueSize, dictionary, nullsBuffer.get(), OMNI_CHAR);
}

BaseVector *VectorHelper::CreateVector(int32_t vectorEncodingId, int32_t dataTypeId, int32_t size,
    int32_t capacityInBytes)
{
    using namespace omniruntime::type;
    if (vectorEncodingId == OMNI_FLAT) {
        return DYNAMIC_TYPE_DISPATCH(CreateFlatVector, dataTypeId, size, capacityInBytes);
    } else if (vectorEncodingId == OMNI_ENCODING_CONTAINER) {
        return new ContainerVector(capacityInBytes, size);
    } else {
        std::string omniExceptionInfo =
            "In function CreateVector, no such encoding type " + std::to_string(vectorEncodingId);
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }
}

void VectorHelper::SetVectorDataType(BaseVector* vec, DataType* dataType)
{
    using namespace omniruntime::type;
    if (vec == nullptr || dataType == nullptr) {
        throw omniruntime::exception::OmniException("INVALID_ARGUMENT",
            "In function SetVectorDataType, vec or dataType is nullptr");
    }
    auto id = dataType->GetId();
    if (id == OMNI_DECIMAL128) {
        auto dt = dynamic_cast<Decimal128DataType *>(dataType);
        if (dt != nullptr) {
            vec->SetDataType(std::make_shared<Decimal128DataType>(dt->GetPrecision(), dt->GetScale()));
        } else {
            vec->SetDataType(std::make_shared<Decimal128DataType>(
                DECIMAL128_DEFAULT_PRECISION, DECIMAL128_DEFAULT_SCALE));
        }
    } else if (id == OMNI_DECIMAL64) {
        auto dt = dynamic_cast<Decimal64DataType *>(dataType);
        if (dt != nullptr) {
            vec->SetDataType(std::make_shared<Decimal64DataType>(dt->GetPrecision(), dt->GetScale()));
        } else {
            vec->SetDataType(std::make_shared<Decimal64DataType>(
                DECIMAL64_DEFAULT_PRECISION, DECIMAL64_DEFAULT_SCALE));
        }
    }
}

BaseVector *VectorHelper::CreateComplexVector(DataType* dataType, int32_t size)
{
    using namespace omniruntime::type;
    auto fieldType = dataType->GetId();
    if (fieldType == OMNI_ROW) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto rowType = static_cast<RowType*>(dataType);
        for (int i = 0; i < rowType->Size(); i++) {
            children.push_back(std::shared_ptr<BaseVector>(CreateComplexVector(rowType->Type(i).get(), size)));
        }
        return new RowVector(size, children);
    } else if (fieldType == OMNI_MAP) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto mapType = static_cast<MapType*>(dataType);
        for (int i = 0; i < mapType->Size(); i++) {
            children.push_back(std::shared_ptr<BaseVector>(CreateComplexVector(mapType->Children()[i].get(), 0)));
        }
        return new MapVector(size, children[0], children[1]);
    } else if (fieldType == OMNI_ARRAY) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto arrayType = static_cast<ArrayType*>(dataType);
        for (int i = 0; i < arrayType->Size(); i++) {
            children.push_back(
                std::shared_ptr<BaseVector>(CreateComplexVector(arrayType->ElementType().get(), 0))
            );
        }
        return new ArrayVector(size, children[0]);
    } else {
        auto vec = CreateFlatVector(fieldType, size);
        SetVectorDataType(vec, dataType);
        return vec;
    }
}

std::shared_ptr<BaseVector> VectorHelper::CreateComplexVectorShared(DataType* dataType, int32_t size)
{
    using namespace omniruntime::type;
    auto fieldType = dataType->GetId();
    if (fieldType == OMNI_ROW) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto rowType = static_cast<RowType*>(dataType);
        for (int i = 0; i < rowType->Size(); i++) {
            children.push_back(std::shared_ptr<BaseVector>(CreateComplexVector(rowType->Type(i).get(), size)));
        }
        return std::make_shared<RowVector>(size, children);
    } else if (fieldType == OMNI_MAP) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto mapType = static_cast<MapType*>(dataType);
        for (int i = 0; i < mapType->Size(); i++) {
            children.push_back(std::shared_ptr<BaseVector>(CreateComplexVector(mapType->Children()[i].get(), 0)));
        }
        return std::make_shared<MapVector>(size, children[0], children[1]);
    } else if (fieldType == OMNI_ARRAY) {
        std::vector<std::shared_ptr<BaseVector>> children;
        auto arrayType = static_cast<ArrayType*>(dataType);
        for (int i = 0; i < arrayType->Size(); i++) {
            children.push_back(std::shared_ptr<BaseVector>(CreateComplexVector(arrayType->ElementType().get(), 0)));
        }
        return std::make_shared<ArrayVector>(size, children[0]);
    } else {
        auto vec = CreateFlatVectorShared(fieldType, size);
        SetVectorDataType(vec.get(), dataType);
        return vec;
    }
}

BaseVector *VectorHelper::CreateEmptyComplexVector(DataType* dataType, int32_t size)
{
    using namespace omniruntime::type;
    auto fieldType = dataType->GetId();
    if (fieldType == OMNI_ROW) {
        return new RowVector(size);
    } else if (fieldType == OMNI_MAP) {
        return new MapVector(size);
    } else if (fieldType == OMNI_ARRAY) {
        auto arrayType = static_cast<ArrayType*>(dataType);
        return new ArrayVector(size);
    } else {
        return CreateFlatVector(fieldType, size);
    }
}

BaseVector *VectorHelper::CreateFlatVector(int32_t dataTypeId, int32_t size, int32_t capacityInBytes)
{
    using namespace omniruntime::type;
    return DYNAMIC_TYPE_DISPATCH(CreateFlatVector, dataTypeId, size, capacityInBytes);
}

std::shared_ptr<BaseVector> VectorHelper::CreateFlatVectorShared(int32_t dataTypeId, int32_t size,
    int32_t capacityInBytes)
{
    using namespace omniruntime::type;
    return DYNAMIC_TYPE_DISPATCH(CreateFlatVectorShared, dataTypeId, size, capacityInBytes);
}

void VectorHelper::SetValue(vec::BaseVector *vector, int32_t index, void *value)
{
    using namespace omniruntime::type;
    DataTypeId typeId = vector->GetTypeId();
    if (typeId == OMNI_ROW) {
        if (value == nullptr) {
            vector->SetNull(index);
        } else {
            static_cast<RowVector *>(vector)->Append(static_cast<BaseVector *>(value), index, 1);
        }
        return;
    }
    if (typeId == OMNI_MAP) {
        if (value == nullptr) {
            vector->SetNull(index);
        } else {
            static_cast<MapVector *>(vector)->SetValue(index, static_cast<MapVector *>(value));
        }
        return;
    }
    if (typeId == OMNI_ARRAY) {
        if (value == nullptr) {
            vector->SetNull(index);
        } else {
            static_cast<ArrayVector *>(vector)->SetValue(index, static_cast<BaseVector *>(value));
        }
        return;
    }
    return DYNAMIC_TYPE_DISPATCH(VectorSetValue, typeId, vector, index, value);
}

void VectorHelper::SetNull(vec::BaseVector *vector, int index)
{
    if (vector == nullptr) {
        return;
    }

    switch (vector->GetTypeId()) {
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY:
            static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->SetNull(index);
        break;
        case type::OMNI_ARRAY:
            static_cast<ArrayVector *>(vector)->SetNull(index);
        break;
        case type::OMNI_MAP:
            static_cast<MapVector *>(vector)->SetNull(index);
        break;
        case type::OMNI_ROW:
            static_cast<RowVector *>(vector)->SetNull(index);
        break;
        default:
            vector->SetNull(index);
        break;
    }
}

void VectorHelper::CopyValue(vec::BaseVector *srcVector, int32_t srcIndex, vec::BaseVector *dstVector, int32_t dstIndex)
{
    using namespace omniruntime::type;
    DataTypeId typeId = srcVector->GetTypeId();
    if (typeId == OMNI_NONE || typeId == OMNI_INVALID) {
        dstVector->SetNull(dstIndex);
        return;
    }
    if (typeId == OMNI_ROW) {
        auto *srcRow = static_cast<RowVector *>(srcVector);
        auto *dstRow = static_cast<RowVector *>(dstVector);
        if (srcRow->IsNull(srcIndex)) {
            dstRow->SetNull(dstIndex);
        } else {
            for (int32_t c = 0; c < srcRow->ChildSize(); ++c) {
                CopyValue(srcRow->ChildAt(c).get(), srcIndex, dstRow->ChildAt(c).get(), dstIndex);
            }
        }
        return;
    }
    if (typeId == OMNI_MAP) {
        auto *srcMap = static_cast<MapVector *>(srcVector);
        auto *dstMap = static_cast<MapVector *>(dstVector);
        if (srcMap->IsNull(srcIndex)) {
            dstMap->SetNull(dstIndex);
        } else {
            BaseVector *oneRow = srcMap->Slice(srcIndex, 1, true);
            dstMap->SetValue(dstIndex, static_cast<MapVector *>(oneRow));
            delete oneRow;
        }
        return;
    }
    if (typeId == OMNI_ARRAY) {
        auto *srcArr = static_cast<ArrayVector *>(srcVector);
        auto *dstArr = static_cast<ArrayVector *>(dstVector);
        if (srcArr->IsNull(srcIndex)) {
            dstArr->SetNull(dstIndex);
        } else {
            std::shared_ptr<BaseVector> slice = srcArr->GetArrayAt(srcIndex, true);
            dstArr->SetValue(dstIndex, slice.get());
        }
        return;
    }
    return DYNAMIC_TYPE_DISPATCH(VectorCopyValue, typeId, srcVector, srcIndex, dstVector, dstIndex);
}

void VectorHelper::PrintVecBatch(VectorBatch *vecBatch)
{
    int32_t vectorCount = vecBatch->GetVectorCount();
    int32_t rowCount = vecBatch->GetRowCount();
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        for (int32_t colIdx = 0; colIdx < vectorCount; ++colIdx) {
            auto vector = vecBatch->Get(colIdx);
            PrintVectorValue(vector, rowIdx);
        }
        std::cout << std::endl;
    }
}

void VectorHelper::PrintVec(BaseVector *vector, int32_t rowCount)
{
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        PrintVectorValue(vector, rowIdx);
        std::cout << std::endl;
    }
}

void VectorHelper::PrintVecVector(std::vector<BaseVector*> *vecVector, int32_t rowCount)
{
    int32_t vectorCount = vecVector->size();
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        for (int32_t colIdx = 0; colIdx < vectorCount; ++colIdx) {
            BaseVector* vector = (*vecVector)[colIdx];
            PrintVectorValue(vector, rowIdx);
        }
        std::cout << std::endl;
    }
}

void VectorHelper::PrintStructVectorValue(BaseVector* vector, int32_t rowIndex) {
    auto* structVec = dynamic_cast<RowVector*>(vector);
    if (!structVec) {
        std::cout << "[Invalid StructVector]";
        return;
    }
    if (structVec->IsNull(rowIndex)) {
        std::cout << "NULL\t";
        return;
    }

    std::cout << "{";
    int32_t childSize = structVec->ChildSize();
    for (int32_t i = 0; i < childSize; i++) {
        if (i > 0) {
            std::cout << ", ";
        }
        auto childVec = structVec->ChildAt(i);
        PrintVectorValue(childVec.get(), rowIndex);
    }
    std::cout << "}";
}

void VectorHelper::PrintArrayVectorValue(BaseVector* vector, int32_t rowIndex) {
    auto* arrayVec = dynamic_cast<ArrayVector*>(vector);
    if (!arrayVec) {
        std::cout << "[Invalid ArrayVector]";
        return;
    }
    int32_t offset = arrayVec->GetOffset(rowIndex);
    int32_t size = arrayVec->GetSize(rowIndex);
    auto elementVec = arrayVec->GetElementVector();
    if (!elementVec) {
        std::cout << "[No Element Vector]";
        return;
    }
    const auto elementType = elementVec->GetTypeId();
    std::cout << "[";
    for (int i = 0; i < size; i++) {
        if (i > 0)
            std::cout << ", ";

        if (arrayVec->IsNull(i)) {
            std::cout << "NULL\t";
            continue;
        }

        switch (elementType) {
            case OMNI_BYTE:
                PrintArrayElement<OMNI_BYTE>(elementVec.get(), offset + i);
                break;
            case OMNI_BOOLEAN:
                PrintArrayElement<OMNI_BOOLEAN>(elementVec.get(), offset + i);
                break;
            case OMNI_SHORT:
                PrintArrayElement<OMNI_SHORT>(elementVec.get(), offset + i);
                break;
            case OMNI_INT:
                PrintArrayElement<OMNI_INT>(elementVec.get(), offset + i);
                break;
            case OMNI_FLOAT:
                PrintArrayElement<OMNI_FLOAT>(elementVec.get(), offset + i);
                break;
            case OMNI_DATE32:
                PrintArrayElement<OMNI_DATE32>(elementVec.get(), offset + i);
                break;
            case OMNI_LONG:
                PrintArrayElement<OMNI_LONG>(elementVec.get(), offset + i);
                break;
            case OMNI_TIMESTAMP:
                PrintArrayElement<OMNI_TIMESTAMP>(elementVec.get(), offset + i);
                break;
            case OMNI_DOUBLE:
                PrintArrayElement<OMNI_DOUBLE>(elementVec.get(), offset + i);
                break;
            case OMNI_DATE64:
                PrintArrayElement<OMNI_DATE64>(elementVec.get(), offset + i);
                break;
            case OMNI_DECIMAL64:
                PrintArrayElement<OMNI_DECIMAL64>(elementVec.get(), offset + i);
                break;
            case OMNI_DECIMAL128:
                PrintArrayElement<OMNI_DECIMAL128>(elementVec.get(), offset + i);
                break;
            case OMNI_VARBINARY:
                PrintArrayElement<OMNI_VARBINARY>(elementVec.get(), offset + i);
                break;
            case OMNI_VARCHAR:
                PrintArrayElement<OMNI_VARCHAR>(elementVec.get(), offset + i);
                break;
            case OMNI_CHAR:
                PrintArrayElement<OMNI_CHAR>(elementVec.get(), offset + i);
                break;
            default:
                throw omniruntime::exception::OmniException("PrintArrayVectorValue", "Unsupported type: " + elementType);
        }
    }
    std::cout << "]";
}

void VectorHelper::PrintArrayVectorOffsetsAndNulls(BaseVector* vector, int32_t rowIndex)
{
    auto* arrayVec = dynamic_cast<ArrayVector*>(vector);
    if (!arrayVec) {
        throw omniruntime::exception::OmniException("RUNTIME_ERROR", "ArrayVector is null!");
    }
    int32_t offset = arrayVec->GetOffset(rowIndex);
    int32_t size = arrayVec->GetSize(rowIndex);
    std::cout << rowIndex << "--- offset: " << offset << "; size: " << size << "; isNull: " << arrayVec->IsNull(rowIndex) << std::endl;
}

void VectorHelper::PrintMapVectorValue(MapVector *mapVec, int32_t rowIndex)
{
    if (mapVec->IsNull(rowIndex)) {
        std::cout << "NULL\t";
        return;
    }

    auto offsets = mapVec->GetOffsets();
    uint32_t start = offsets[rowIndex];
    uint32_t end = offsets[rowIndex + 1];

    if (start == end) {
        std::cout << "{}\t";
        return;
    }
    std::cout << "{";
    auto keysVec = mapVec->GetKeyVector();
    auto valuesVec = mapVec->GetValueVector();

    const char* sep = "";
    for (uint32_t i = start; i < end; ++i) {
        std::cout << sep;
        sep = ",";
        PrintVectorValue(keysVec.get(), static_cast<int32_t>(i));
        std::cout << ":";
        PrintVectorValue(valuesVec.get(), static_cast<int32_t>(i));
    }
    std::cout << "}\t";
}

void VectorHelper::PrintVectorValue(BaseVector *vector, int32_t rowIndex)
{
    using namespace omniruntime::type;
    if (vector->IsNull(rowIndex)) {
        std::cout << "NULL"
                  << "\t";
        return;
    }

    auto encoding = vector->GetEncoding();
    if (encoding == vec::OMNI_DICTIONARY) {
        DYNAMIC_TYPE_DISPATCH(PrintDictionaryVectorValue, vector->GetTypeId(), vector, rowIndex);
    } else if (encoding == vec::OMNI_ENCODING_CONTAINER) {
        auto vecCount = static_cast<ContainerVector *>(vector)->GetVectorCount();
        for (int32_t vecIdx = 0; vecIdx < vecCount; vecIdx++) {
            auto value = static_cast<ContainerVector *>(vector)->GetValue(vecIdx);
            auto valueVec = reinterpret_cast<BaseVector *>(value);
            DYNAMIC_TYPE_DISPATCH(PrintFlatVectorValue, valueVec->GetTypeId(), valueVec, rowIndex);
        }
    } else if (encoding == vec::OMNI_ENCODING_CONST) {
        DYNAMIC_TYPE_DISPATCH(PrintConstVectorValue, vector->GetTypeId(), vector, rowIndex);
    } else if (encoding == vec::OMNI_ENCODING_ARRAY) {
        PrintArrayVectorValue(vector, rowIndex);
    } else if (encoding == vec::OMNI_ENCODING_STRUCT) {
        PrintStructVectorValue(vector, rowIndex);
    } else if (encoding == vec::OMNI_ENCODING_MAP) {
        PrintMapVectorValue(static_cast<MapVector *>(vector), rowIndex);
    } else {
        DYNAMIC_TYPE_DISPATCH(PrintFlatVectorValue, vector->GetTypeId(), vector, rowIndex);
    }
}

BaseVector *VectorHelper::CreateDictionaryVector(int *values, int valueSize, BaseVector *vector, int dataTypeId, bool valueAsNullifyIndices)
{
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<int32_t> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_SHORT:
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<int16_t> *>(vector), valueAsNullifyIndices);
        case type::OMNI_BYTE:
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<int8_t> *>(vector), valueAsNullifyIndices);
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<int64_t> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_DECIMAL128: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<type::Decimal128> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_DOUBLE: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<double> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_FLOAT: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<float> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_BOOLEAN: {
            return CreateDictionary(values, valueSize, reinterpret_cast<Vector<bool> *>(vector), valueAsNullifyIndices);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return CreateStringDictionary(values, valueSize,
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector), valueAsNullifyIndices);
        }
        default: {
            std::string omniExceptionInfo =
                "In function CreateDictionaryVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void *VectorHelper::UnsafeGetValuesDictionary(BaseVector *vector)
{
    switch (vector->GetTypeId()) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vector)));
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<int16_t>> *>(vector)));
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<int8_t>> *>(vector)));
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(vector)));
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<type::Decimal128>> *>(vector)));
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<double>> *>(vector)));
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<float>> *>(vector)));
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<bool>> *>(vector)));
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetIds(
                reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector)));
        }
        default: {
            std::string omniExceptionInfo =
                "In function UnsafeGetValuesDictionary, no such data type " + std::to_string(vector->GetTypeId());
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

BaseVector *VectorHelper::CastConstVectorToVector(BaseVector *vector)
{
    switch (vector->GetTypeId()) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            auto *col = new Vector<int32_t>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<int32_t> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_SHORT: {
            auto *col = new Vector<int16_t>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<int16_t> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_BYTE: {
            auto *col = new Vector<int8_t>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<int8_t> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            auto *col = new Vector<int64_t>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<int64_t> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_DECIMAL128: {
            auto *col = new Vector<Decimal128>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<Decimal128> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_DOUBLE: {
            auto *col = new Vector<double>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<double> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_FLOAT: {
            auto *col = new Vector<float>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<float> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_BOOLEAN: {
            auto *col = new Vector<bool>(1);
            col->SetValue(0, reinterpret_cast<ConstVector<bool> *>(vector)->GetConstValue());
            delete vector;
            return col;
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            auto *col = new Vector<LargeStringContainer<std::string_view>>(1);
            // ConstVector for VARCHAR/CHAR is instantiated as ConstVector<std::string_view>,
            // NOT ConstVector<std::string>. They have different memory layouts!
            auto tmp = reinterpret_cast<ConstVector<std::string_view> *>(vector)->GetConstValue();
            col->SetValue(0, tmp);
            delete vector;
            return col;
        }
        default: {
            std::string omniExceptionInfo = "In function UnsafeGetValuesDictionary, no such data type " +
                std::to_string(vector->GetTypeId());
            delete vector;
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void *VectorHelper::UnsafeGetValues(BaseVector *vector)
{
    if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
        // ConstVector does not have a flat values buffer.
        // Return nullptr so callers (e.g. Transform) can handle it safely.
        return nullptr;
    }
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        return UnsafeGetValuesDictionary(vector);
    }
    DataTypeId dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(vector)));
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int16_t> *>(vector)));
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int8_t> *>(vector)));
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int64_t> *>(vector)));
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<type::Decimal128> *>(vector)));
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<double> *>(vector)));
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<void *>(
                    unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<float> *>(vector)));
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<bool> *>(vector)));
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<void *>(unsafe::UnsafeStringVector::GetValues(
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)));
        }
        case type::OMNI_CONTAINER:
            return reinterpret_cast<void *>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<ContainerVector *>(vector)));
        case type::OMNI_ARRAY:
        case type::OMNI_MAP:
        case type::OMNI_ROW:
            return 0;
        default: {
            std::string omniExceptionInfo =
                "In function UnsafeGetValues, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("VECTOR_HELPER_ERROR", omniExceptionInfo);
        }
    }
}

void *VectorHelper::UnsafeGetOffsetsAddr(BaseVector *vector)
{
    DataTypeId dataTypeId = vector->GetTypeId();
    if (dataTypeId == type::OMNI_VARCHAR || dataTypeId == type::OMNI_CHAR || dataTypeId == type::OMNI_VARBINARY) {
        if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
            // ConstVector has no offsets array; the JIT getter functions handle
            // constant string values directly via the ConstVector pointer.
            return nullptr;
        }
        if (vector->GetEncoding() == OMNI_DICTIONARY) {
            auto dictVarCharVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(vector);
            return unsafe::UnsafeDictionaryVector::GetDictionaryOffsets(dictVarCharVec);
        } else {
            auto varCharVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
            return reinterpret_cast<void *>(unsafe::UnsafeStringVector::GetOffsets(varCharVec));
        }
    }
    if (dataTypeId == type::OMNI_ARRAY) {
        return reinterpret_cast<void *>(reinterpret_cast<ArrayVector *>(vector)->GetOffsets());
    }
    return nullptr;
}

BaseVector *VectorHelper::SliceDictionaryVector(BaseVector *vector, int positionOffset, int length)
{
    switch (vector->GetTypeId()) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<Vector<DictionaryContainer<int16_t>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<Vector<DictionaryContainer<int8_t>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<Vector<DictionaryContainer<double>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<Vector<DictionaryContainer<float>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<Vector<DictionaryContainer<bool>> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(vector)->Slice(positionOffset,
                length);
        }
        default: {
            std::string omniExceptionInfo =
                "In function SliceDictionaryVector, no such data type " + std::to_string(vector->GetTypeId());
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

BaseVector *VectorHelper::SliceConstVector(BaseVector *vector, int length)
{
    DataTypeId dataTypeId = vector->GetTypeId();
    BaseVector *result = nullptr;
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32:
            result = new ConstVector<int32_t>(
                reinterpret_cast<ConstVector<int32_t> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_SHORT:
            result = new ConstVector<int16_t>(
                reinterpret_cast<ConstVector<int16_t> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_BYTE:
            result = new ConstVector<int8_t>(
                reinterpret_cast<ConstVector<int8_t> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64:
            result = new ConstVector<int64_t>(
                reinterpret_cast<ConstVector<int64_t> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_DECIMAL128:
            result = new ConstVector<type::Decimal128>(
                reinterpret_cast<ConstVector<type::Decimal128> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_DOUBLE:
            result = new ConstVector<double>(
                reinterpret_cast<ConstVector<double> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_FLOAT:
            result = new ConstVector<float>(
                reinterpret_cast<ConstVector<float> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_BOOLEAN:
            result = new ConstVector<bool>(
                reinterpret_cast<ConstVector<bool> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
            result = new ConstVector<std::string_view>(
                reinterpret_cast<ConstVector<std::string_view> *>(vector)->GetConstValue(), dataTypeId, length);
            break;
        default: {
            std::string omniExceptionInfo =
                "In function SliceConstVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }

    // copy nullsBuffer
    if (vector->HasNull()) {
        result->SetNulls(0, true, length);
    }

    return result;
}

BaseVector *VectorHelper::SliceVector(BaseVector *vector, int positionOffset, int length)
{
    auto encoding = vector->GetEncoding();
    if (encoding == OMNI_DICTIONARY) {
        return SliceDictionaryVector(vector, positionOffset, length);
    }
    if (encoding == OMNI_ENCODING_CONST) {
        return SliceConstVector(vector, length);
    }
    if (encoding == OMNI_ENCODING_CONTAINER) {
        auto containerVector = reinterpret_cast<ContainerVector *>(vector);
        std::vector<int64_t> newFieldVecs;
        auto fieldCount = containerVector->GetVectorCount();
        for (int32_t i = 0; i < fieldCount; i++) {
            auto field = reinterpret_cast<BaseVector *>(containerVector->GetValue(i));
            auto newField = SliceVector(field, positionOffset, length);
            newFieldVecs.emplace_back(reinterpret_cast<int64_t>(newField));
        }
        return new ContainerVector(length, newFieldVecs, containerVector->GetDataTypes());
    }
    DataTypeId dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<Vector<int32_t> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<Vector<int16_t> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<Vector<int8_t> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<Vector<int64_t> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<Vector<type::Decimal128> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<Vector<double> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<Vector<float> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<Vector<bool> *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->Slice(positionOffset,
                length);
        }
        case type::OMNI_ARRAY: {
            return reinterpret_cast<ArrayVector *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_MAP: {
            return reinterpret_cast<MapVector *>(vector)->Slice(positionOffset, length);
        }
        case type::OMNI_ROW: {
            return reinterpret_cast<RowVector *>(vector)->Slice(positionOffset, length);
        }
        default: {
            std::string omniExceptionInfo =
                "In function SliceVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

BaseVector *VectorHelper::CopyPositionsDictionaryVector(BaseVector *vector, int *positions, int offset, int length)
{
    DataTypeId dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vector)->CopyPositions(positions,
                offset, length);
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<Vector<DictionaryContainer<int16_t>> *>(vector)->CopyPositions(positions,
                offset, length);
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<Vector<DictionaryContainer<int8_t>> *>(vector)->CopyPositions(positions,
                offset, length);
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(vector)->CopyPositions(positions,
                offset, length);
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<Vector<DictionaryContainer<type::Decimal128>> *>(vector)->CopyPositions(
                positions, offset, length);
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<Vector<DictionaryContainer<double>> *>(vector)->CopyPositions(positions, offset,
                length);
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<Vector<DictionaryContainer<float>> *>(vector)->CopyPositions(positions, offset,
                length);
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<Vector<DictionaryContainer<bool>> *>(vector)->CopyPositions(positions, offset,
                length);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector)
                ->CopyPositions(positions, offset, length);
        }
        default: {
            std::string omniExceptionInfo =
                "In fuction CopyPositionsDictionaryVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

std::unique_ptr<VectorBatch> VectorHelper::CopyVectorBatch(VectorBatch *src)
{
    const int32_t rowCount = src->GetRowCount();
    const int32_t vecCount = src->GetVectorCount();
    std::vector<int32_t> identity(static_cast<size_t>(rowCount));
    for (int32_t i = 0; i < rowCount; ++i) {
        identity[static_cast<size_t>(i)] = i;
    }
    auto batch = std::make_unique<VectorBatch>(static_cast<size_t>(rowCount));
    batch->ResizeVectorCount(vecCount);
    int *const pos = rowCount > 0 ? reinterpret_cast<int *>(identity.data()) : nullptr;
    for (int32_t i = 0; i < vecCount; ++i) {
        BaseVector *col = src->Get(i);
        if (col == nullptr) {
            batch->SetVector(i, nullptr);
            continue;
        }
        batch->SetVector(i, VectorHelper::CopyPositionsVector(col, pos, 0, rowCount));
    }
    return batch;
}

BaseVector *VectorHelper::CopyPositionsVector(BaseVector *vector, int *positions, int offset, int length)
{
    if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
        return SliceConstVector(vector, length);
    }
    if (vector->GetEncoding() == OMNI_DICTIONARY) {
        return CopyPositionsDictionaryVector(vector, positions, offset, length);
    }
    DataTypeId dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<Vector<int32_t> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<Vector<int16_t> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<Vector<int8_t> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<Vector<int64_t> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<Vector<double> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<Vector<float> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<Vector<bool> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vector)->CopyPositions(
                positions, offset, length);
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<Vector<type::Decimal128> *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_CONTAINER: {
            return reinterpret_cast<ContainerVector *>(vector)->CopyPositions(positions, offset, length);
        }
        case type::OMNI_ARRAY: {
            ArrayVector* arrayVector = reinterpret_cast<ArrayVector*>(vector);
            ArrayVector* result = arrayVector->CopyPositions(positions, offset, length);
            return reinterpret_cast<BaseVector*>(result);
        }
        case type::OMNI_MAP: {
            MapVector* mapVector = reinterpret_cast<MapVector*>(vector);
            MapVector* result = mapVector->CopyPositions(positions, offset, length);
            return reinterpret_cast<BaseVector*>(result);
        }
        case type::OMNI_ROW: {
            RowVector* rowVector = reinterpret_cast<RowVector*>(vector);
            RowVector* result = rowVector->CopyPositions(positions, offset, length);
            return reinterpret_cast<BaseVector*>(result);
        }
        default: {
            std::string omniExceptionInfo =
                "In function CopyPositionsVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void *VectorHelper::UnsafeGetDictionary(BaseVector *vector)
{
    DataTypeId dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<int32_t>> *>(vector)));
        }
        case type::OMNI_SHORT: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<int16_t>> *>(vector)));
        }
        case type::OMNI_BYTE: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<int8_t>> *>(vector)));
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<int64_t>> *>(vector)));
        }
        case type::OMNI_DECIMAL128: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<type::Decimal128>> *>(vector)));
        }
        case type::OMNI_DOUBLE: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<double>> *>(vector)));
        }
        case type::OMNI_FLOAT: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<float>> *>(vector)));
        }
        case type::OMNI_BOOLEAN: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetDictionary(
                static_cast<Vector<DictionaryContainer<bool>> *>(vector)));
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return reinterpret_cast<void *>(unsafe::UnsafeDictionaryVector::GetVarCharDictionary(
                static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector)));
        }
        default: {
            std::string omniExceptionInfo =
                "In function UnsafeGetDictionary, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void VectorHelper::AppendVector(BaseVector *destVector, int32_t offset, BaseVector *srcVector, int32_t length)
{
    DataTypeId dataTypeId = destVector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            reinterpret_cast<Vector<int32_t> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_SHORT: {
            reinterpret_cast<Vector<int16_t> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_BYTE: {
            reinterpret_cast<Vector<int8_t> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            reinterpret_cast<Vector<int64_t> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_DECIMAL128: {
            reinterpret_cast<Vector<type::Decimal128> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_DOUBLE: {
            reinterpret_cast<Vector<double> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_FLOAT: {
            reinterpret_cast<Vector<float> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_BOOLEAN: {
            reinterpret_cast<Vector<bool> *>(destVector)->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(destVector)
                ->Append(srcVector, offset, length);
            break;
        }
        case type::OMNI_CONTAINER:
            reinterpret_cast<ContainerVector *>(destVector)->Append(srcVector, offset, length);
            break;
        case type::OMNI_ARRAY:
            reinterpret_cast<ArrayVector *>(destVector)->Append(srcVector, offset, length);
            break;
        case type::OMNI_MAP:
            reinterpret_cast<MapVector *>(destVector)->Append(srcVector, offset, length);
            break;
        case type::OMNI_ROW:
            reinterpret_cast<RowVector *>(destVector)->Append(srcVector, offset, length);
            break;
        default: {
            std::string omniExceptionInfo =
                "In function AppendVector, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void VectorHelper::ExpandElementVector(BaseVector *elementVec, type::DataTypeId typeId, int32_t newSize) {
    switch (typeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            auto *vec = dynamic_cast<Vector<int32_t> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_SHORT: {
            auto *vec = dynamic_cast<Vector<int16_t> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_BYTE: {
            auto *vec = dynamic_cast<Vector<int8_t> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            auto *vec = dynamic_cast<Vector<int64_t> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_DECIMAL128: {
            auto *vec = dynamic_cast<Vector<type::Decimal128> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_DOUBLE: {
            auto *vec = dynamic_cast<Vector<double> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_FLOAT: {
            auto *vec = dynamic_cast<Vector<float> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_BOOLEAN: {
            auto *vec = dynamic_cast<Vector<bool> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_CHAR:
        case type::OMNI_VARCHAR: {
            auto *vec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elementVec);
            vec->Expand(newSize);
            break;
        }
        default:
            throw OmniException("ExpandElementVector", "Unsupported element type: " + std::to_string(typeId));
    }
}

BaseVector *VectorHelper::DecodeVarcharDictionaryVector(BaseVector *vector)
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    int size = vector->GetSize();
    VarcharVector *flatVector = new VarcharVector(size);
    auto dicVector =
        reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector);
    for (int i = 0; i < size; i++) {
        if (dicVector->IsNull(i)) {
            flatVector->SetNull(i);
        } else {
            std::string_view value = dicVector->GetValue(i);
            flatVector->SetValue(i, value);
        }
    }
    return flatVector;
}

BaseVector *VectorHelper::DecodeDictionaryVector(BaseVector *vector)
{
    int32_t dataTypeId = vector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            return DecodeFlatDictionaryVector<int32_t>(vector);
        }
        case type::OMNI_SHORT: {
            return DecodeFlatDictionaryVector<int16_t>(vector);
        }
        case type::OMNI_BYTE: {
            return DecodeFlatDictionaryVector<int8_t>(vector);
        }
        case type::OMNI_LONG:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64: {
            return DecodeFlatDictionaryVector<int64_t>(vector);
        }
        case type::OMNI_DECIMAL128: {
            return DecodeFlatDictionaryVector<type::Decimal128>(vector);
        }
        case type::OMNI_DOUBLE: {
            return DecodeFlatDictionaryVector<double>(vector);
        }
        case type::OMNI_FLOAT: {
            return DecodeFlatDictionaryVector<float>(vector);
        }
        case type::OMNI_BOOLEAN: {
            return DecodeFlatDictionaryVector<bool>(vector);
        }
        case type::OMNI_VARBINARY:
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR: {
            return DecodeVarcharDictionaryVector(vector);
        }
        default: {
            std::string message("No such data type " + std::to_string(dataTypeId));
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", message);
        }
    }
}

// used for copy from flat vector to another flat vector, and it does not support string types
void VectorHelper::CopyFlatVector(BaseVector *destVector, BaseVector *srcVector, int32_t offset, int32_t length)
{
    if (srcVector->GetEncoding() != OMNI_FLAT || destVector->GetEncoding() != OMNI_FLAT) {
        std::string message("Unsupported copy vector from or to a non flat vector.");
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    auto destDataTypeId = destVector->GetTypeId();
    auto srcDataTypeId = srcVector->GetTypeId();
    if (destDataTypeId != srcDataTypeId) {
        std::string message("Can't copy from data type " + std::to_string(srcDataTypeId) + " to data type " +
            std::to_string(destDataTypeId));
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    auto srcSize = srcVector->GetSize();
    if (UNLIKELY(offset + length > srcSize)) {
        std::string message("Copy vector out of src range(needed size:%d, real size:%d).", offset + length,
            srcSize);
        throw OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    auto destSize = destVector->GetSize();
    if (length > destSize) {
        std::string message("Can't copy since dest vector size " + std::to_string(destSize) +
            " is smaller than src length " + std::to_string(length));
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", message);
    }

    auto destNulls = unsafe::UnsafeBaseVector::GetNulls(destVector);
    auto srcNulls = unsafe::UnsafeBaseVector::GetNulls(srcVector);
    BitUtil::CopyBits(reinterpret_cast<uint64_t *>(srcNulls), offset, reinterpret_cast<uint64_t *>(destNulls), 0,
        length);
    switch (destDataTypeId) {
        case type::OMNI_INT:
        case type::OMNI_DATE32: {
            CopyFlatVector<int32_t>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_SHORT: {
            CopyFlatVector<int16_t>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_BYTE: {
            CopyFlatVector<int8_t>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_LONG:
        case type::OMNI_DATE64:
        case type::OMNI_DECIMAL64:
        case type::OMNI_TIMESTAMP: {
            CopyFlatVector<int64_t>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_DECIMAL128: {
            CopyFlatVector<Decimal128>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_DOUBLE: {
            CopyFlatVector<double>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_FLOAT: {
            CopyFlatVector<float>(destVector, srcVector, offset, length);
            break;
        }
        case type::OMNI_BOOLEAN: {
            CopyFlatVector<bool>(destVector, srcVector, offset, length);
            break;
        }
        default: {
            std::string message("Unsupported data type " + std::to_string(destDataTypeId));
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", message);
        }
    }
}

DataTypePtr VectorHelper::GetDataType(BaseVector *srcVector)
{
    DataTypeId dataTypeId = srcVector->GetTypeId();
    switch (dataTypeId) {
        case type::OMNI_INT: {
            return IntType();
        }
        case type::OMNI_DATE32: {
            return Date32Type();
        }
        case type::OMNI_SHORT: {
            return ShortType();
        }
        case type::OMNI_BYTE: {
            return ByteType();
        }
        case type::OMNI_LONG: {
            return LongType();
        }
        case type::OMNI_TIMESTAMP: {
            return TimestampType();
        }
        case type::OMNI_DATE64: {
            return Date64Type();
        }
        case type::OMNI_DECIMAL64: {
            return Decimal64Type();
        }
        case type::OMNI_DECIMAL128: {
            return Decimal128Type();
        }
        case type::OMNI_DOUBLE: {
            return DoubleType();
        }
        case type::OMNI_FLOAT: {
            return FloatType();
        }
        case type::OMNI_BOOLEAN: {
            return BooleanType();
        }
        case type::OMNI_VARBINARY: {
            return VarBinaryType();
        }
        case type::OMNI_VARCHAR: {
            return VarcharType();
        }
        case type::OMNI_CHAR: {
            return CharType();
        }
        case type::OMNI_CONTAINER: {
            return ContainerType();
        }
        case type::OMNI_ROW: {
            auto rowVector = static_cast<RowVector *>(srcVector);
            std::vector<DataTypePtr> fieldTypes;
            for (int i = 0; i < rowVector->ChildSize(); i++) {
                fieldTypes.push_back(GetDataType(rowVector->ChildAt(i).get()));
            }
            return RowDataType(fieldTypes);
        }
        case type::OMNI_ARRAY: {
            auto arrayVector = static_cast<ArrayVector *>(srcVector);
            return ArrayDataType(GetDataType(arrayVector->GetElementVector().get()));
        }
        case type::OMNI_MAP: {
            auto mapVector = static_cast<MapVector *>(srcVector);
            return MapDataType(GetDataType(mapVector->GetKeyVector().get()),
                GetDataType(mapVector->GetValueVector().get()));
        }
        default: {
            std::string omniExceptionInfo =
                "In function GetDataType, no such data type " + std::to_string(dataTypeId);
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
        }
    }
}

void VectorHelper::EmptyArrayProjection(ArrayVector *dstArrVec, DataTypeId typeId)
{
    if (dstArrVec == nullptr) {
        return;
    }
    auto emptyElemVec = VectorHelper::CreateFlatVector(static_cast<int32_t>(typeId), 0);
    dstArrVec->SetElementVector(std::shared_ptr<BaseVector>(emptyElemVec));
}

void VectorHelper::EmptyMapProjection(MapVector *dstArrVec, DataTypeId keyTypeId, DataTypeId valueTypeId)
{
    if (dstArrVec == nullptr) {
        return;
    }
    auto emptyKeyElemVec = VectorHelper::CreateFlatVector(static_cast<int32_t>(keyTypeId),0);
    auto emptyValueElemVec = VectorHelper::CreateFlatVector(static_cast<int32_t>(valueTypeId),0);
    dstArrVec->SetKeyVector(std::shared_ptr<BaseVector>(emptyKeyElemVec));
    dstArrVec->SetValueVector(std::shared_ptr<BaseVector>(emptyValueElemVec));
}

void VectorHelper::CreateNullArrayVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output)
{
    output = new ArrayVector(rowSize);
    auto arrayVec = dynamic_cast<ArrayVector *>(output);
    auto arrayTypePtr = std::dynamic_pointer_cast<ArrayType>(dataType);
    if (!arrayTypePtr) {
        OMNI_THROW("Runtime error", "DataType is marked as OMNI_ARRAY but actual object is not ArrayType");
    }
    auto emptyElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateComplexVector(arrayTypePtr->ElementType().get(), 0));
    arrayVec->SetElementVector(emptyElements);
    for (int32_t i = 0; i < rowSize; ++i) {
        arrayVec->SetNull(i);
    }
}

void VectorHelper::CreateNullMapVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output)
{
    output = new MapVector(rowSize);
    auto mapVec = dynamic_cast<MapVector *>(output);
    auto mapTypePtr = std::dynamic_pointer_cast<MapType>(dataType);
    if (!mapTypePtr) {
        OMNI_THROW("Runtime error", "DataType is marked as OMNI_MAP but actual object is not MapType");
    }
    auto emptyKeyElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateComplexVector(mapTypePtr->Key().get(), 0));
    auto emptyValueElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateComplexVector(mapTypePtr->Value().get(), 0));
    mapVec->SetKeyVector(emptyKeyElements);
    mapVec->SetValueVector(emptyValueElements);
    for (int32_t i = 0; i < rowSize; ++i) {
        mapVec->SetNull(i);
    }
}

void VectorHelper::CreateNullRowVector(int32_t rowSize, const DataTypePtr& dataType, BaseVector*& output)
{
    output = new RowVector(rowSize);
    auto rowVector = dynamic_cast<RowVector *>(output);
    auto rowTypePtr = std::dynamic_pointer_cast<RowType>(dataType);
    if (!rowTypePtr) {
        OMNI_THROW("Runtime error", "DataType is marked as OMNI_ROW but actual object is not RowType");
    }
    for (const auto& childTypePtr : rowTypePtr->Children()) {
        BaseVector *childRaw = nullptr;
        const auto cid = childTypePtr->GetId();
        if (cid == OMNI_ROW) {
            CreateNullRowVector(rowSize, childTypePtr, childRaw);
        } else if (cid == OMNI_ARRAY) {
            CreateNullArrayVector(rowSize, childTypePtr, childRaw);
        } else if (cid == OMNI_MAP) {
            CreateNullMapVector(rowSize, childTypePtr, childRaw);
        } else {
            childRaw = VectorHelper::CreateFlatVector(static_cast<int32_t>(cid), rowSize);
            for (int32_t i = 0; i < rowSize; ++i) {
                childRaw->SetNull(i);
            }
        }
        rowVector->AddChild(std::shared_ptr<BaseVector>(childRaw));
    }
    for (int32_t i = 0; i < rowSize; ++i) {
        rowVector->SetNull(i);
    }
}

std::string_view VectorHelper::GetStringValueFromVector(BaseVector *vec, int32_t row) {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("OMNI RUNTIME ERROR", "Unsupported encoding type for string");
    }
}
} // namespace omniruntime::vec
