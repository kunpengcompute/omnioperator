/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayJoin function implementation
 */

#include "ArrayJoin.h"
#include "type/decimal128.h"
#include "type/decimal_operations.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

std::string ArrayJoinImpl::GetStringValue(BaseVector *vec, int32_t row) const
{
    if (vec == nullptr) {
        return "";
    }
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        if (vec->IsNull(0)) {
            return "";
        }
        auto *constVec = dynamic_cast<ConstVector<std::string_view> *>(vec);
        if (constVec != nullptr) {
            std::string_view sv = constVec->GetConstValue();
            return std::string(sv.data(), sv.size());
        }
    } else {
        if (vec->IsNull(row)) {
            return "";
        }
        auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        if (strVec != nullptr) {
            std::string_view sv = strVec->GetValue(row);
            return std::string(sv.data(), sv.size());
        }
    }
    return "";
}

std::string ArrayJoinImpl::ElementToString(BaseVector *elementVector, int64_t index) const
{
    DataTypeId typeId = elementVector->GetTypeId();
    int32_t idx = static_cast<int32_t>(index);

    switch (typeId) {
        case OMNI_BYTE: {
            auto *vec = dynamic_cast<Vector<int8_t> *>(elementVector);
            return std::to_string(vec->GetValue(idx));
        }
        case OMNI_SHORT: {
            auto *vec = dynamic_cast<Vector<int16_t> *>(elementVector);
            return std::to_string(vec->GetValue(idx));
        }
        case OMNI_INT: {
            auto *vec = dynamic_cast<Vector<int32_t> *>(elementVector);
            return std::to_string(vec->GetValue(idx));
        }
        case OMNI_DATE32: {
            auto *vec = dynamic_cast<Vector<int32_t> *>(elementVector);
            int32_t dateVal = vec->GetValue(idx);
            Date32 date(dateVal);
            char buf[64] = {0};
            date.ToString(buf, sizeof(buf));
            return std::string(buf);
        }
        case OMNI_LONG: {
            auto *vec = dynamic_cast<Vector<int64_t> *>(elementVector);
            return std::to_string(vec->GetValue(idx));
        }
        case OMNI_TIMESTAMP: {
            auto *vec = dynamic_cast<Vector<int64_t> *>(elementVector);
            int64_t tsVal = vec->GetValue(idx);
            int64_t seconds = tsVal / 1000000;
            uint64_t nanos = static_cast<uint64_t>((tsVal % 1000000) * 1000);
            if (tsVal < 0 && nanos != 0) {
                seconds -= 1;
                nanos = 1000000000ULL - nanos;
            }
            Timestamp ts(seconds, nanos);
            TimestampToStringOptions options;
            options.precision = TimestampToStringOptions::Precision::kMilliseconds;
            char startPosition[64] = {0};
            std::string_view sv = Timestamp::tsToStringView(ts, options, startPosition);
            return std::string(sv.data(), sv.size());
        }
        case OMNI_FLOAT: {
            auto *vec = dynamic_cast<Vector<float> *>(elementVector);
            float val = vec->GetValue(idx);
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }
        case OMNI_DOUBLE: {
            auto *vec = dynamic_cast<Vector<double> *>(elementVector);
            double val = vec->GetValue(idx);
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }
        case OMNI_BOOLEAN: {
            auto *vec = dynamic_cast<Vector<bool> *>(elementVector);
            return vec->GetValue(idx) ? "true" : "false";
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            auto *vec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elementVector);
            std::string_view sv = vec->GetValue(idx);
            return std::string(sv.data(), sv.size());
        }
        case OMNI_DECIMAL64: {
            auto *vec = dynamic_cast<Vector<int64_t> *>(elementVector);
            return std::to_string(vec->GetValue(idx));
        }
        case OMNI_DECIMAL128: {
            auto *vec = dynamic_cast<Vector<Decimal128> *>(elementVector);
            Decimal128 val = vec->GetValue(idx);
            return val.ToString();
        }
        default:
            OMNI_THROW("ArrayJoin error:",
                "Unsupported element type: " + std::to_string(typeId));
    }
    return "";
}

void ArrayJoinImpl::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    bool hasNullReplacement = (args.size() >= 3);

    BaseVector *nullReplacementArg = nullptr;
    if (hasNullReplacement) {
        nullReplacementArg = args.top();
        args.pop();
    }
    auto nullReplacementGuard = MakeVectorArgGuard(nullReplacementArg);

    auto *delimiterArg = args.top();
    args.pop();
    auto delimiterGuard = MakeVectorArgGuard(delimiterArg);
    auto *arrayArg = args.top();
    args.pop();
    auto arrayGuard = MakeVectorArgGuard(arrayArg);

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        OMNI_THROW("ArrayJoin error:", "First argument must be ARRAY type");
    }

    int32_t rowSize = context->GetResultRowSize();
    auto *resultVector = VectorHelper::CreateStringVector(rowSize);
    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultVector);

    auto inputElementVector = arrayVec->GetElementVector();

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            strResult->SetNull(row);
            continue;
        }

        std::string delimiter = GetStringValue(delimiterArg, row);

        std::string nullReplacement;
        bool useNullReplacement = false;
        if (hasNullReplacement && nullReplacementArg != nullptr) {
            bool isNullRepl = false;
            if (nullReplacementArg->GetEncoding() == OMNI_ENCODING_CONST) {
                isNullRepl = nullReplacementArg->IsNull(0);
            } else {
                isNullRepl = nullReplacementArg->IsNull(row);
            }
            if (!isNullRepl) {
                nullReplacement = GetStringValue(nullReplacementArg, row);
                useNullReplacement = true;
            }
        }

        int64_t arraySize = arrayVec->GetSize(row);
        int64_t arrayOffset = arrayVec->GetOffset(row);

        std::string joined;
        bool firstNonNull = true;

        for (int64_t i = 0; i < arraySize; ++i) {
            int64_t elemIdx = arrayOffset + i;
            bool elemIsNull = (inputElementVector == nullptr) || inputElementVector->IsNull(static_cast<int32_t>(elemIdx));

            if (elemIsNull) {
                if (useNullReplacement) {
                    if (!firstNonNull) {
                        joined += delimiter;
                    }
                    joined += nullReplacement;
                    firstNonNull = false;
                }
            } else {
                if (!firstNonNull) {
                    joined += delimiter;
                }
                joined += ElementToString(inputElementVector.get(), elemIdx);
                firstNonNull = false;
            }
        }

        strResult->SetNotNull(row);
        std::string_view sv(joined);
        strResult->SetValue(row, sv);
    }

    result = strResult;
}

} // namespace omniruntime::vectorization
