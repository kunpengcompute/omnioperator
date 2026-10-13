/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_object_keys function for JSON operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "util/debug.h"
#include "rapidjson/document.h"
#include <vector>
#include <string>
#include <string_view>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class JsonObjectKeysFunction final : public VectorFunction {
public:
    explicit JsonObjectKeysFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        auto *inputArg = args.top();
        args.pop();

        int32_t rowSize = inputArg->GetSize();
        auto *arrayResult = new ArrayVector(rowSize, context->GetPool());
        result = arrayResult;

        std::vector<std::string> allKeys;
        std::vector<int64_t> offsets = {0};
        std::vector<bool> validRows(rowSize, false);
        rapidjson::Document doc;

        for (int32_t row = 0; row < rowSize; ++row) {
            if (inputArg->IsNull(row)) {
                arrayResult->SetNull(row);
                offsets.push_back(offsets.back());
                continue;
            }

            std::string_view jsonStr = GetStringValue(inputArg, row);
            doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());

            if (doc.HasParseError() || !doc.IsObject()) {
                arrayResult->SetNull(row);
                offsets.push_back(offsets.back());
                continue;
            }

            validRows[row] = true;
            int64_t currentOffset = offsets.back();
            int64_t keyCount = 0;
            for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
                allKeys.emplace_back(it->name.GetString(), it->name.GetStringLength());
                ++keyCount;
            }
            offsets.push_back(currentOffset + keyCount);
        }

        auto elementVector =
            std::make_shared<Vector<LargeStringContainer<std::string_view>>>(allKeys.size());
        for (size_t i = 0; i < allKeys.size(); ++i) {
            std::string_view sv(allKeys[i]);
            elementVector->SetValue(i, sv);
        }

        for (size_t i = 0; i < offsets.size(); ++i) {
            arrayResult->SetOffset(i, offsets[i]);
        }
        arrayResult->SetElementVector(elementVector);

        for (int32_t row = 0; row < rowSize; ++row) {
            if (validRows[row]) {
                arrayResult->SetNotNull(row);
            }
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
                OMNI_THROW("JsonObjectKeysFunction Error:", "Unsupported encoding type");
        }
    }
};
}
