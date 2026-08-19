/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Least and Greatest function implementation for vectorized comparison expressions
 */

#include "LeastGreatest.h"
#include "vector/vector.h"
#include <limits>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

// Helper function for comparing values with NaN handling (Spark SQL semantics)
// In Spark, NaN is considered greater than any other value
template<CompareMode Mode, typename T>
static bool shouldReplace(const T &newVal, const T &currentVal) {
    if constexpr (std::is_floating_point_v<T>) {
        // NaN handling: NaN is considered greater than any other value
        bool newIsNan = std::isnan(newVal);
        bool currentIsNan = std::isnan(currentVal);
        
        if (Mode == CompareMode::GREATEST) {
            // For greatest: NaN > everything, so if new is NaN, replace
            if (newIsNan) return true;
            if (currentIsNan) return false;
            return newVal > currentVal;
        } else {
            // For least: NaN > everything, so NaN is never the least
            if (newIsNan) return false;
            if (currentIsNan) return true;
            return newVal < currentVal;
        }
    } else {
        if constexpr (Mode == CompareMode::GREATEST) {
            return newVal > currentVal;
        } else {
            return newVal < currentVal;
        }
    }
}

template<CompareMode Mode>
void LeastGreatestFunction<Mode>::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, 
    BaseVector *&result, ExecutionContext *context) const {
    if (numArgs_ < 2 || args.size() < numArgs_) {
        OMNI_THROW("LeastGreatest function Error", "Least/Greatest requires at least 2 arguments");
    }
    std::vector<BaseVector *> argVectors;
    argVectors.reserve(numArgs_);
    for (size_t i = 0; i < numArgs_; ++i) {
        argVectors.push_back(args.top());
        args.pop();
    }
    std::reverse(argVectors.begin(), argVectors.end());
    DispatchCompare(argVectors, outputType, result, context);
}

template<CompareMode Mode>
bool LeastGreatestFunction<Mode>::HasNullArg(const std::vector<BaseVector *> &argVectors, int32_t row) const {
    if (!propagateNull_) {
        return false;
    }
    for (auto *argVec : argVectors) {
        int32_t nullCheckIdx = (argVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
        if (argVec->IsNull(nullCheckIdx)) {
            return true;
        }
    }
    return false;
}

template<CompareMode Mode>
void LeastGreatestFunction<Mode>::DispatchCompare(const std::vector<BaseVector *> &argVectors, 
    const DataTypePtr &outputType, BaseVector *&result, ExecutionContext *context) const {
    DataTypeId outputTypeId = outputType->GetId();
    
    if (TypeUtil::IsStringType(outputTypeId)) {
        CompareString(argVectors, result, outputType, context);
    } else if (outputTypeId == OMNI_BOOLEAN) {
        CompareBoolean(argVectors, result, outputType, context);
    } else {
        // Numeric types
        switch (outputTypeId) {
            case OMNI_BYTE:
                CompareNumeric<int8_t>(argVectors, result, outputType, context);
                break;
            case OMNI_SHORT:
                CompareNumeric<int16_t>(argVectors, result, outputType, context);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                CompareNumeric<int32_t>(argVectors, result, outputType, context);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                CompareNumeric<int64_t>(argVectors, result, outputType, context);
                break;
            case OMNI_FLOAT:
                CompareNumeric<float>(argVectors, result, outputType, context);
                break;
            case OMNI_DOUBLE:
                CompareNumeric<double>(argVectors, result, outputType, context);
                break;
            case OMNI_DECIMAL128:
                CompareNumeric<int128_t>(argVectors, result, outputType, context);
                break;
            case OMNI_VARBINARY:
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                CompareString(argVectors, result, outputType, context);
                break;
            default:
                OMNI_THROW("LeastGreatest function Error", 
                        "Unsupported output type: " + TypeUtil::TypeToString(outputTypeId));
        }
    }
}

template<CompareMode Mode>
template<typename T>
void LeastGreatestFunction<Mode>::CompareNumeric(const std::vector<BaseVector *> &argVectors, 
    BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
    
    for (int32_t row = 0; row < size; ++row) {
        if (HasNullArg(argVectors, row)) {
            result->SetNull(row);
            continue;
        }
        bool hasNonNull = false;
        T bestValue{};
        
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                T value = GetValueFromVector<T>(argVectors[argIdx], row);
                if (!hasNonNull) {
                    bestValue = value;
                    hasNonNull = true;
                } else if (shouldReplace<Mode, T>(value, bestValue)) {
                    bestValue = value;
                }
            }
        }
        
        if (hasNonNull) {
            SetValueToVector(result, row, bestValue);
        } else {
            // All arguments are NULL
            result->SetNull(row);
        }
    }
}

