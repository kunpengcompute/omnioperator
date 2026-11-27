/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayConstructor function implementation
 */

#include "ArrayConstructorFunction.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

bool ArrayConstructorFunction::IsValueNull(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return vec->IsNull(0);
    }
    return vec->IsNull(row);
}

template <typename T>
T ArrayConstructorFunction::GetValue(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    }
    return static_cast<Vector<T> *>(vec)->GetValue(row);
}

std::string_view ArrayConstructorFunction::GetStringValue(BaseVector *vec, int32_t row)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = dynamic_cast<ConstVector<std::string_view> *>(vec);
        if (constVec != nullptr) {
            return constVec->GetConstValue();
        }
        return std::string_view();
    }
    if (vec->GetEncoding() == OMNI_DICTIONARY) {
        // Dictionary-encoded varchar produced by e.g. the native ORC reader.
        // dynamic_cast<Vector<LargeStringContainer<...>>*> on it returns nullptr
        // and would otherwise silently produce empty strings.
        using DictVarcharVector = Vector<DictionaryContainer<std::string_view, LargeStringContainer>>;
        auto *dictVec = dynamic_cast<DictVarcharVector *>(vec);
        if (dictVec != nullptr) {
            return dictVec->GetValue(row);
        }
        return std::string_view();
    }
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    auto *varcharVec = dynamic_cast<VarcharVector *>(vec);
    if (varcharVec != nullptr) {
        return varcharVec->GetValue(row);
    }
    return std::string_view();
}

void ArrayConstructorFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    std::vector<BaseVector *> argVectors;
    auto paramNums = context->getInputParamsNUms();
    while (!args.empty() && (paramNums-- > 0)) {
        argVectors.push_back(args.top());
        args.pop();
    }
    std::reverse(argVectors.begin(), argVectors.end());

    int32_t rowSize = context->GetResultRowSize();

    if (argVectors.empty()) {
        ConstructEmptyArray(result, rowSize, context->GetPool());
        return;
    }

    DataTypeId elementTypeId = argVectors[0]->GetTypeId();

    switch (elementTypeId) {
        case OMNI_BYTE:
            ConstructArray<int8_t>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_SHORT:
            ConstructArray<int16_t>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ConstructArray<int32_t>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ConstructArray<int64_t>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_FLOAT:
            ConstructArray<float>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DOUBLE:
            ConstructArray<double>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_BOOLEAN:
            ConstructArray<bool>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_DECIMAL128:
            ConstructArray<Decimal128>(argVectors, result, rowSize, elementTypeId, context->GetPool());
            break;
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY:
            ConstructArrayVarchar(argVectors, result, rowSize, context->GetPool());
            break;
        case OMNI_ARRAY:
            ConstructArrayNested(argVectors, result, rowSize, context->GetPool());
            break;
        case OMNI_MAP:
            ConstructArrayMap(argVectors, result, rowSize, context->GetPool());
            break;
        case OMNI_ROW:
            ConstructArrayStruct(argVectors, result, rowSize, context->GetPool());
            break;
        default:
            for (auto *vec : argVectors) {
                delete vec;
            }
            OMNI_THROW("ArrayConstructorFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }

    for (auto *vec : argVectors) {
        delete vec;
    }
}

template <typename T>
void ArrayConstructorFunction::ConstructArray(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
    int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    size_t numArgs = argVectors.size();
    int64_t totalElements = static_cast<int64_t>(rowSize) * static_cast<int64_t>(numArgs);

    auto resultElementVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(elementTypeId), static_cast<int32_t>(totalElements)));
    auto *typedResultElementVec = dynamic_cast<Vector<T> *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayConstructorFunction Error:", "Failed to create result element vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(offset));

        for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
            if (IsValueNull(argVectors[argIdx], row)) {
                typedResultElementVec->SetNull(static_cast<int32_t>(offset));
            } else {
                T value = GetValue<T>(argVectors[argIdx], row);
                typedResultElementVec->SetValue(static_cast<int32_t>(offset), value);
            }
            offset++;
        }

        arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = arrayResult;
}

