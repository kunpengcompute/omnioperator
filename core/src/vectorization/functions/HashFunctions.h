/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "util/compiler_util.h"
#include <iostream>
#include "codegen/bloom_filter.h"
#include "codegen/functions/murmur3_hash.h"
#include "type/decimal128.h"
#include "../registration/SimpleFunctionRegistry.h"
#include "codegen/functions/xxhash64_hash.h"
#include "operator/util/mm3_util.h"

namespace omniruntime::vectorization {

class MurMur3HashFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        auto seedVec = args.top();
        args.pop();
        auto valVec = args.top();
        args.pop();
        const auto size = context->GetResultRowSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        auto resultVector = reinterpret_cast<Vector<int32_t> *>(result);

        const auto valTypeId = valVec->GetTypeId();
        for (size_t i = 0; i < size; i++) {
            resultVector->SetNotNull(i);
            if (valVec->IsNull(i)) {
                resultVector->SetValue(i, VectorHelper::GetValueFromVector<int32_t>(seedVec, i));
                continue;
            }
            switch (valTypeId) {
                case OMNI_BOOLEAN:
                    resultVector->SetValue(i, codegen::function::Mm3Boolean(VectorHelper::GetValueFromVector<bool>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_BYTE:
                    resultVector->SetValue(i, codegen::function::Mm3Int8(VectorHelper::GetValueFromVector<int8_t>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_SHORT:
                    resultVector->SetValue(i, codegen::function::Mm3Int16(VectorHelper::GetValueFromVector<int16_t>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                    resultVector->SetValue(i, codegen::function::Mm3Int32(VectorHelper::GetValueFromVector<int32_t>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    resultVector->SetValue(i, codegen::function::Mm3Int64(VectorHelper::GetValueFromVector<int64_t>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_FLOAT:
                    resultVector->SetValue(i, codegen::function::Mm3Float(VectorHelper::GetValueFromVector<float>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_DOUBLE:
                    resultVector->SetValue(i, codegen::function::Mm3Double(VectorHelper::GetValueFromVector<double>(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_CHAR:
                case OMNI_VARCHAR:
                case OMNI_VARBINARY:
                    resultVector->SetValue(i, codegen::function::Mm3String1(VectorHelper::GetStringValueFromVector(valVec, i), false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                case OMNI_DECIMAL128: {
                    Decimal128 val = VectorHelper::GetValueFromVector<Decimal128>(valVec, i);
                    resultVector->SetValue(i, codegen::function::Mm3Decimal128(val.HighBits(), val.LowBits(), 0, 0, false, VectorHelper::GetValueFromVector<int32_t>(seedVec, i), false));
                    break;
                }
                case OMNI_ARRAY: {
                    auto *arrayVec = reinterpret_cast<vec::ArrayVector *>(valVec);
                    resultVector->SetValue(i, static_cast<int32_t>(op::HashArrayAtRow(
                        arrayVec, static_cast<int64_t>(i), static_cast<uint32_t>(VectorHelper::GetValueFromVector<int32_t>(seedVec, i)))));
                    break;
                }
                case OMNI_ROW: {
                    auto *rowVec = reinterpret_cast<vec::RowVector *>(valVec);
                    resultVector->SetValue(i, static_cast<int32_t>(op::HashStructAtRow(
                        rowVec, static_cast<int64_t>(i), static_cast<uint32_t>(VectorHelper::GetValueFromVector<int32_t>(seedVec, i)))));
                    break;
                }
            }
        }
        delete seedVec;
        delete valVec;
    }

};

void RegisterMurMur3HashFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_BOOLEAN, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_FLOAT, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DOUBLE, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL64, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL128, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARBINARY, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_ARRAY, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_ROW, OMNI_INT}, OMNI_INT, std::make_shared<MurMur3HashFunction>());
}

class XxHash64Function : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        auto seedVec = args.top();
        args.pop();
        auto valVec = args.top();
        args.pop();
        const auto size = context->GetResultRowSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        auto resultVector = reinterpret_cast<Vector<int64_t> *>(result);

        const auto valTypeId = valVec->GetTypeId();
        for (size_t i = 0; i < size; i++) {
            resultVector->SetNotNull(i);
            if (valVec->IsNull(i)) {
                resultVector->SetValue(i, VectorHelper::GetValueFromVector<int64_t>(seedVec, i));
                continue;
            }
            switch (valTypeId) {
                case OMNI_BOOLEAN:
                    resultVector->SetValue(i, codegen::function::XxH64Boolean(VectorHelper::GetValueFromVector<bool>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_BYTE:
                    resultVector->SetValue(i, codegen::function::XxH64Int8(VectorHelper::GetValueFromVector<int8_t>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_SHORT:
                    resultVector->SetValue(i, codegen::function::XxH64Int16(VectorHelper::GetValueFromVector<int16_t>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                    resultVector->SetValue(i, codegen::function::XxH64Int32(VectorHelper::GetValueFromVector<int32_t>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    resultVector->SetValue(i, codegen::function::XxH64Int64(VectorHelper::GetValueFromVector<int64_t>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_FLOAT:
                    resultVector->SetValue(i, codegen::function::XxH64Float(VectorHelper::GetValueFromVector<float>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_DOUBLE:
                    resultVector->SetValue(i, codegen::function::XxH64Double(VectorHelper::GetValueFromVector<double>(valVec, i), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                case OMNI_CHAR:
                case OMNI_VARCHAR:
                case OMNI_VARBINARY: {
                    std::string_view valString = VectorHelper::GetStringValueFromVector(valVec, i);
                    resultVector->SetValue(i, codegen::function::XxH64String(valString.data(), valString.size(), false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                }
                case OMNI_DECIMAL128: {
                    Decimal128 val = VectorHelper::GetValueFromVector<Decimal128>(valVec, i);
                    resultVector->SetValue(i, codegen::function::XxH64Decimal128(val.HighBits(), val.LowBits(), 0, 0, false, VectorHelper::GetValueFromVector<int64_t>(seedVec, i), false));
                    break;
                }
            }
        }
        delete seedVec;
        delete valVec;
    }
};

void RegisterXxHash64Function(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_BOOLEAN, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_FLOAT, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DOUBLE, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL64, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL128, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARBINARY, OMNI_LONG}, OMNI_LONG, std::make_shared<XxHash64Function>());
}

class MightContainFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {

        auto hashVec = args.top();
        args.pop();
        auto bloomFilterAddrVec = args.top();
        args.pop();

        const auto size = context->GetResultRowSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }
        auto resultVector = reinterpret_cast<Vector<bool> *>(result);

        // SIMD
        if (TryMightContainBatch(bloomFilterAddrVec, hashVec, resultVector, size)) {
            delete hashVec;
            delete bloomFilterAddrVec;
            return;
        }

        // Scalar
        for (size_t i = 0; i < size; ++i) {
            resultVector->SetNotNull(i);
            if (bloomFilterAddrVec->IsNull(i) || hashVec->IsNull(i)) {
                resultVector->SetValue(i, false);
                continue;
            }

            auto bloomFilterAddr = VectorHelper::GetValueFromVector<int64_t>(bloomFilterAddrVec, i);
            if (bloomFilterAddr == 0) {
                resultVector->SetValue(i, false);
                continue;
            }

            auto bloomFilter = reinterpret_cast<op::BloomFilter *>(bloomFilterAddr);
            auto hashValue = VectorHelper::GetValueFromVector<int64_t>(hashVec, i);
            resultVector->SetValue(i, bloomFilter->MightContainLong(hashValue));
        }

        delete hashVec;
        delete bloomFilterAddrVec;
    }

private:
    bool TryMightContainBatch(BaseVector *bloomFilterAddrVec, BaseVector *hashVec, Vector<bool> *resultVector,
        size_t size) const
    {
        if (size == 0 || bloomFilterAddrVec->HasNull() || hashVec->HasNull() ||
            hashVec->GetEncoding() != OMNI_FLAT || resultVector->GetEncoding() != OMNI_FLAT) {
            return false;
        }

        auto *hashValues = reinterpret_cast<int64_t *>(VectorHelper::UnsafeGetValues(hashVec));
        auto *resultValues = reinterpret_cast<bool *>(VectorHelper::UnsafeGetValues(resultVector));
        if (hashValues == nullptr || resultValues == nullptr) {
            return false;
        }

        int64_t bloomFilterAddr = VectorHelper::GetValueFromVector<int64_t>(bloomFilterAddrVec, 0);
        if (bloomFilterAddr == 0) {
            return false;
        }

        if (bloomFilterAddrVec->GetEncoding() != OMNI_ENCODING_CONST) {
            for (size_t i = 1; i < size; i++) {
                if (VectorHelper::GetValueFromVector<int64_t>(bloomFilterAddrVec, i) != bloomFilterAddr) {
                    return false;
                }
            }
        }

        auto bloomFilter = reinterpret_cast<op::BloomFilter *>(bloomFilterAddr);
        if (bloomFilter->GetVersion() != op::BloomFilter::VERSION) {
            return false;
        }

        resultVector->SetNulls(0, false, static_cast<int32_t>(size));
        resultVector->SetNullFlag(false);
        bloomFilter->MightContainLongBatch(hashValues, resultValues, static_cast<int32_t>(size));
        return true;
    }
};

void RegisterMightContainFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_LONG}, OMNI_BOOLEAN,
        std::make_shared<MightContainFunction>());
}
}