template<CompareMode Mode>
void LeastGreatestFunction<Mode>::CompareString(const std::vector<BaseVector *> &argVectors, 
    BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
    
    for (int32_t row = 0; row < size; ++row) {
        if (HasNullArg(argVectors, row)) {
            result->SetNull(row);
            continue;
        }
        bool hasNonNull = false;
        std::string_view bestValue;
        
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                std::string_view value = GetStringValueFromVector(argVectors[argIdx], row);
                if (!hasNonNull) {
                    bestValue = value;
                    hasNonNull = true;
                } else {
                    bool replace = (Mode == CompareMode::GREATEST) ? (value > bestValue) : (value < bestValue);
                    if (replace) {
                        bestValue = value;
                    }
                }
            }
        }
        
        if (hasNonNull) {
            SetStringValueToVector(result, row, const_cast<std::string_view &>(bestValue));
        } else {
            // All arguments are NULL
            result->SetNull(row);
        }
    }
}

template<CompareMode Mode>
void LeastGreatestFunction<Mode>::CompareBoolean(const std::vector<BaseVector *> &argVectors, 
    BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
    
    for (int32_t row = 0; row < size; ++row) {
        if (HasNullArg(argVectors, row)) {
            result->SetNull(row);
            continue;
        }
        bool hasNonNull = false;
        bool bestValue = false;
        
        for (size_t argIdx = 0; argIdx < argVectors.size(); ++argIdx) {
            if (!argVectors[argIdx]->IsNull(row)) {
                bool value = GetValueFromVector<bool>(argVectors[argIdx], row);
                if (!hasNonNull) {
                    bestValue = value;
                    hasNonNull = true;
                } else {
                    // For greatest: true > false, so replace if value is true and current is false
                    // For least: false < true, so replace if value is false and current is true
                    bool replace = (Mode == CompareMode::GREATEST) ? 
                                   (value && !bestValue) : (!value && bestValue);
                    if (replace) {
                        bestValue = value;
                    }
                }
            }
        }
        
        if (hasNonNull) {
            SetValueToVector(result, row, bestValue);
        } else {
            // All arguments are NULL
            result->SetNull(row);
        }
    }
}

namespace {

template<typename TargetT, typename SourceT>
TargetT ReadConstValue(BaseVector *vec) {
    return static_cast<TargetT>(static_cast<ConstVector<SourceT> *>(vec)->GetConstValue());
}

template<typename TargetT, typename SourceT>
TargetT ReadFlatValue(BaseVector *vec, int32_t row) {
    return static_cast<TargetT>(static_cast<Vector<SourceT> *>(vec)->GetValue(row));
}

template<typename T>
T ReadWithTypePromotion(BaseVector *vec, int32_t row, Encoding encoding) {
    DataTypeId typeId = vec->GetTypeId();

    if constexpr (std::is_same_v<T, int128_t>) {
        switch (typeId) {
            case OMNI_DECIMAL128:
                if (encoding == OMNI_ENCODING_CONST) {
                    T result;
                    std::memcpy(&result,
                                reinterpret_cast<const char *>(vec) + sizeof(BaseVector),
                                sizeof(T));
                    return result;
                }
                return ReadFlatValue<T, int128_t>(vec, row);
            case OMNI_DECIMAL64:
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int64_t>(vec) : ReadFlatValue<T, int64_t>(vec, row);
            case OMNI_INT:
            case OMNI_DATE32:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int32_t>(vec) : ReadFlatValue<T, int32_t>(vec, row);
            case OMNI_SHORT:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int16_t>(vec) : ReadFlatValue<T, int16_t>(vec, row);
            case OMNI_BYTE:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int8_t>(vec) : ReadFlatValue<T, int8_t>(vec, row);
            default:
                break;
        }
    }
    if constexpr (std::is_same_v<T, int64_t>) {
        switch (typeId) {
            case OMNI_INT:
            case OMNI_DATE32:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int32_t>(vec) : ReadFlatValue<T, int32_t>(vec, row);
            case OMNI_SHORT:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int16_t>(vec) : ReadFlatValue<T, int16_t>(vec, row);
            case OMNI_BYTE:
                return (encoding == OMNI_ENCODING_CONST)
                    ? ReadConstValue<T, int8_t>(vec) : ReadFlatValue<T, int8_t>(vec, row);
            default:
                break;
        }
    }
    if constexpr (std::is_same_v<T, double>) {
        if (typeId == OMNI_FLOAT) {
            return (encoding == OMNI_ENCODING_CONST)
                ? ReadConstValue<T, float>(vec) : ReadFlatValue<T, float>(vec, row);
        }
    }

    if (encoding == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    }
    return static_cast<Vector<T> *>(vec)->GetValue(row);
}

} // anonymous namespace

