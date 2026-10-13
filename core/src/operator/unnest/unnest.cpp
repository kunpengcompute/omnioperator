/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "unnest.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {

namespace {
// Recursively decode OMNI_DICTIONARY encoded vectors to OMNI_FLAT, including children
// of complex vectors (RowVector / ArrayVector / MapVector). Required because the unnest
// operator only handles FLAT / ARRAY / MAP / STRUCT encodings, and common producers
// (e.g. ORC native reader) emit dictionary-encoded leaf columns — especially for
// varchar/char — which would otherwise either be rejected by the encoding whitelist
// or cause undefined behaviour via static_cast to Vector<LargeStringContainer<...>>*
// inside VectorHelper::CopyValue.
// Returns a vector the caller owns. If no decoding was needed, returns `source` and
// `replaced` stays false; otherwise a newly-allocated vector is returned and the
// caller is responsible for releasing the original.
omniruntime::vec::BaseVector* NormalizeEncoding(omniruntime::vec::BaseVector* source, bool& replaced)
{
    using namespace omniruntime::vec;
    if (source == nullptr) {
        return source;
    }
    auto encoding = source->GetEncoding();
    if (encoding == OMNI_DICTIONARY) {
        replaced = true;
        return VectorHelper::DecodeDictionaryVector(source, source->GetPool());
    }
    if (encoding == OMNI_ENCODING_STRUCT) {
        auto* srcRow = dynamic_cast<RowVector*>(source);
        if (srcRow == nullptr) {
            return source;
        }
        for (int32_t c = 0; c < srcRow->ChildSize(); ++c) {
            bool childReplaced = false;
            auto* original = srcRow->ChildAt(c).get();
            auto* normalized = NormalizeEncoding(original, childReplaced);
            if (childReplaced) {
                // RowVector::Set takes ownership of the raw pointer via shared_ptr,
                // and the previous child's shared_ptr will be released here.
                srcRow->Set(c, normalized);
            }
        }
        return source;
    }
    if (encoding == OMNI_ENCODING_ARRAY) {
        auto* srcArr = dynamic_cast<ArrayVector*>(source);
        if (srcArr == nullptr) {
            return source;
        }
        bool childReplaced = false;
        auto* original = srcArr->GetElementVector().get();
        auto* normalized = NormalizeEncoding(original, childReplaced);
        if (childReplaced) {
            srcArr->SetElementVector(std::shared_ptr<BaseVector>(normalized));
        }
        return source;
    }
    if (encoding == OMNI_ENCODING_MAP) {
        auto* srcMap = dynamic_cast<MapVector*>(source);
        if (srcMap == nullptr) {
            return source;
        }
        bool keyReplaced = false;
        auto* keyNorm = NormalizeEncoding(srcMap->GetKeyVector().get(), keyReplaced);
        if (keyReplaced) {
            srcMap->SetKeyVector(std::shared_ptr<BaseVector>(keyNorm));
        }
        bool valReplaced = false;
        auto* valNorm = NormalizeEncoding(srcMap->GetValueVector().get(), valReplaced);
        if (valReplaced) {
            srcMap->SetValueVector(std::shared_ptr<BaseVector>(valNorm));
        }
        return source;
    }
    return source;
}

omniruntime::vec::BaseVector* CreateOutputVectorLike(omniruntime::vec::BaseVector* source, int32_t size)
{
    using namespace omniruntime::vec;
    auto encoding = source->GetEncoding();

    if (encoding == OMNI_ENCODING_STRUCT) {
        auto* srcRow = dynamic_cast<RowVector*>(source);
        std::vector<std::shared_ptr<BaseVector>> children;
        for (int32_t c = 0; c < srcRow->ChildSize(); ++c) {
            children.push_back(std::shared_ptr<BaseVector>(
                CreateOutputVectorLike(srcRow->ChildAt(c).get(), size)));
        }
        return new RowVector(size, children, source->GetPool());
    } else if (encoding == OMNI_ENCODING_ARRAY) {
        auto* srcArray = dynamic_cast<ArrayVector*>(source);
        auto elemChild = std::shared_ptr<BaseVector>(
            CreateOutputVectorLike(srcArray->GetElementVector().get(), 0));
        return new ArrayVector(size, elemChild, source->GetPool());
    } else if (encoding == OMNI_ENCODING_MAP) {
        auto* srcMap = dynamic_cast<MapVector*>(source);
        auto keyChild = std::shared_ptr<BaseVector>(
            CreateOutputVectorLike(srcMap->GetKeyVector().get(), 0));
        auto valueChild = std::shared_ptr<BaseVector>(
            CreateOutputVectorLike(srcMap->GetValueVector().get(), 0));
        return new MapVector(size, keyChild, valueChild, source->GetPool());
    } else {
        return VectorHelper::CreateVector(source->GetPool(), OMNI_FLAT, source->GetTypeId(), size);
    }
}
} // anonymous namespace

