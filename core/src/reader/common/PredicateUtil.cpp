/**
* Copyright (C) 2025-2025. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PredicateUtil.h"
#include <algorithm>
#include <vector>
#include <nlohmann/json.hpp>
#include "../common/JulianGregorianRebase.h"
#include "../common/TimeRebaseInfo.h"

using namespace omniruntime::vec;
static common::JulianGregorianRebaseDays g_dateRebaseDays;

namespace common {
    std::unique_ptr<PredicateCondition> buildLeafPredicateCondition(PredicateOperatorType &op,
                                                                    nlohmann::json &jsonCondition,
                                                                    int32_t columnCount) {
        using namespace omniruntime::type;
        int32_t index = jsonCondition["index"].get<int32_t>();
        std::string value = jsonCondition["value"];
        DataTypeId typeId = jsonCondition["dataType"].get<DataTypeId>();
        if (index >= columnCount) {
            if (op == IS_NULL) {
                return std::make_unique<LeafPredicateCondition<int8_t>>(TRUE, 0, 1);
            } else {
                return std::make_unique<LeafPredicateCondition<int8_t>>(FALSE, 0, 0);
            }
        }
        switch (typeId) {
            case OMNI_SHORT: {
                return std::make_unique<LeafPredicateCondition<int16_t>>(op, index, static_cast<int16_t>(stoi(value)));
            }
            case OMNI_BYTE: {
                return std::make_unique<LeafPredicateCondition<int8_t>>(op, index, static_cast<int8_t>(stoi(value)));
            }
            case OMNI_INT: {
                return std::make_unique<LeafPredicateCondition<int32_t>>(op, index, stoi(value));
            }
            case OMNI_LONG: {
                return std::make_unique<LeafPredicateCondition<int64_t>>(op, index, stol(value));
            }
            case OMNI_DOUBLE: {
                return std::make_unique<LeafPredicateCondition<double>>(op, index, stod(value));
            }
            case OMNI_DATE32: {
                return std::make_unique<LeafPredicateCondition<int32_t>>(op, index, stoi(value));
            }
            case OMNI_BOOLEAN: {
                return std::make_unique<LeafPredicateCondition<int8_t>>(op, index, value == "true" ? 1 : 0);
            }
            default: {
                throw OmniException("OPERATOR_RUNTIME_ERROR", "buildLeafPredicateCondition UnSupport DataTypeId: " + std::to_string(typeId));
            }
        }
    }

    std::unique_ptr<PredicateCondition> buildLeafPredicateConditionWithRebase(
            PredicateOperatorType &op,
            nlohmann::json &jsonCondition,
            int32_t columnCount,
            const std::unique_ptr<common::TimeRebaseInfo>& rebaseInfo
    ) {
        using namespace omniruntime::type;
        int32_t index = jsonCondition["index"].get<int32_t>();
        std::string value = jsonCondition["value"];
        DataTypeId typeId = jsonCondition["dataType"].get<DataTypeId>();

        if (index >= columnCount) {
            if (op == IS_NULL) {
                return std::make_unique<LeafPredicateCondition<int8_t>>(TRUE, 0, 1);
            } else {
                return std::make_unique<LeafPredicateCondition<int8_t>>(FALSE, 0, 0);
            }
        }

        switch (typeId) {
            case OMNI_SHORT: {
                return std::make_unique<LeafPredicateCondition<int16_t>>(op, index, static_cast<int16_t>(stoi(value)));
            }
            case OMNI_BYTE: {
                return std::make_unique<LeafPredicateCondition<int8_t>>(op, index, static_cast<int8_t>(stoi(value)));
            }
            case OMNI_INT: {
                return std::make_unique<LeafPredicateCondition<int32_t>>(op, index, stoi(value));
            }
            case OMNI_LONG: {
                return std::make_unique<LeafPredicateCondition<int64_t>>(op, index, stol(value));
            }
            case OMNI_DOUBLE: {
                return std::make_unique<LeafPredicateCondition<double>>(op, index, stod(value));
            }
            case OMNI_DATE32: {
                int32_t epochDay = stoi(value);
                if (rebaseInfo != nullptr) {
                    epochDay = g_dateRebaseDays.RebaseJulianToGregorianDays(epochDay);
                }
                return std::make_unique<LeafPredicateCondition<int32_t>>(op, index, epochDay);
            }
            case OMNI_BOOLEAN: {
                return std::make_unique<LeafPredicateCondition<int8_t>>(op, index, value == "true" ? 1 : 0);
            }
            default: {
                throw OmniException("OPERATOR_RUNTIME_ERROR", "buildLeafPredicateConditionWithRebase UnSupport DataTypeId: " + std::to_string(typeId));
            }
        }
    }

    std::unique_ptr<PredicateCondition> buildPredicateCondition(nlohmann::json &jsonCondition, int32_t columnCount) {
        PredicateOperatorType op = jsonCondition["op"].get<PredicateOperatorType>();
        switch (op) {
            case TRUE:
            case FALSE:
            case EQUAL_TO:
            case GREATER_THAN:
            case GREATER_THAN_OR_EQUAL:
            case LESS_THAN:
            case LESS_THAN_OR_EQUAL:
            case IS_NOT_NULL:
            case IS_NULL: {
                return buildLeafPredicateCondition(op, jsonCondition, columnCount);
            }
            case OR: {
                auto orLeft = buildPredicateCondition(jsonCondition["left"], columnCount);
                auto orRight = buildPredicateCondition(jsonCondition["right"], columnCount);
                return std::make_unique<OrPredicateCondition>(std::move(orLeft), std::move(orRight));
            }
            case AND: {
                auto andLeft = buildPredicateCondition(jsonCondition["left"], columnCount);
                auto andRight = buildPredicateCondition(jsonCondition["right"], columnCount);
                return std::make_unique<AndPredicateCondition>(std::move(andLeft), std::move(andRight));
            }
            case NOT: {
                auto child = buildPredicateCondition(jsonCondition["child"], columnCount);
                return std::make_unique<NotPredicateCondition>(std::move(child));
            }
            default: {
                throw OmniException("OPERATOR_RUNTIME_ERROR", "buildPredicateCondition UnSupport PredicateOperatorType: " + std::to_string(op));
            }
        }
    }

    std::unique_ptr<PredicateCondition> buildPredicateConditionWithRebase(
            nlohmann::json &jsonCondition,
            int32_t columnCount,
            const std::unique_ptr<common::TimeRebaseInfo>& rebaseInfo
    ) {
        PredicateOperatorType op = jsonCondition["op"].get<PredicateOperatorType>();
        switch (op) {
            case TRUE:
            case FALSE:
            case EQUAL_TO:
            case GREATER_THAN:
            case GREATER_THAN_OR_EQUAL:
            case LESS_THAN:
            case LESS_THAN_OR_EQUAL:
            case IS_NOT_NULL:
            case IS_NULL: {
                return buildLeafPredicateConditionWithRebase(op, jsonCondition, columnCount, rebaseInfo);
            }
            case OR: {
                auto orLeft = buildPredicateConditionWithRebase(jsonCondition["left"], columnCount, rebaseInfo);
                auto orRight = buildPredicateConditionWithRebase(jsonCondition["right"], columnCount, rebaseInfo);
                return std::make_unique<OrPredicateCondition>(std::move(orLeft), std::move(orRight));
            }
            case AND: {
                auto andLeft = buildPredicateConditionWithRebase(jsonCondition["left"], columnCount, rebaseInfo);
                auto andRight = buildPredicateConditionWithRebase(jsonCondition["right"], columnCount, rebaseInfo);
                return std::make_unique<AndPredicateCondition>(std::move(andLeft), std::move(andRight));
            }
            case NOT: {
                auto child = buildPredicateConditionWithRebase(jsonCondition["child"], columnCount, rebaseInfo);
                return std::make_unique<NotPredicateCondition>(std::move(child));
            }
            default: {
                throw OmniException("OPERATOR_RUNTIME_ERROR", "buildPredicateConditionWithRebase UnSupport PredicateOperatorType: " + std::to_string(op));
            }
        }
    }

    std::unique_ptr<PredicateCondition> BuildVecPredicateCondition(nlohmann::json &json, int32_t columnCount) {
        if (!json.contains("vecPredicateCondition")) {
            return nullptr;
        }

        const auto& conditionField = json["vecPredicateCondition"];
        std::string conditionStr = conditionField.get<std::string>();
        auto jsonCondition = nlohmann::json::parse(conditionStr);
        auto predicate = buildPredicateCondition(jsonCondition, columnCount);
        predicate->buildNullColumns(columnCount);
        return predicate;
    }

    std::unique_ptr<PredicateCondition> BuildResidualPredicateCondition(nlohmann::json &jsonCondition,
                                                                        int32_t columnCount) {
        auto predicate = buildPredicateCondition(jsonCondition, columnCount);
        predicate->buildNullColumns(columnCount);
        return predicate;
    }

    std::unique_ptr<PredicateCondition> BuildVecPredicateConditionWithRebase(
            nlohmann::json &json,
            int32_t columnCount,
            std::unique_ptr<common::TimeRebaseInfo> rebaseInfo
    ) {
        if (!json.contains("vecPredicateCondition")) {
            return nullptr;
        }

        const auto& conditionField = json["vecPredicateCondition"];
        std::string conditionStr = conditionField.get<std::string>();
        auto jsonCondition = nlohmann::json::parse(conditionStr);
        auto predicate = buildPredicateConditionWithRebase(jsonCondition, columnCount, rebaseInfo);
        predicate->buildNullColumns(columnCount);
        return predicate;
    }

    struct BitMaskIndex {
        BitMaskIndex() {
            for (int i = 0; i < (1 << N); i++) {
                int32_t startIndex = i * (N + 1);
                int32_t index = startIndex;
                for (int bit = 0; bit < N; bit++) {
                    if (i & (1 << bit)) {
                        memo_[++index] = bit;
                    }
                }
                memo_[startIndex] = index - startIndex;
            }
        }

        const inline uint8_t* operator[](size_t i) const {
            return memo_ + (i * (N + 1));
        }

    private:
        static constexpr int N = 8;
        uint8_t memo_[(1 << N) * (N + 1)]{0};
    };

    const BitMaskIndex bitMaskIndex;
    const int32_t batchStep = 8;

    template <typename FlatVector, typename RAW_DATA_TYPE>
    void SetFlatVectorValue(int32_t rowCount, BaseVector *baseVector, BaseVector *selectedBaseVector,
        const uint8_t *bitMark, bool isAllNull, bool isAllNotNull)
    {
        int32_t index = 0;
        int32_t j = 0;
        uint8_t mask;
        // 该列过滤出来都是全部为空的情况
        if (isAllNull) {
            auto *nulls = reinterpret_cast<uint8_t *>(UnsafeBaseVector::GetNulls(selectedBaseVector));
            memset(nulls, -1, BitUtil::Nbytes(selectedBaseVector->GetSize()));
            return;
        }
        // 该列过滤出来都是全部不为空的情况
        if (isAllNotNull) {
            auto *srcValues = UnsafeVector::GetRawValues(static_cast<FlatVector *>(baseVector));
            auto *destValues = UnsafeVector::GetRawValues(static_cast<FlatVector *>(selectedBaseVector));
            for (; j + batchStep <= rowCount; j += batchStep) {
                mask = bitMark[j >> 3];
                if (mask == 0) {
                    continue;
                }
                if (mask == 255) {
                    memcpy(destValues + index, srcValues + j, batchStep * sizeof(RAW_DATA_TYPE));
                    index += batchStep;
                    continue;
                }
                const uint8_t *maskArr = bitMaskIndex[mask];
                for (int i = 1; i <= *maskArr; i++) {
                    auto offset = j + maskArr[i];
                    auto value = static_cast<FlatVector *>(baseVector)->GetValue(offset);
                    static_cast<FlatVector *>(selectedBaseVector)->SetValue(index++, value);
                }
            }
            for (; j < rowCount; j++) {
                if (BitUtil::IsBitSet(bitMark, j)) {
                    auto value = static_cast<FlatVector *>(baseVector)->GetValue(j);
                    static_cast<FlatVector *>(selectedBaseVector)->SetValue(index++, value);
                }
            }
            return;
        }
        // 其他情况
        for (; j + batchStep <= rowCount; j += batchStep) {
            mask = bitMark[j >> 3];
            if (mask == 0) {
                continue;
            }
            const uint8_t *maskArr = bitMaskIndex[mask];
            for (int i = 1; i <= *maskArr; i++) {
                auto offset = j + maskArr[i];
                if (UNLIKELY(baseVector->IsNull(offset))) {
                    static_cast<FlatVector *>(selectedBaseVector)->SetNull(index++);
                } else {
                    auto value = static_cast<FlatVector *>(baseVector)->GetValue(offset);
                    static_cast<FlatVector *>(selectedBaseVector)->SetValue(index++, value);
                }
            }
        }
        for (; j < rowCount; j++) {
            if (BitUtil::IsBitSet(bitMark, j)) {
                if (UNLIKELY(baseVector->IsNull(j))) {
                    static_cast<FlatVector *>(selectedBaseVector)->SetNull(index++);
                } else {
                    auto value = static_cast<FlatVector *>(baseVector)->GetValue(j);
                    static_cast<FlatVector *>(selectedBaseVector)->SetValue(index++, value);
                }
            }
        }
    }

    template <typename VectorType>
    void SetStringVectorValue(int32_t rowCount, VectorType *baseVector,
        Vector<LargeStringContainer<std::string_view>> *selectedBaseVector, const uint8_t *bitMark, bool isAllNull,
        bool isAllNotNull)
    {
        int32_t index = 0;
        int32_t j = 0;
        uint8_t mask;
        // 该列过滤出来都是全部为空的情况
        if (isAllNull) {
            for (; j < selectedBaseVector->GetSize(); j++) {
                selectedBaseVector->SetNull(j);
            }
            return;
        }
        // 该列过滤出来都是全部不为空的情况
        if (isAllNotNull) {
            for (; j + batchStep <= rowCount; j += batchStep) {
                mask = bitMark[j >> 3];
                if (mask == 0) {
                    continue;
                }
                const uint8_t *maskArr = bitMaskIndex[mask];
                for (int i = 1; i <= *maskArr; i++) {
                    auto offset = j + maskArr[i];
                    auto value = baseVector->GetValue(offset);
                    selectedBaseVector->SetValue(index++, value);
                }
            }
            for (; j < rowCount; j++) {
                if (BitUtil::IsBitSet(bitMark, j)) {
                    auto value = baseVector->GetValue(j);
                    selectedBaseVector->SetValue(index++, value);
                }
            }
            return;
        }
        // 其他情况
        for (; j + batchStep <= rowCount; j += batchStep) {
            mask = bitMark[j >> 3];
            if (mask == 0) {
                continue;
            }
            const uint8_t *maskArr = bitMaskIndex[mask];
            for (int i = 1; i <= *maskArr; i++) {
                auto offset = j + maskArr[i];
                if (UNLIKELY(baseVector->IsNull(offset))) {
                    selectedBaseVector->SetNull(index++);
                } else {
                    auto value = baseVector->GetValue(offset);
                    selectedBaseVector->SetValue(index++, value);
                }
            }
        }
        for (; j < rowCount; j++) {
            if (BitUtil::IsBitSet(bitMark, j)) {
                if (UNLIKELY(baseVector->IsNull(j))) {
                    selectedBaseVector->SetNull(index++);
                } else {
                    auto value = baseVector->GetValue(j);
                    selectedBaseVector->SetValue(index++, value);
                }
            }
        }
    }

    bool GetFlatBaseVectorsFromBitMark(std::vector<BaseVector *> &baseVectors, std::vector<BaseVector *> &result,
        uint8_t *bitMark, int32_t vectorSize, const std::set<int32_t>& isNullSet, const std::set<int32_t>& isNotNullSet) {
        int32_t resultSize = BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize);
        if (resultSize == vectorSize) {
            // all selected, filtering is not required.
            return false;
        }
        if (UNLIKELY(baseVectors.empty())) {
            return false;
        }
        int32_t rowCount = baseVectors[0]->GetSize();
        std::vector<int32_t> retainPositions;
        retainPositions.reserve(resultSize);
        for (int32_t j = 0; j < rowCount; ++j) {
            if (BitUtil::IsBitSet(bitMark, j)) {
                retainPositions.push_back(j);
            }
        }
        result.resize(baseVectors.size(), nullptr);
        for (int32_t i = 0; i < baseVectors.size(); i++) {
            auto baseVector = baseVectors[i];
            auto dataType = baseVector->GetTypeId();
            auto isAllNull = isNullSet.count(i);
            auto isAllNotNull = isNotNullSet.count(i);
            BaseVector *selectedBaseVector = nullptr;
            switch (dataType) {
                case OMNI_ROW:
                case OMNI_ARRAY:
                case OMNI_MAP:
                case OMNI_CONTAINER: {
                    selectedBaseVector = VectorHelper::CopyPositionsVector(baseVector, retainPositions.data(), 0,
                        resultSize);
                    result[i] = selectedBaseVector;
                    continue;
                }
                default: {
                    selectedBaseVector =
                        VectorHelper::CreateVector(OMNI_FLAT, dataType, static_cast<int32_t>(resultSize));
                }
            }
            switch (dataType) {
                case OMNI_INT:
                case OMNI_DATE32: {
                    SetFlatVectorValue<Vector<int32_t>, int32_t>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_SHORT: {
                    SetFlatVectorValue<Vector<int16_t>, int16_t>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_BYTE: {
                    SetFlatVectorValue<Vector<int8_t>, int8_t>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64: {
                    SetFlatVectorValue<Vector<int64_t>, int64_t>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_DOUBLE: {
                    SetFlatVectorValue<Vector<double>, double>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_FLOAT: {
                    SetFlatVectorValue<Vector<float>, float>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_BOOLEAN: {
                    SetFlatVectorValue<Vector<bool>, bool>(rowCount, baseVector, selectedBaseVector, bitMark,
                        isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_DECIMAL128: {
                    SetFlatVectorValue<Vector<Decimal128>, Decimal128>(rowCount, baseVector, selectedBaseVector,
                        bitMark, isAllNull, isAllNotNull);
                    break;
                }
                case OMNI_VARCHAR:
                case OMNI_CHAR: {
                    if (baseVector->GetEncoding() == OMNI_DICTIONARY) {
                        auto dictVector = dynamic_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(baseVector);
                        SetStringVectorValue(rowCount, dictVector,
                            dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(selectedBaseVector), bitMark,
                            isAllNull, isAllNotNull);
                    } else {
                        auto flatVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(baseVector);
                        SetStringVectorValue(rowCount, flatVector,
                            dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(selectedBaseVector), bitMark,
                            isAllNull, isAllNotNull);
                    }
                    break;
                }
                default: {
                    LogError("No such %d type support", dataType);
                    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                        "unsupported selectivity type: " + std::to_string(static_cast<int>(dataType)));
                }
            }
            result[i] = selectedBaseVector;
        }
        return true;
    }
} // namespace common