template<CompareMode Mode>
template<typename T>
T LeastGreatestFunction<Mode>::GetValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST || encoding == OMNI_FLAT) {
        return ReadWithTypePromotion<T>(vec, row, encoding);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("LeastGreatest function Error", "Unsupported encoding type");
    }
}

template<CompareMode Mode>
std::string_view LeastGreatestFunction<Mode>::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
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
        OMNI_THROW("LeastGreatest function Error", "Unsupported encoding type for string");
    }
}

template<CompareMode Mode>
template<typename T>
void LeastGreatestFunction<Mode>::SetValueToVector(BaseVector *vec, int32_t row, const T &value) const {
    auto *resultVec = static_cast<Vector<T> *>(vec);
    resultVec->SetValue(row, value);
}

template<CompareMode Mode>
void LeastGreatestFunction<Mode>::SetStringValueToVector(BaseVector *vec, int32_t row, 
                                            std::string_view &value) const {
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}

// Factory functions
// inputArgs carries the call site argument types, so its size is the real arity of the call.
std::shared_ptr<VectorFunction> makeGreatest(const std::string &name, 
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &) {
    return std::make_shared<GreatestFunction>(inputArgs.size(), false);
}

std::shared_ptr<VectorFunction> makeLeast(const std::string &name, 
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &) {
    return std::make_shared<LeastFunction>(inputArgs.size(), false);
}

std::shared_ptr<VectorFunction> makeFlinkGreatest(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &) {
    return std::make_shared<GreatestFunction>(inputArgs.size(), true);
}

std::shared_ptr<VectorFunction> makeFlinkLeast(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &) {
    return std::make_shared<LeastFunction>(inputArgs.size(), true);
}

// Helper function to generate signatures for both least and greatest
static std::vector<std::shared_ptr<codegen::FunctionSignature>> GenerateSignatures(const std::string &funcName) {
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
        OMNI_VARBINARY
    };
    
    for (const auto &typeId : supportedTypes) {
        signatures.emplace_back(
            codegen::FunctionSignature::Variadic(funcName, typeId, typeId, 2));
    }
    
    return signatures;
}

std::vector<std::shared_ptr<codegen::FunctionSignature>> GreatestSignatures() {
    return GenerateSignatures("Greatest");
}

std::vector<std::shared_ptr<codegen::FunctionSignature>> LeastSignatures() {
    return GenerateSignatures("Least");
}

std::vector<std::shared_ptr<codegen::FunctionSignature>> FlinkGreatestSignatures() {
    return GenerateSignatures("flink_greatest");
}

std::vector<std::shared_ptr<codegen::FunctionSignature>> FlinkLeastSignatures() {
    return GenerateSignatures("flink_least");
}

// Explicit template instantiations for GREATEST
template class LeastGreatestFunction<CompareMode::GREATEST>;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<int8_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<int16_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<int32_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<int64_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<float>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<double>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::CompareNumeric<int128_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;

template int8_t LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<double>(BaseVector *, int32_t) const;
template int128_t LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<int128_t>(BaseVector *, int32_t) const;
template bool LeastGreatestFunction<CompareMode::GREATEST>::GetValueFromVector<bool>(BaseVector *, int32_t) const;

template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<int8_t>(BaseVector *, int32_t, const int8_t &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<int16_t>(BaseVector *, int32_t, const int16_t &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<int32_t>(BaseVector *, int32_t, const int32_t &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<int64_t>(BaseVector *, int32_t, const int64_t &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<int128_t>(BaseVector *, int32_t, const int128_t &) const;
template void LeastGreatestFunction<CompareMode::GREATEST>::SetValueToVector<bool>(BaseVector *, int32_t, const bool &) const;

// Explicit template instantiations for LEAST
template class LeastGreatestFunction<CompareMode::LEAST>;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<int8_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<int16_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<int32_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<int64_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<float>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<double>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;
template void LeastGreatestFunction<CompareMode::LEAST>::CompareNumeric<int128_t>(const std::vector<BaseVector *> &, BaseVector *&, const DataTypePtr &, ExecutionContext *) const;

template int8_t LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<double>(BaseVector *, int32_t) const;
template int128_t LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<int128_t>(BaseVector *, int32_t) const;
template bool LeastGreatestFunction<CompareMode::LEAST>::GetValueFromVector<bool>(BaseVector *, int32_t) const;

template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<int8_t>(BaseVector *, int32_t, const int8_t &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<int16_t>(BaseVector *, int32_t, const int16_t &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<int32_t>(BaseVector *, int32_t, const int32_t &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<int64_t>(BaseVector *, int32_t, const int64_t &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<int128_t>(BaseVector *, int32_t, const int128_t &) const;
template void LeastGreatestFunction<CompareMode::LEAST>::SetValueToVector<bool>(BaseVector *, int32_t, const bool &) const;

}