UnnestOperatorFactory* UnnestOperatorFactory::CreateUnnestOperatorFactory(
    std::shared_ptr<const UnnestNode> planNode)
{
    return new UnnestOperatorFactory(planNode);
}

// Pool-bound override
Operator* UnnestOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    UnnestOperator *unnestOperator = new UnnestOperator(pool, planNode_);
    return unnestOperator;
}

// Pool-bound ctor — same body, chains Operator(pool).
UnnestOperator::UnnestOperator(omniruntime::mem::OmniTaskMemoryPool *pool, std::shared_ptr<const UnnestNode> planNode)
    : Operator(pool),
      withOrdinality_(planNode->withOrdinality()),
      outer_(planNode->outer()),
      outputVecBatch(nullptr)
{
    InitFromCtor(planNode);
}

void UnnestOperator::InitFromCtor(const std::shared_ptr<const UnnestNode>& planNode)
{
    const auto& unnestVariables = planNode->unnestVariables();
    for (const auto& variable : unnestVariables) {
        auto fieldExpr = dynamic_cast<FieldExpr *>(variable);
        if (!fieldExpr->FieldIsArray() && !fieldExpr->FieldIsMap()) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest operator supports only ARRAY and MAP types");
        }
        unnestChannels_.push_back(fieldExpr->colVal);
    }

    uint32_t outputChannel = 0;
    for (const auto& variable : planNode->replicateVariables()) {
        auto fieldExpr = dynamic_cast<FieldExpr *>(variable);
        identityProjections_.emplace_back(fieldExpr->colVal, outputChannel++);
    }
    outputTypeSize_ = planNode->OutputType()->GetSize();
}


int32_t UnnestOperator::AddInput(omniruntime::vec::VectorBatch* vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        return 0;
    }

    // Some producers (e.g. native ORC reader) feed dictionary-encoded leaf vectors
    // — including leaves nested inside STRUCT / ARRAY / MAP. The unnest fast paths
    // only accept FLAT / ARRAY / MAP / STRUCT encodings and also perform unchecked
    // static_cast<Vector<...>*> on leaves, so decode any dictionary encoding up front.
    for (int32_t i = 0; i < vecBatch->GetVectorCount(); ++i) {
        auto* vec = vecBatch->Get(i);
        bool replaced = false;
        auto* normalized = NormalizeEncoding(vec, replaced);
        if (replaced) {
            vecBatch->SetVector(i, normalized);
            delete vec;
        }
    }

    int32_t numElements = 0;
    rawMaxSizes_.resize(vecBatch->Get(0)->GetSize());
    
    // 特殊情况：如果没有unnest变量，直接复制所有行
    if (unnestChannels_.empty()) {
        std::fill(rawMaxSizes_.begin(), rawMaxSizes_.end(), 1);
        numElements = vecBatch->Get(0)->GetSize();
    } else {
        // 根据outer参数初始化：outer=true时初始化为1（保留null/empty行），outer=false时初始化为0（过滤null/empty行）
        std::fill(rawMaxSizes_.begin(), rawMaxSizes_.end(), outer_ ? 1 : 0);
        for (size_t channel = 0; channel < unnestChannels_.size(); ++channel) {
            const auto& unnestVector = vecBatch->Get(unnestChannels_[channel]);

            auto processVector = [&](auto* inputVector) {
                for (auto row = 0; row < unnestVector->GetSize(); ++row) {
                    // 只有当数组不为null时才更新rawMaxSizes_（类似Velox的实现）
                    if (!unnestVector->IsNull(row)) {
                        auto rowSize = inputVector->GetSize(row);
                        rawMaxSizes_[row] = (rowSize > rawMaxSizes_[row]) ? rowSize : rawMaxSizes_[row];
                    }
                    // 如果为null且outer=false，rawMaxSizes_[row]保持为0（将被过滤）
                    // 如果为null且outer=true，rawMaxSizes_[row]保持为1（将被保留）
                }
            };

            if (auto arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(unnestVector)) {
                processVector(arrayVector);
            } else if (auto mapVector = dynamic_cast<omniruntime::vec::MapVector*>(unnestVector)) {
                processVector(mapVector);
            }
        }

        // 计算numElements：所有rawMaxSizes_的总和，但对于outer模式，确保至少等于输入行数
        int32_t totalSizes = std::accumulate(rawMaxSizes_.begin(), rawMaxSizes_.end(), 0);
        if (outer_) {
            // 对于outer模式，确保每个输入行至少生成一行输出（用于null/empty数组）
            numElements = std::max(totalSizes, vecBatch->Get(0)->GetSize());
        } else {
            // 对于非outer模式，只计算实际元素数（null/empty数组贡献0）
            numElements = totalSizes;
        }
    }
    generateOutput(numElements, vecBatch);
    return 0;
}


int32_t UnnestOperator::GetOutput(omniruntime::vec::VectorBatch** resultVecBatch)
{
    if (outputVecBatch != nullptr) {
        *resultVecBatch = outputVecBatch;
        outputVecBatch = nullptr;
        return 0;
    }

    if (noMoreInput_) {
        SetStatus(OMNI_STATUS_FINISHED);
    }
    return 0;
}

