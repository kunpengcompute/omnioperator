/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Spark ceil/ceiling on DECIMAL. Toward +inf, result scale is 0.
 */

#pragma once

#include "vectorization/VectorFunction.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "type/decimal128.h"

namespace omniruntime::vectorization {
class CeilDecimalFunction : public VectorFunction {
public:
    CeilDecimalFunction(const type::DataTypePtr &inputType, const type::DataTypePtr &outputType)
        : inputType_(inputType), outputType_(outputType) {}

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType, vec::BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        (void)outputType;
        auto input = args.top();
        args.pop();
        const int32_t size = context == nullptr ? input->GetSize() : context->GetResultRowSize();
        // Branch on the expression decimal type, same as omni_commercial_1230.
        // Decimal64 scan vectors are OMNI_LONG; the logical type is still DECIMAL64.
        const auto id = inputType_ == nullptr ? input->GetTypeId() : inputType_->GetId();
        const auto *inputDec = dynamic_cast<const type::DecimalDataType *>(inputType_.get());
        const auto *outputDec = dynamic_cast<const type::DecimalDataType *>(outputType_.get());
        int32_t scale = inputDec == nullptr ? 0 : inputDec->GetScale();
        int32_t precision = inputDec == nullptr ? 0 : inputDec->GetPrecision();
        if (scale == 0 && inputDec != nullptr && outputDec != nullptr &&
            inputDec->GetPrecision() > outputDec->GetPrecision()) {
            scale = inputDec->GetPrecision() - outputDec->GetPrecision() + 1;
        }
        int128_t factor = 1;
        for (int32_t i = 0; i < scale && factor <= (static_cast<int128_t>(1) << 126) / 10; ++i) {
            factor *= 10;
        }
        if (factor == 0) {
            factor = 1;
        }
        int64_t factorI64 = static_cast<int64_t>(factor);
        if (factorI64 == 0) {
            factorI64 = 1;
        }
        const bool isConst = input->GetEncoding() == vec::OMNI_ENCODING_CONST;

        result = vec::VectorHelper::CreateFlatVector(id == type::OMNI_LONG ? type::OMNI_DECIMAL64 : id, size);
        if (id == type::OMNI_DECIMAL64 || id == type::OMNI_LONG) {
            type::Decimal64DataType outType(precision, 0);
            vec::VectorHelper::SetVectorDataType(result, &outType);
            auto resultVec = static_cast<vec::Vector<int64_t> *>(result);
            for (int32_t row = 0; row < size; ++row) {
                if (input->IsNull(isConst ? 0 : row)) {
                    result->SetNull(row);
                    continue;
                }
                int64_t value = vec::VectorHelper::GetValueFromVector<int64_t>(input, row);
                int64_t quotient = value / factorI64;
                int64_t remainder = value % factorI64;
                if (remainder != 0 && value > 0) {
                    quotient += 1;
                }
                resultVec->SetValue(row, quotient);
            }
        } else {
            type::Decimal128DataType outType(precision, 0);
            vec::VectorHelper::SetVectorDataType(result, &outType);
            auto resultVec = static_cast<vec::Vector<type::Decimal128> *>(result);
            for (int32_t row = 0; row < size; ++row) {
                if (input->IsNull(isConst ? 0 : row)) {
                    result->SetNull(row);
                    continue;
                }
                int128_t value = vec::VectorHelper::GetValueFromVector<type::Decimal128>(input, row).ToInt128();
                int128_t quotient = value / factor;
                int128_t remainder = value % factor;
                if (remainder != 0 && value > 0) {
                    quotient += 1;
                }
                resultVec->SetValue(row, type::Decimal128(quotient));
            }
        }
        delete input;
    }

private:
    type::DataTypePtr inputType_;
    type::DataTypePtr outputType_;
};
}