void ArrayConstructorFunction::ConstructArrayVarchar(const std::vector<BaseVector *> &argVectors,
    BaseVector *&result, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    size_t numArgs = argVectors.size();
    int64_t totalElements = static_cast<int64_t>(rowSize) * static_cast<int64_t>(numArgs);

    auto resultElementVec = std::shared_ptr<BaseVector>(
        new VarcharVector(static_cast<int32_t>(totalElements), pool));
    auto *typedResultElementVec = dynamic_cast<VarcharVector *>(resultElementVec.get());
    if (!typedResultElementVec) {
        OMNI_THROW("ArrayConstructorFunction Error:", "Failed to create result varchar vector");
    }

    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        arrayResult->SetOffset(row, static_cast<int32_t>(offset));

        for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
            if (IsValueNull(argVectors[argIdx], row)) {
                typedResultElementVec->SetNull(static_cast<int32_t>(offset));
            } else {
                std::string_view value = GetStringValue(argVectors[argIdx], row);
                typedResultElementVec->SetValue(static_cast<int32_t>(offset), value);
            }
            offset++;
        }

        arrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = arrayResult;
}

void ArrayConstructorFunction::ConstructArrayNested(const std::vector<BaseVector *> &argVectors,
    BaseVector *&result, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    size_t numArgs = argVectors.size();
    int64_t totalMiddleElements = static_cast<int64_t>(rowSize) * static_cast<int64_t>(numArgs);

    auto *firstArrayArg = dynamic_cast<ArrayVector *>(argVectors[0]);
    if (!firstArrayArg) {
        OMNI_THROW("ArrayConstructorFunction Error:", "Expected ArrayVector for nested array construction");
    }
    auto innerElementVec = firstArrayArg->GetElementVector();
    DataTypeId innerElemTypeId = innerElementVec ? innerElementVec->GetTypeId() : OMNI_INT;

    int64_t totalInnerElements = 0;
    for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
        auto *arrayArg = dynamic_cast<ArrayVector *>(argVectors[argIdx]);
        if (!arrayArg) {
            continue;
        }
        for (int32_t row = 0; row < rowSize; ++row) {
            if (!IsValueNull(argVectors[argIdx], row)) {
                totalInnerElements += arrayArg->GetSize(row);
            }
        }
    }

    std::shared_ptr<BaseVector> flatInnerElements;
    if (TypeUtil::IsStringType(innerElemTypeId)) {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        flatInnerElements = std::shared_ptr<BaseVector>(
            new VarcharVector(static_cast<int32_t>(totalInnerElements), pool));
    } else {
        flatInnerElements = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(innerElemTypeId),
                static_cast<int32_t>(totalInnerElements)));
    }

    auto middleArrayVec = std::shared_ptr<BaseVector>(
        new ArrayVector(totalMiddleElements, flatInnerElements, pool));
    auto *middleArray = dynamic_cast<ArrayVector *>(middleArrayVec.get());

    int64_t innerOffset = 0;
    int32_t middleIdx = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
            middleArray->SetOffset(middleIdx, static_cast<int32_t>(innerOffset));

            if (IsValueNull(argVectors[argIdx], row)) {
                middleArray->SetNull(middleIdx);
                middleArray->SetOffset(middleIdx + 1, static_cast<int32_t>(innerOffset));
            } else {
                auto *arrayArg = dynamic_cast<ArrayVector *>(argVectors[argIdx]);
                if (!arrayArg) {
                    middleArray->SetNull(middleIdx);
                    middleArray->SetOffset(middleIdx + 1, static_cast<int32_t>(innerOffset));
                } else {
                    int64_t srcStart = arrayArg->GetOffset(row);
                    int64_t srcLen = arrayArg->GetSize(row);
                    auto srcElemVec = arrayArg->GetElementVector();

                    if (srcLen > 0 && srcElemVec) {
                        BaseVector *srcSlice = srcElemVec->Slice(
                            static_cast<int>(srcStart), static_cast<int>(srcLen), false);
                        VectorHelper::AppendVector(flatInnerElements.get(),
                            static_cast<int32_t>(innerOffset), srcSlice, static_cast<int32_t>(srcLen));
                        delete srcSlice;
                    }
                    innerOffset += srcLen;
                    middleArray->SetOffset(middleIdx + 1, static_cast<int32_t>(innerOffset));
                }
            }
            middleIdx++;
        }
    }

    auto *outerArrayResult = new ArrayVector(rowSize, middleArrayVec, pool);
    int64_t outerOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        outerArrayResult->SetOffset(row, static_cast<int32_t>(outerOffset));
        outerOffset += static_cast<int64_t>(numArgs);
        outerArrayResult->SetOffset(row + 1, static_cast<int32_t>(outerOffset));
    }

    result = outerArrayResult;
}