void UnnestOperator::generateOutput(int32_t numElements, omniruntime::vec::VectorBatch *vecBatch)
{
    auto result = std::make_unique<VectorBatch>(numElements);
    result->ResizeVectorCount(outputTypeSize_);
    generateRepeatedColumns(numElements, vecBatch, result.get());
    generateUnrepeatedColumns(numElements, vecBatch, result.get());

    if (withOrdinality_) {
        generateOrdinalityColumns(numElements, vecBatch, result.get());
    }

    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    outputVecBatch = result.release();
}

template<typename VectorType>
void UnnestOperator::generateRepeatedValues(VectorType* inputVector, VectorType* outputVector)
{
    int32_t index = 0;
    int32_t inputSize = inputVector->GetSize();
    for (auto i = 0; i < inputSize; ++i) {
        // 只有当rawMaxSizes_[i] > 0时才生成repeated值
        // 对于outer=false的null/empty数组，rawMaxSizes_[i]为0，因此不会生成输出
        if (rawMaxSizes_[i] > 0) {
            if (inputVector->IsNull(i)) {
                for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputVector->SetNull(index++);
                }
            } else {
                auto value = inputVector->GetValue(i);
                for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputVector->SetValue(index++, value);
                }
            }
        }
    }
}

void UnnestOperator::generateComplexRepeatedValuesForType(DataTypeId typeId, int32_t inputSize, auto* inputVector, auto* outputVector,
                                                          BaseVector* inputElementVector, BaseVector* outputElementVector)
{
    switch (typeId) {
        case OMNI_BYTE:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<int8_t>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<int8_t>*>(outputElementVector));
            break;
        case OMNI_SHORT:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<int16_t>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<int16_t>*>(outputElementVector));
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<int32_t>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<int32_t>*>(outputElementVector));
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<int64_t>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<int64_t>*>(outputElementVector));
            break;
        case OMNI_DECIMAL128:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(outputElementVector));
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(outputElementVector));
            break;
        case OMNI_FLOAT:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<float>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<float>*>(outputElementVector));
            break;
        case OMNI_DOUBLE:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<double>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<double>*>(outputElementVector));
            break;
        case OMNI_BOOLEAN:
            generateComplexRepeatedValues(inputSize, inputVector, outputVector,
                                          dynamic_cast<omniruntime::vec::Vector<bool>*>(inputElementVector),
                                          dynamic_cast<omniruntime::vec::Vector<bool>*>(outputElementVector));
            break;
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "This type not supported yet");
    }
}

void UnnestOperator::generateArrayRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                                 omniruntime::vec::BaseVector* outputVector)
{
    auto inputArrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector);
    auto outputArrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(outputVector);
    auto inputElementVector = inputArrayVector->GetElementVector().get();
    auto outputElementVector = outputArrayVector->GetElementVector().get();
    auto typeId = inputElementVector->GetTypeId();
    auto inputSize = inputVector->GetSize();
    generateComplexRepeatedValuesForType(typeId, inputSize, inputArrayVector, outputArrayVector, inputElementVector, outputElementVector);
}

void UnnestOperator::generateMapRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                               omniruntime::vec::BaseVector* outputVector)
{
    auto inputMapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector);
    auto outputMapVector = dynamic_cast<omniruntime::vec::MapVector*>(outputVector);
    auto inputKeyVector = inputMapVector->GetKeyVector().get();
    auto inputValueVector = inputMapVector->GetValueVector().get();
    auto outputKeyVector = outputMapVector->GetKeyVector().get();
    auto outputValueVector = outputMapVector->GetValueVector().get();
    auto keyTypeId = inputKeyVector->GetTypeId();
    auto valueTypeid = inputValueVector->GetTypeId();
    auto inputSize = inputVector->GetSize();

    generateComplexRepeatedValuesForType(keyTypeId, inputSize, inputMapVector, outputMapVector, inputKeyVector, outputKeyVector);
    generateComplexRepeatedValuesForType(valueTypeid, inputSize, inputMapVector, outputMapVector, inputValueVector, outputValueVector);
}

void UnnestOperator::generateStructRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                                  omniruntime::vec::BaseVector* outputVector)
{
    auto* inputRowVector = dynamic_cast<omniruntime::vec::RowVector*>(inputVector);
    auto* outputRowVector = dynamic_cast<omniruntime::vec::RowVector*>(outputVector);
    int32_t index = 0;
    int32_t inputSize = inputVector->GetSize();

    for (int32_t i = 0; i < inputSize; ++i) {
        if (rawMaxSizes_[i] > 0) {
            for (int32_t j = 0; j < rawMaxSizes_[i]; ++j) {
                if (inputRowVector->IsNull(i)) {
                    outputRowVector->SetNull(index);
                    for (int32_t c = 0; c < outputRowVector->ChildSize(); ++c) {
                        outputRowVector->ChildAt(c)->SetNull(index);
                    }
                } else {
                    outputRowVector->SetNotNull(index);
                    for (int32_t c = 0; c < inputRowVector->ChildSize(); ++c) {
                        if (inputRowVector->ChildAt(c)->IsNull(i)) {
                            outputRowVector->ChildAt(c)->SetNull(index);
                        } else {
                            outputRowVector->ChildAt(c)->SetNotNull(index);
                            VectorHelper::CopyValue(
                                inputRowVector->ChildAt(c).get(), i,
                                outputRowVector->ChildAt(c).get(), index);
                        }
                    }
                }
                index++;
            }
        }
    }
}

