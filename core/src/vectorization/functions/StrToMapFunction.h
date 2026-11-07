/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/map_vector.h"
#include "vector/large_string_container.h"
#include <vector>
#include <string>
#include <string_view>
#include <unordered_set>
#include <memory>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class StrToMapFunction final : public VectorFunction {
public:
    explicit StrToMapFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override
    {
        auto kvDelimiterArg = args.top();
        args.pop();
        auto entryDelimiterArg = args.top();
        args.pop();
        auto inputArg = args.top();
        args.pop();

        // Delimiter checks and duplicate-key detection throw; guards release the args on those paths too.
        using VectorGuard = std::unique_ptr<BaseVector, void (*)(BaseVector *)>;
        auto deleter = [](BaseVector *v) {
            if (!v->GetIsField()) {
                delete v;
            }
        };
        VectorGuard kvDelimiterGuard(kvDelimiterArg, deleter);
        VectorGuard entryDelimiterGuard(entryDelimiterArg, deleter);
        VectorGuard inputGuard(inputArg, deleter);

        auto *entryDelimConst = dynamic_cast<ConstVector<std::string_view> *>(entryDelimiterArg);
        std::string_view entryDelimiter = entryDelimConst->GetConstValue();
        if (entryDelimiter.size() != 1) {
            OMNI_THROW("StrToMap Error:", "entryDelimiter's size should be 1.");
        }

        auto *kvDelimConst = dynamic_cast<ConstVector<std::string_view> *>(kvDelimiterArg);
        std::string_view kvDelimiter = kvDelimConst->GetConstValue();
        if (kvDelimiter.size() != 1) {
            OMNI_THROW("StrToMap Error:", "keyValueDelimiter's size should be 1.");
        }

        int32_t rowSize = inputArg->GetSize();
        std::vector<std::string_view> allKeys;
        std::vector<std::string_view> allValues;
        std::vector<bool> valueNulls;
        std::vector<int32_t> offsets = {0};
        std::vector<bool> rowNulls(rowSize);

        for (int32_t row = 0; row < rowSize; ++row) {
            if (inputArg->IsNull(row)) {
                rowNulls[row] = true;
                offsets.push_back(offsets.back());
                continue;
            }
            rowNulls[row] = false;

            std::string_view inputStr = GetStringValue(inputArg, row);
            std::unordered_set<std::string_view> keySet;
            size_t pos = 0;
            auto nextEntryPos = inputStr.find(entryDelimiter, pos);

            while (nextEntryPos != std::string_view::npos) {
                ProcessEntry(std::string_view(inputStr.data() + pos, nextEntryPos - pos),
                    kvDelimiter, keySet, allKeys, allValues, valueNulls);
                pos = nextEntryPos + 1;
                nextEntryPos = inputStr.find(entryDelimiter, pos);
            }

            ProcessEntry(std::string_view(inputStr.data() + pos, inputStr.size() - pos),
                kvDelimiter, keySet, allKeys, allValues, valueNulls);

            offsets.push_back(static_cast<int32_t>(allKeys.size()));
        }

        result = new MapVector(rowSize);
        auto *mapResult = static_cast<MapVector *>(result);

        auto keyVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(allKeys.size());
        auto valueVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(allValues.size());

        for (int32_t i = 0; i < static_cast<int32_t>(allKeys.size()); ++i) {
            keyVector->SetValue(i, allKeys[i]);
            if (valueNulls[i]) {
                valueVector->SetNull(i);
            } else {
                valueVector->SetValue(i, allValues[i]);
            }
        }

        mapResult->SetKeyVector(keyVector);
        mapResult->SetValueVector(valueVector);

        for (int32_t i = 0; i < static_cast<int32_t>(offsets.size()); ++i) {
            mapResult->SetOffset(i, offsets[i]);
        }

        for (int32_t row = 0; row < rowSize; ++row) {
            if (rowNulls[row]) {
                mapResult->SetNull(row);
            } else {
                mapResult->SetNotNull(row);
            }
        }
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
                auto *dictVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(
                    vector);
                return dictVector->GetValue(row);
            }
            case OMNI_ENCODING_CONST: {
                auto *constVector = static_cast<ConstVector<std::string_view> *>(vector);
                return constVector->GetConstValue();
            }
            default: OMNI_THROW("StrToMap Error:", "Unsupported encoding type");
        }
    }

    void ProcessEntry(std::string_view entry, std::string_view kvDelimiter,
        std::unordered_set<std::string_view> &keySet,
        std::vector<std::string_view> &allKeys,
        std::vector<std::string_view> &allValues,
        std::vector<bool> &valueNulls) const
    {
        auto delimiterPos = entry.find(kvDelimiter, 0);
        if (delimiterPos == std::string_view::npos) {
            auto insertResult = keySet.insert(entry);
            if (!insertResult.second) {
                OMNI_THROW("StrToMap Error:", "Duplicate keys are not allowed.");
            }
            allKeys.push_back(entry);
            allValues.push_back(std::string_view());
            valueNulls.push_back(true);
            return;
        }

        std::string_view key(entry.data(), delimiterPos);
        auto insertResult = keySet.insert(key);
        if (!insertResult.second) {
            OMNI_THROW("StrToMap Error:", "Duplicate keys are not allowed.");
        }

        std::string_view value(entry.data() + delimiterPos + 1, entry.size() - delimiterPos - 1);
        allKeys.push_back(key);
        allValues.push_back(value);
        valueNulls.push_back(false);
    }
};
}