void ArrayConstructorFunction::ConstructArrayMap(const std::vector<BaseVector *> &argVectors,
    BaseVector *&result, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    size_t numArgs = argVectors.size();
    int64_t totalMapElements = static_cast<int64_t>(rowSize) * static_cast<int64_t>(numArgs);

    MapVector *firstMapArg = nullptr;
    for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
        firstMapArg = dynamic_cast<MapVector *>(argVectors[argIdx]);
        if (firstMapArg) break;
    }
    if (!firstMapArg) {
        OMNI_THROW("ArrayConstructorFunction Error:", "Expected MapVector for map array construction");
    }
    DataTypeId keyTypeId = firstMapArg->GetKeyVector()->GetTypeId();
    DataTypeId valueTypeId = firstMapArg->GetValueVector()->GetTypeId();

    int64_t totalKVPairs = 0;
    for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
        auto *mapArg = dynamic_cast<MapVector *>(argVectors[argIdx]);
        if (!mapArg) continue;
        for (int32_t row = 0; row < rowSize; ++row) {
            if (!IsValueNull(argVectors[argIdx], row)) {
                totalKVPairs += mapArg->GetSize(row);
            }
        }
    }

    auto createFlatVecByType = [pool](DataTypeId typeId, int32_t size) -> std::shared_ptr<BaseVector> {
        if (TypeUtil::IsStringType(typeId)) {
            using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
            return std::shared_ptr<BaseVector>(new VarcharVector(size, pool));
        }
        return std::shared_ptr<BaseVector>(VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(typeId), size));
    };
    auto keyVec = createFlatVecByType(keyTypeId, static_cast<int32_t>(totalKVPairs));
    auto valueVec = createFlatVecByType(valueTypeId, static_cast<int32_t>(totalKVPairs));

    auto middleMapVec = std::shared_ptr<BaseVector>(
        new MapVector(totalMapElements, keyVec, valueVec, pool));
    auto *middleMap = dynamic_cast<MapVector *>(middleMapVec.get());

    int64_t kvOffset = 0;
    int32_t mapIdx = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
            middleMap->SetOffset(mapIdx, static_cast<int32_t>(kvOffset));

            if (IsValueNull(argVectors[argIdx], row)) {
                middleMap->SetNull(mapIdx);
                middleMap->SetOffset(mapIdx + 1, static_cast<int32_t>(kvOffset));
            } else {
                auto *mapArg = dynamic_cast<MapVector *>(argVectors[argIdx]);
                if (!mapArg) {
                    middleMap->SetNull(mapIdx);
                    middleMap->SetOffset(mapIdx + 1, static_cast<int32_t>(kvOffset));
                } else {
                    int64_t srcStart = mapArg->GetOffset(row);
                    int64_t srcLen = mapArg->GetSize(row);

                    if (srcLen > 0) {
                        auto srcKeyVec = mapArg->GetKeyVector();
                        auto srcValueVec = mapArg->GetValueVector();
                        BaseVector *keySlice = srcKeyVec->Slice(
                            static_cast<int>(srcStart), static_cast<int>(srcLen), false);
                        VectorHelper::AppendVector(keyVec.get(),
                            static_cast<int32_t>(kvOffset), keySlice, static_cast<int32_t>(srcLen));
                        delete keySlice;
                        BaseVector *valueSlice = srcValueVec->Slice(
                            static_cast<int>(srcStart), static_cast<int>(srcLen), false);
                        VectorHelper::AppendVector(valueVec.get(),
                            static_cast<int32_t>(kvOffset), valueSlice, static_cast<int32_t>(srcLen));
                        delete valueSlice;
                    }
                    kvOffset += srcLen;
                    middleMap->SetOffset(mapIdx + 1, static_cast<int32_t>(kvOffset));
                }
            }
            mapIdx++;
        }
    }

    auto *outerArrayResult = new ArrayVector(rowSize, middleMapVec, pool);
    int64_t outerOffset = 0;
    for (int32_t row = 0; row < rowSize; ++row) {
        outerArrayResult->SetOffset(row, static_cast<int32_t>(outerOffset));
        outerOffset += static_cast<int64_t>(numArgs);
        outerArrayResult->SetOffset(row + 1, static_cast<int32_t>(outerOffset));
    }

    result = outerArrayResult;
}