template<typename VectorType>
void UnnestOperator::generateComplexRepeatedValues(int32_t inputSize, auto* inputVector, auto* outputVector,
                                                   VectorType* inputElementVector, VectorType* outputElementVector)
{
    int32_t index = 0;
    int32_t elementIndex = 0;
    for (auto i = 0; i < inputSize; ++i) {
        // 只有当rawMaxSizes_[i] > 0时才处理（该行会在输出中）
        if (rawMaxSizes_[i] > 0) {
            if (inputVector->IsNull(i)) {
                // 对于null数组，在输出中创建空数组
                for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputVector->SetSize(index++, 0);
                }
            } else {
                int64_t start = inputVector->GetOffset(i);
                int64_t end = inputVector->GetOffset(i + 1);
                int64_t length = end - start;
                
                // 对于repeated列（数组），将整个数组复制rawMaxSizes_[i]次
                // 每个输出行都获得整个数组的副本
                for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                    // 为这个输出行复制整个数组
                    for (auto k = start; k < end; ++k) {
                        if (inputElementVector->IsNull(k)) {
                            outputElementVector->SetNull(elementIndex++);
                        } else {
                            auto value = inputElementVector->GetValue(k);
                            outputElementVector->SetValue(elementIndex++, value);
                        }
                    }
                    outputVector->SetSize(index++, length);
                }
            }
        }
    }
}

template<typename VectorType>
void UnnestOperator::generateUnrepeatedValues(omniruntime::vec::BaseVector* inputVector, VectorType* elementVector, VectorType* outputVector)
{
    int32_t index = 0;
    int32_t inputSize = inputVector->GetSize();

    auto insetVector = [&](auto* unrepeatVector, auto* outputVector) {
        for (auto i = 0; i < inputSize; ++i) {
            if (unrepeatVector->IsNull(i)) {
                // 对于null数组，只有当outer=true时才生成null值
                if (outer_ && rawMaxSizes_[i] > 0) {
                    for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                        outputVector->SetNull(index++);
                    }
                }
                // 如果outer=false，rawMaxSizes_[i]为0，因此不会生成输出（被过滤）
            } else {
                auto start = unrepeatVector->GetOffset(i);
                auto length = unrepeatVector->GetSize(i);
                // 生成实际的数组元素
                for (auto j = 0; j < length; ++j) {
                    // Check if the element itself is null (important for stack function with NULL values)
                    if (elementVector->IsNull(start + j)) {
                        outputVector->SetNull(index++);
                    } else {
                        auto value = elementVector->GetValue(start + j);
                        outputVector->SetValue(index++, value);
                    }
                }
                // 当rawMaxSizes_[i] > length时生成null填充
                // 这在多数组unnest且数组大小不同时是必需的
                // 注意：此填充与outer参数无关 - 它用于对齐所有unnest列到相同的输出行数
                if (rawMaxSizes_[i] > length) {
                    for (auto j = length; j < rawMaxSizes_[i]; ++j) {
                        outputVector->SetNull(index++);
                    }
                }
            }
        }
    };

    if (auto arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector)) {
        insetVector(arrayVector, outputVector);
    } else if (auto mapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector)) {
        insetVector(mapVector, outputVector);
    }
}

