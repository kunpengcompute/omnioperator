/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS JSON function implementation
 *
 * is_json_value/scalar/array/object(string) -> boolean. See IsJson.h for full semantics.
 */

#include "IsJson.h"

#include <memory>

namespace omniruntime::vectorization {

void IsJsonFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("IsJson function Error:", "Expected 1 argument");
    }

    auto *inputArg = args.top();
    args.pop();
    // inputArg ownership transfers here; wrap in unique_ptr so every exception path
    // (including GetStringValue's OMNI_THROW inside the row loop) releases it. Mirrors
    // the ArrayContainsFunction cleanup pattern but covers loop-body throws too.
    std::unique_ptr<BaseVector, void (*)(BaseVector *)> inputGuard(inputArg,
        [](BaseVector *p) { delete p; });

    int32_t rowSize = context->GetResultRowSize();
    if (result == nullptr) {
        result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowSize);
    }
    if (result == nullptr) {
        OMNI_THROW("IsJson function Error:", "Failed to create result vector");
    }
    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    if (resultVec == nullptr) {
        OMNI_THROW("IsJson function Error:", "Result vector is not a FlatVector<bool>");
    }

    rapidjson::Document doc;
    for (int32_t row = 0; row < rowSize; ++row) {
        // NULL input -> FALSE (Flink semantics: not NULL propagation). The result
        // type is BOOLEAN NOT NULL, so every row is explicitly non-NULL.
        if (inputArg->IsNull(row)) {
            resultVec->SetValue(row, false);
            result->SetNotNull(row);
            continue;
        }

        std::string_view jsonStr = GetStringValue(inputArg, row);
        // kParseNoFlags mirrors the default lenient parse used by FromJson/
        // JsonArrayLength; leading/trailing whitespace is tolerated (matching
        // Jackson's behaviour for IS JSON).
        doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());

        bool isMatch = false;
        if (!doc.HasParseError()) {
            isMatch = Classify(doc);
        }
        resultVec->SetValue(row, isMatch);
        result->SetNotNull(row);
    }

    // Normal path: release ownership back and delete explicitly to preserve the
    // original delete-inputArg contract observed by sibling JSON functions.
    inputGuard.reset();
}

bool IsJsonFunction::Classify(const rapidjson::Document &doc) const
{
    switch (type_) {
        case JsonType::VALUE:
            // Any valid JSON value (scalar, array, or object).
            return true;
        case JsonType::SCALAR:
            // Scalar = not object and not array (covers number/string/bool/null).
            // Mirrors Jackson: !(o instanceof Map) && !(o instanceof Collection).
            return !doc.IsObject() && !doc.IsArray();
        case JsonType::ARRAY:
            return doc.IsArray();
        case JsonType::OBJECT:
            return doc.IsObject();
        default:
            return false;
    }
}

std::string_view IsJsonFunction::GetStringValue(BaseVector *vector, int32_t row) const
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
            OMNI_THROW("IsJson function Error:", "Unsupported encoding type");
    }
}

} // namespace omniruntime::vectorization
