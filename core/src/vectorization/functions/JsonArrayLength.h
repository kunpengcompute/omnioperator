/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_array_length function for JSON operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"
#include "rapidjson/document.h"
#include <vector>
#include <string>
#include <string_view>
#include <limits>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class JsonArrayLengthFunction final : public VectorFunction {
public:
    explicit JsonArrayLengthFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        if (args.empty()) {
            OMNI_THROW("JsonArrayLengthFunction Error:", "No arguments provided");
        }

        auto *inputArg = args.top();
        args.pop();

        int32_t rowSize = context->GetResultRowSize();
        result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_INT, rowSize);
        if (!result) {
            OMNI_THROW("JsonArrayLengthFunction Error:", "Failed to create result vector");
        }

        auto *resultVec = dynamic_cast<Vector<int32_t> *>(result);
        if (!resultVec) {
            OMNI_THROW("JsonArrayLengthFunction Error:", "Result vector is not a FlatVector<int32_t>");
        }

        rapidjson::Document doc;

        for (int32_t row = 0; row < rowSize; ++row) {
            if (inputArg->IsNull(row)) {
                resultVec->SetNull(row);
                continue;
            }

            std::string_view jsonStr = GetStringValue(inputArg, row);
            doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());

            if (doc.HasParseError() || !doc.IsArray()) {
                resultVec->SetNull(row);
                continue;
            }

            int64_t arraySize = doc.Size();
            if (arraySize > std::numeric_limits<int32_t>::max()) {
                OMNI_THROW("JsonArrayLengthFunction Error:",
                    "Array size " + std::to_string(arraySize) + " exceeds int32_t range");
            }
            resultVec->SetValue(row, static_cast<int32_t>(arraySize));
            resultVec->SetNotNull(row);
        }

        delete inputArg;
    }

private:
    std::string_view GetStringValue(BaseVector *vector, int32_t row) const
    {
        switch (vector->GetEncoding()) {
            case OMNI_FLAT: {
                auto *stringVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
                return stringVector->GetValue(row);
            }
            case OMNI_DICTIONARY: {
                auto *dictVector =
                    static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector);
                return dictVector->GetValue(row);
            }
            case OMNI_ENCODING_CONST: {
                auto *constVector = static_cast<ConstVector<std::string_view> *>(vector);
                return constVector->GetConstValue();
            }
            default:
                OMNI_THROW("JsonArrayLengthFunction Error:", "Unsupported encoding type");
        }
    }
};
}