void UnnestOperator::generateRepeatedColumns(int32_t numElements, omniruntime::vec::VectorBatch* vecBatch,
                                             omniruntime::vec::VectorBatch* resultVecBatch)
{
    for (const auto& projection : identityProjections_) {
        omniruntime::vec::BaseVector* inputVector = vecBatch->Get(projection.inputChannel);
        if (inputVector->GetEncoding() != OMNI_FLAT && inputVector->GetEncoding() != OMNI_ENCODING_ARRAY &&
            inputVector->GetEncoding() != OMNI_ENCODING_MAP && inputVector->GetEncoding() != OMNI_ENCODING_STRUCT) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest not support this encoding.");
        }
        auto typeId = inputVector->GetTypeId();
        omniruntime::vec::BaseVector* outputVector = nullptr;
        if (auto arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector)) {
            int64_t totalSize = 0;
            for (auto i = 0; i < inputVector->GetSize(); ++i) {
                totalSize += arrayVector->GetSize(i) * rawMaxSizes_[i];
            }
            auto elementVector = arrayVector->GetElementVector().get();
            if (elementVector->GetEncoding() != OMNI_FLAT) {
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest not support this encoding.");
            }
            auto typeId = elementVector->GetTypeId();
            auto newElementVector = std::shared_ptr<BaseVector>(VectorHelper::CreateVector(this->pool(), OMNI_FLAT, typeId, totalSize));
            outputVector = new omniruntime::vec::ArrayVector(numElements, newElementVector, this->pool());
        } else if (auto mapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector)) {
            int64_t totalSize = 0;
            for (auto i = 0; i < inputVector->GetSize(); ++i) {
                totalSize += mapVector->GetSize(i) * rawMaxSizes_[i];
            }
            auto keyVector = mapVector->GetKeyVector().get();
            auto valueVector = mapVector->GetValueVector().get();
            if (keyVector->GetEncoding() != OMNI_FLAT || valueVector->GetEncoding() != OMNI_FLAT) {
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest not support this encoding.");
            }
            auto newKeyVector = std::shared_ptr<BaseVector>(VectorHelper::CreateVector(this->pool(), OMNI_FLAT, keyVector->GetTypeId(), totalSize));
            auto newValueVector = std::shared_ptr<BaseVector>(VectorHelper::CreateVector(this->pool(), OMNI_FLAT, valueVector->GetTypeId(), totalSize));
            outputVector = new omniruntime::vec::MapVector(numElements, newKeyVector, newValueVector, this->pool());
        } else if (dynamic_cast<omniruntime::vec::RowVector*>(inputVector)) {
            outputVector = CreateOutputVectorLike(inputVector, numElements);
        } else {
            outputVector = VectorHelper::CreateVector(this->pool(), OMNI_FLAT, typeId, numElements);
        }

        switch (typeId) {
            case OMNI_BYTE:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<int8_t>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<int8_t>*>(outputVector));
                break;
            case OMNI_SHORT:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<int16_t>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<int16_t>*>(outputVector));
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<int32_t>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<int32_t>*>(outputVector));
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<int64_t>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<int64_t>*>(outputVector));
                break;
            case OMNI_DECIMAL128:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(outputVector));
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            case OMNI_VARBINARY:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(outputVector));
                break;
            case OMNI_FLOAT:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<float>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<float>*>(outputVector));
                break;
            case OMNI_DOUBLE:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<double>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<double>*>(outputVector));
                break;
            case OMNI_BOOLEAN:
                generateRepeatedValues(dynamic_cast<omniruntime::vec::Vector<bool>*>(inputVector),
                                       dynamic_cast<omniruntime::vec::Vector<bool>*>(outputVector));
                break;
            case OMNI_ARRAY:
                generateArrayRepeatedValues(inputVector, outputVector);
                break;
            case OMNI_MAP:
                generateMapRepeatedValues(inputVector, outputVector);
                break;
            case OMNI_ROW:
                generateStructRepeatedValues(inputVector, outputVector);
                break;
            default:
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "This type not supported yet");
        }

        resultVecBatch->SetVector(projection.outputChannel, outputVector);
    }
}

omniruntime::vec::BaseVector* UnnestOperator::generateUnrepeatedRowValues(
    omniruntime::vec::RowVector* elementVector, omniruntime::vec::BaseVector* inputVector, int32_t numElements)
{
    auto* outputRowVector = dynamic_cast<omniruntime::vec::RowVector*>(
        CreateOutputVectorLike(elementVector, numElements));

    auto* arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector);
    auto* mapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector);
    int32_t index = 0;
    int32_t inputSize = inputVector->GetSize();

    auto getOffset = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(arrayVector != nullptr ? arrayVector->GetOffset(row) : mapVector->GetOffset(row));
    };
    auto getSize = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(arrayVector != nullptr ? arrayVector->GetSize(row) : mapVector->GetSize(row));
    };

    for (int32_t i = 0; i < inputSize; ++i) {
        if (inputVector->IsNull(i)) {
            if (outer_ && rawMaxSizes_[i] > 0) {
                for (int32_t j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputRowVector->SetNull(index);
                    for (int32_t c = 0; c < outputRowVector->ChildSize(); ++c) {
                        outputRowVector->ChildAt(c)->SetNull(index);
                    }
                    index++;
                }
            }
        } else {
            int32_t start = getOffset(i);
            int32_t length = getSize(i);
            for (int32_t j = 0; j < length; ++j) {
                int32_t srcIdx = start + j;
                if (elementVector->IsNull(srcIdx)) {
                    outputRowVector->SetNull(index);
                    for (int32_t c = 0; c < outputRowVector->ChildSize(); ++c) {
                        outputRowVector->ChildAt(c)->SetNull(index);
                    }
                } else {
                    outputRowVector->SetNotNull(index);
                    for (int32_t c = 0; c < elementVector->ChildSize(); ++c) {
                        auto* srcChild = elementVector->ChildAt(c).get();
                        auto* dstChild = outputRowVector->ChildAt(c).get();
                        if (srcChild->IsNull(srcIdx)) {
                            dstChild->SetNull(index);
                        } else {
                            dstChild->SetNotNull(index);
                            VectorHelper::CopyValue(srcChild, srcIdx, dstChild, index);
                        }
                    }
                }
                index++;
            }
            if (rawMaxSizes_[i] > length) {
                for (int32_t j = length; j < rawMaxSizes_[i]; ++j) {
                    outputRowVector->SetNull(index);
                    for (int32_t c = 0; c < outputRowVector->ChildSize(); ++c) {
                        outputRowVector->ChildAt(c)->SetNull(index);
                    }
                    index++;
                }
            }
        }
    }
    return outputRowVector;
}