BaseVector* ArrayConstructorFunction::CreateVectorLike(BaseVector* source, int32_t size,
    omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto encoding = source->GetEncoding();
    if (encoding == OMNI_ENCODING_STRUCT) {
        auto* srcRow = dynamic_cast<RowVector*>(source);
        std::vector<std::shared_ptr<BaseVector>> children;
        for (int32_t c = 0; c < srcRow->ChildSize(); ++c) {
            children.push_back(std::shared_ptr<BaseVector>(
                CreateVectorLike(srcRow->ChildAt(c).get(), size, pool)));
        }
        return new RowVector(size, children, pool);
    } else if (encoding == OMNI_ENCODING_ARRAY) {
        auto* srcArray = dynamic_cast<ArrayVector*>(source);
        auto elemChild = std::shared_ptr<BaseVector>(
            CreateVectorLike(srcArray->GetElementVector().get(), 0, pool));
        return new ArrayVector(size, elemChild, pool);
    } else if (encoding == OMNI_ENCODING_MAP) {
        auto* srcMap = dynamic_cast<MapVector*>(source);
        auto keyChild = std::shared_ptr<BaseVector>(
            CreateVectorLike(srcMap->GetKeyVector().get(), 0, pool));
        auto valueChild = std::shared_ptr<BaseVector>(
            CreateVectorLike(srcMap->GetValueVector().get(), 0, pool));
        return new MapVector(size, keyChild, valueChild, pool);
    } else {
        return VectorHelper::CreateVector(pool, OMNI_FLAT, source->GetTypeId(), size);
    }
}

void ArrayConstructorFunction::ConstructArrayStruct(const std::vector<BaseVector *> &argVectors,
    BaseVector *&result, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    size_t numArgs = argVectors.size();
    int64_t totalElements = static_cast<int64_t>(rowSize) * static_cast<int64_t>(numArgs);

    RowVector *firstRowArg = nullptr;
    for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
        firstRowArg = dynamic_cast<RowVector *>(argVectors[argIdx]);
        if (firstRowArg) break;
    }
    if (!firstRowArg) {
        OMNI_THROW("ArrayConstructorFunction Error:",
            "Expected RowVector for struct array construction");
    }

    auto* elementRow = dynamic_cast<RowVector*>(
        CreateVectorLike(firstRowArg, static_cast<int32_t>(totalElements), pool));
    auto elementRowVec = std::shared_ptr<BaseVector>(elementRow);

    auto *outerArrayResult = new ArrayVector(rowSize, elementRowVec, pool);
    int64_t offset = 0;

    for (int32_t row = 0; row < rowSize; ++row) {
        outerArrayResult->SetOffset(row, static_cast<int32_t>(offset));
        for (size_t argIdx = 0; argIdx < numArgs; ++argIdx) {
            int32_t elemIdx = static_cast<int32_t>(offset);
            if (IsValueNull(argVectors[argIdx], row)) {
                elementRow->SetNull(elemIdx);
                for (int32_t c = 0; c < elementRow->ChildSize(); ++c) {
                    elementRow->ChildAt(c)->SetNull(elemIdx);
                }
            } else {
                auto *srcRow = dynamic_cast<RowVector*>(argVectors[argIdx]);
                if (!srcRow) {
                    elementRow->SetNull(elemIdx);
                    for (int32_t c = 0; c < elementRow->ChildSize(); ++c) {
                        elementRow->ChildAt(c)->SetNull(elemIdx);
                    }
                } else {
                    elementRow->SetNotNull(elemIdx);
                    for (int32_t c = 0; c < firstRowArg->ChildSize(); ++c) {
                        if (srcRow->ChildAt(c)->IsNull(row)) {
                            elementRow->ChildAt(c)->SetNull(elemIdx);
                        } else {
                            elementRow->ChildAt(c)->SetNotNull(elemIdx);
                            VectorHelper::CopyValue(
                                srcRow->ChildAt(c).get(), row,
                                elementRow->ChildAt(c).get(), elemIdx);
                        }
                    }
                }
            }
            offset++;
        }
        outerArrayResult->SetOffset(row + 1, static_cast<int32_t>(offset));
    }

    result = outerArrayResult;
}