omniruntime::vec::BaseVector* UnnestOperator::generateUnrepeatedArrayValues(
    omniruntime::vec::ArrayVector* elementVector, omniruntime::vec::BaseVector* inputVector, int32_t numElements)
{
    auto* outerArrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector);
    auto* outerMapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector);
    int32_t inputSize = inputVector->GetSize();

    auto getOffset = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(
            outerArrayVector != nullptr ? outerArrayVector->GetOffset(row) : outerMapVector->GetOffset(row));
    };
    auto getSize = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(
            outerArrayVector != nullptr ? outerArrayVector->GetSize(row) : outerMapVector->GetSize(row));
    };

    int64_t totalInnerElements = 0;
    for (int32_t i = 0; i < inputSize; ++i) {
        if (rawMaxSizes_[i] > 0 && !inputVector->IsNull(i)) {
            int32_t start = getOffset(i);
            int32_t length = getSize(i);
            for (int32_t j = 0; j < length; ++j) {
                if (!elementVector->IsNull(start + j)) {
                    totalInnerElements += elementVector->GetSize(start + j);
                }
            }
        }
    }

    auto innerElementVec = std::shared_ptr<BaseVector>(
        CreateOutputVectorLike(elementVector->GetElementVector().get(), static_cast<int32_t>(totalInnerElements)));
    auto* outputArrayVector = new ArrayVector(numElements, innerElementVec, this->pool());

    int32_t outputIdx = 0;
    int64_t innerOffset = 0;

    for (int32_t i = 0; i < inputSize; ++i) {
        if (rawMaxSizes_[i] == 0) continue;

        if (inputVector->IsNull(i)) {
            if (outer_) {
                for (int32_t j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputArrayVector->SetOffset(outputIdx, static_cast<int32_t>(innerOffset));
                    outputArrayVector->SetNull(outputIdx);
                    outputArrayVector->SetOffset(outputIdx + 1, static_cast<int32_t>(innerOffset));
                    outputIdx++;
                }
            }
            continue;
        }

        int32_t start = getOffset(i);
        int32_t length = getSize(i);

        for (int32_t j = 0; j < length; ++j) {
            int32_t elemIdx = start + j;
            outputArrayVector->SetOffset(outputIdx, static_cast<int32_t>(innerOffset));

            if (elementVector->IsNull(elemIdx)) {
                outputArrayVector->SetNull(outputIdx);
                outputArrayVector->SetOffset(outputIdx + 1, static_cast<int32_t>(innerOffset));
            } else {
                outputArrayVector->SetNotNull(outputIdx);
                int64_t arrStart = elementVector->GetOffset(elemIdx);
                int64_t arrLen = elementVector->GetSize(elemIdx);

                if (arrLen > 0) {
                    auto srcElementVec = elementVector->GetElementVector();
                    BaseVector* slice = srcElementVec->Slice(
                        static_cast<int>(arrStart), static_cast<int>(arrLen), false);
                    VectorHelper::AppendVector(innerElementVec.get(),
                        static_cast<int32_t>(innerOffset), slice, static_cast<int32_t>(arrLen));
                    delete slice;
                }
                innerOffset += arrLen;
                outputArrayVector->SetOffset(outputIdx + 1, static_cast<int32_t>(innerOffset));
            }
            outputIdx++;
        }

        if (rawMaxSizes_[i] > length) {
            for (int32_t j = length; j < rawMaxSizes_[i]; ++j) {
                outputArrayVector->SetOffset(outputIdx, static_cast<int32_t>(innerOffset));
                outputArrayVector->SetNull(outputIdx);
                outputArrayVector->SetOffset(outputIdx + 1, static_cast<int32_t>(innerOffset));
                outputIdx++;
            }
        }
    }

    return outputArrayVector;
}