void ArrayConstructorFunction::ConstructEmptyArray(BaseVector *&result, int32_t rowSize,
    omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto resultElementVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(pool, static_cast<int32_t>(OMNI_INT), 0));
    auto *arrayResult = new ArrayVector(rowSize, resultElementVec, pool);

    for (int32_t row = 0; row <= rowSize; ++row) {
        arrayResult->SetOffset(row, 0);
    }

    result = arrayResult;
}

std::shared_ptr<VectorFunction> makeArrayConstructor(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &)
{
    return std::make_shared<ArrayConstructorFunction>();
}

std::vector<std::shared_ptr<codegen::FunctionSignature>> ArrayConstructorSignatures()
{
    std::vector<std::shared_ptr<codegen::FunctionSignature>> signatures;

    std::vector<DataTypeId> supportedTypes = {
        OMNI_BOOLEAN,
        OMNI_BYTE,
        OMNI_SHORT,
        OMNI_INT,
        OMNI_LONG,
        OMNI_FLOAT,
        OMNI_DOUBLE,
        OMNI_VARCHAR,
        OMNI_CHAR,
        OMNI_DATE32,
        OMNI_TIMESTAMP,
        OMNI_DECIMAL64,
        OMNI_DECIMAL128,
        OMNI_VARBINARY,
        OMNI_ARRAY,
        OMNI_MAP,
        OMNI_ROW
    };

    for (const auto &typeId : supportedTypes) {
        signatures.emplace_back(
            codegen::FunctionSignature::Variadic("array", typeId, OMNI_ARRAY, 1));
    }

    return signatures;
}

template void ArrayConstructorFunction::ConstructArray<int8_t>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<int16_t>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<int32_t>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<int64_t>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<float>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<double>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<bool>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;
template void ArrayConstructorFunction::ConstructArray<Decimal128>(const std::vector<BaseVector *> &,
    BaseVector *&, int32_t, DataTypeId, omniruntime::mem::OmniTaskMemoryPool *) const;

template int8_t ArrayConstructorFunction::GetValue<int8_t>(BaseVector *, int32_t);
template int16_t ArrayConstructorFunction::GetValue<int16_t>(BaseVector *, int32_t);
template int32_t ArrayConstructorFunction::GetValue<int32_t>(BaseVector *, int32_t);
template int64_t ArrayConstructorFunction::GetValue<int64_t>(BaseVector *, int32_t);
template float ArrayConstructorFunction::GetValue<float>(BaseVector *, int32_t);
template double ArrayConstructorFunction::GetValue<double>(BaseVector *, int32_t);
template bool ArrayConstructorFunction::GetValue<bool>(BaseVector *, int32_t);
template Decimal128 ArrayConstructorFunction::GetValue<Decimal128>(BaseVector *, int32_t);

} // namespace omniruntime::vectorization