omniruntime::vec::BaseVector* UnnestOperator::generateUnrepeatedMapValues(
    omniruntime::vec::MapVector* elementVector, omniruntime::vec::BaseVector* inputVector, int32_t numElements)
{
    auto* arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector);
    auto* mapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector);
    int32_t inputSize = inputVector->GetSize();

    auto getOffset = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(arrayVector != nullptr ? arrayVector->GetOffset(row) : mapVector->GetOffset(row));
    };
    auto getSize = [&](int32_t row) -> int32_t {
        return static_cast<int32_t>(arrayVector != nullptr ? arrayVector->GetSize(row) : mapVector->GetSize(row));
    };

    int64_t totalKVPairs = 0;
    for (int32_t i = 0; i < inputSize; ++i) {
        if (rawMaxSizes_[i] > 0 && !inputVector->IsNull(i)) {
            int32_t start = getOffset(i);
            int32_t length = getSize(i);
            for (int32_t j = 0; j < length; ++j) {
                if (!elementVector->IsNull(start + j)) {
                    totalKVPairs += elementVector->GetSize(start + j);
                }
            }
        }
    }

    auto outputKeyVec = std::shared_ptr<BaseVector>(
        CreateOutputVectorLike(elementVector->GetKeyVector().get(), static_cast<int32_t>(totalKVPairs)));
    auto outputValueVec = std::shared_ptr<BaseVector>(
        CreateOutputVectorLike(elementVector->GetValueVector().get(), static_cast<int32_t>(totalKVPairs)));

    auto* outputMapVector = new MapVector(numElements, outputKeyVec, outputValueVec, this->pool());

    int32_t outputIdx = 0;
    int64_t kvOffset = 0;

    for (int32_t i = 0; i < inputSize; ++i) {
        if (rawMaxSizes_[i] == 0) continue;

        if (inputVector->IsNull(i)) {
            if (outer_) {
                for (int32_t j = 0; j < rawMaxSizes_[i]; ++j) {
                    outputMapVector->SetOffset(outputIdx, static_cast<int32_t>(kvOffset));
                    outputMapVector->SetNull(outputIdx);
                    outputMapVector->SetOffset(outputIdx + 1, static_cast<int32_t>(kvOffset));
                    outputIdx++;
                }
            }
            continue;
        }

        int32_t start = getOffset(i);
        int32_t length = getSize(i);

        for (int32_t j = 0; j < length; ++j) {
            int32_t elemIdx = start + j;
            outputMapVector->SetOffset(outputIdx, static_cast<int32_t>(kvOffset));

            if (elementVector->IsNull(elemIdx)) {
                outputMapVector->SetNull(outputIdx);
                outputMapVector->SetOffset(outputIdx + 1, static_cast<int32_t>(kvOffset));
            } else {
                outputMapVector->SetNotNull(outputIdx);
                int64_t mapStart = elementVector->GetOffset(elemIdx);
                int64_t mapLen = elementVector->GetSize(elemIdx);

                if (mapLen > 0) {
                    auto srcKeyVec = elementVector->GetKeyVector();
                    auto srcValueVec = elementVector->GetValueVector();
                    BaseVector* keySlice = srcKeyVec->Slice(
                        static_cast<int>(mapStart), static_cast<int>(mapLen), false);
                    VectorHelper::AppendVector(outputKeyVec.get(),
                        static_cast<int32_t>(kvOffset), keySlice, static_cast<int32_t>(mapLen));
                    delete keySlice;
                    BaseVector* valueSlice = srcValueVec->Slice(
                        static_cast<int>(mapStart), static_cast<int>(mapLen), false);
                    VectorHelper::AppendVector(outputValueVec.get(),
                        static_cast<int32_t>(kvOffset), valueSlice, static_cast<int32_t>(mapLen));
                    delete valueSlice;
                }
                kvOffset += mapLen;
                outputMapVector->SetOffset(outputIdx + 1, static_cast<int32_t>(kvOffset));
            }
            outputIdx++;
        }

        if (rawMaxSizes_[i] > length) {
            for (int32_t j = length; j < rawMaxSizes_[i]; ++j) {
                outputMapVector->SetOffset(outputIdx, static_cast<int32_t>(kvOffset));
                outputMapVector->SetNull(outputIdx);
                outputMapVector->SetOffset(outputIdx + 1, static_cast<int32_t>(kvOffset));
                outputIdx++;
            }
        }
    }

    return outputMapVector;
}

omniruntime::vec::BaseVector* UnnestOperator::generateUnrepeatedElementValues(
    omniruntime::vec::BaseVector* elementVector, omniruntime::vec::BaseVector* inputVector, int32_t numElements)
{
    if (auto rowElementVector = dynamic_cast<omniruntime::vec::RowVector*>(elementVector)) {
        return generateUnrepeatedRowValues(rowElementVector, inputVector, numElements);
    }
    if (auto mapElementVector = dynamic_cast<omniruntime::vec::MapVector*>(elementVector)) {
        return generateUnrepeatedMapValues(mapElementVector, inputVector, numElements);
    }
    if (auto arrayElementVector = dynamic_cast<omniruntime::vec::ArrayVector*>(elementVector)) {
        return generateUnrepeatedArrayValues(arrayElementVector, inputVector, numElements);
    }
    return generateUnrepeatedValuesForType(elementVector, inputVector, numElements);
}

void UnnestOperator::generateUnrepeatedColumns(int32_t numElements, omniruntime::vec::VectorBatch* vecBatch,
                                               omniruntime::vec::VectorBatch* resultVecBatch)
{
    size_t resultIndex = identityProjections_.size();
    for (auto channel : unnestChannels_) {
        omniruntime::vec::BaseVector* inputVector = vecBatch->Get(channel);
        if (auto arrayVector = dynamic_cast<omniruntime::vec::ArrayVector*>(inputVector)) {
            auto elementVector = arrayVector->GetElementVector().get();
            resultVecBatch->SetVector(resultIndex++,
                generateUnrepeatedElementValues(elementVector, inputVector, numElements));
        } else if (auto mapVector = dynamic_cast<omniruntime::vec::MapVector*>(inputVector)) {
            auto keyElementVector = mapVector->GetKeyVector().get();
            resultVecBatch->SetVector(resultIndex++,
                generateUnrepeatedElementValues(keyElementVector, inputVector, numElements));
            auto valueElementVector = mapVector->GetValueVector().get();
            resultVecBatch->SetVector(resultIndex++,
                generateUnrepeatedElementValues(valueElementVector, inputVector, numElements));
        }
    }
}

omniruntime::vec::BaseVector* UnnestOperator::generateUnrepeatedValuesForType(omniruntime::vec::BaseVector* elementVector,
                                                                              omniruntime::vec::BaseVector* inputVector, int32_t numElements)
{
    if (elementVector->GetEncoding() != OMNI_FLAT) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Unnest not support this encoding.");
    }

    auto typeId = elementVector->GetTypeId();
    auto outputVector = VectorHelper::CreateVector(this->pool(), OMNI_FLAT, typeId, numElements);

    switch (typeId) {
        case OMNI_BYTE:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<int8_t>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<int8_t>*>(outputVector));
            break;
        case OMNI_SHORT:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<int16_t>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<int16_t>*>(outputVector));
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<int32_t>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<int32_t>*>(outputVector));
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<int64_t>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<int64_t>*>(outputVector));
            break;
        case OMNI_DECIMAL128:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<Decimal128>*>(outputVector));
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>>*>(outputVector));
            break;
        case OMNI_DOUBLE:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<double>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<double>*>(outputVector));
            break;
        case OMNI_BOOLEAN:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<bool>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<bool>*>(outputVector));
            break;
        case OMNI_FLOAT:
            generateUnrepeatedValues(inputVector,
                                     dynamic_cast<omniruntime::vec::Vector<float>*>(elementVector),
                                     dynamic_cast<omniruntime::vec::Vector<float>*>(outputVector));
            break;
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "This type not supported yet");
    }
    return outputVector;
}

void UnnestOperator::generateOrdinalityColumns(int32_t numElements, omniruntime::vec::VectorBatch* vecBatch,
    omniruntime::vec::VectorBatch* resultVecBatch)
{
    int32_t index = 0;
    BaseVector* baseVector = VectorHelper::CreateVector(this->pool(), OMNI_FLAT, OMNI_LONG, numElements);
    auto ordVector = dynamic_cast<omniruntime::vec::Vector<int64_t>*>(baseVector);
    
    if (unnestChannels_.empty()) {
        resultVecBatch->SetVector(outputTypeSize_ - 1, baseVector);
        return;
    }
    
    const auto& unnestVector = vecBatch->Get(unnestChannels_[0]);
    int32_t inputSize = unnestVector->GetSize();
    
    // Generate ordinality following the exact same logic as generateUnrepeatedValues
    auto generateOrdinality = [&](auto* unrepeatVector) {
        for (auto i = 0; i < inputSize; ++i) {
            // Skip rows that won't be in output
            if (rawMaxSizes_[i] == 0) {
                continue;
            }
            
            if (unrepeatVector->IsNull(i)) {
                // Null array in outer mode: pos should be NULL
                for (auto j = 0; j < rawMaxSizes_[i]; ++j) {
                    ordVector->SetNull(index++);
                }
                continue;
            }
            
            auto length = unrepeatVector->GetSize(i);
            
            // Generate ordinality for actual array elements (0, 1, 2, ...)
            for (auto j = 0; j < length; ++j) {
                ordVector->SetValue(index++, j);
            }
            
            // Handle padding rows (when rawMaxSizes_[i] > length)
            if (rawMaxSizes_[i] > length) {
                // Empty array case: length=0, outer=true, rawMaxSizes_[i]=1
                if (length == 0 && outer_ && rawMaxSizes_[i] == 1) {
                    ordVector->SetNull(index++);
                } else {
                    // Padding rows: set to NULL
                    for (auto j = length; j < rawMaxSizes_[i]; ++j) {
                        ordVector->SetNull(index++);
                    }
                }
            }
        }
    };
    
    if (auto arrayVec = dynamic_cast<const vec::ArrayVector*>(unnestVector)) {
        generateOrdinality(arrayVec);
    } else if (auto mapVec = dynamic_cast<const vec::MapVector*>(unnestVector)) {
        generateOrdinality(mapVec);
    }
    
    resultVecBatch->SetVector(outputTypeSize_ - 1, baseVector);
}

OmniStatus UnnestOperator::Close()
{
    return OMNI_STATUS_NORMAL;
}
}
}
