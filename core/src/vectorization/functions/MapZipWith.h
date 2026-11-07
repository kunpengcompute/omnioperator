/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: map_zip_with lambda function for map operations
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/ExprEval.h"
#include "type/data_operations.h"
#include <vector>
#include <memory>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    class MapZipWithVectorFunction : public VectorFunction {
    public:
        explicit MapZipWithVectorFunction() {}

        void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
                   ExecutionContext *context) const override
        {
            BaseVector *rightMapBase = args.top();
            args.pop();
            auto rightGuard = MakeVectorArgGuard(rightMapBase);
            BaseVector *leftMapBase = args.top();
            args.pop();
            auto leftGuard = MakeVectorArgGuard(leftMapBase);

            const expressions::LambdaExpr *lambdaExpr = context->GetCurrentLambda();
            if (lambdaExpr == nullptr) {
                throw OmniException("MAP_ZIP_WITH_ERROR", "Lambda expression is null for map_zip_with function");
            }
            if (lambdaExpr->GetParamNum() != 3) {
                throw OmniException("MAP_ZIP_WITH_ERROR", "map_zip_with requires lambda with exactly 3 parameters");
            }
            expressions::Expr *lambdaBody = lambdaExpr->GetBody();

            MapVector *leftMap = dynamic_cast<MapVector *>(leftMapBase);
            MapVector *rightMap = dynamic_cast<MapVector *>(rightMapBase);
            if (!leftMap || !rightMap) {
                throw OmniException("MAP_ZIP_WITH_ERROR", "Input vectors are not valid MapVectors");
            }

            int32_t rowSize = leftMap->GetSize();
            result = new MapVector(rowSize);
            auto *dstMap = static_cast<MapVector *>(result);

            auto keyTypeId = leftMap->GetKeyVector()->GetTypeId();
            auto leftValueTypeId = leftMap->GetValueVector()->GetTypeId();
            auto rightValueTypeId = rightMap->GetValueVector()->GetTypeId();

            BaseVector *srcLeftKeys = leftMap->GetKeyVector().get();
            BaseVector *srcRightKeys = rightMap->GetKeyVector().get();
            BaseVector *srcLeftValues = leftMap->GetValueVector().get();
            BaseVector *srcRightValues = rightMap->GetValueVector().get();

            struct MergeEntry {
                int32_t keySourceIdx;
                bool keyFromLeft;
                int32_t leftValueIdx;
                int32_t rightValueIdx;
            };

            std::vector<std::vector<MergeEntry>> rowMergeEntries(rowSize);
            int32_t totalMergedElements = 0;

            for (int32_t row = 0; row < rowSize; ++row) {
                if (leftMap->IsNull(row) || rightMap->IsNull(row)) {
                    dstMap->SetNull(row);
                    dstMap->SetOffset(row + 1, totalMergedElements);
                    continue;
                }

                int64_t leftOffset = leftMap->GetOffset(row);
                int64_t leftSize = leftMap->GetSize(row);
                int64_t rightOffset = rightMap->GetOffset(row);
                int64_t rightSize = rightMap->GetSize(row);

                std::vector<bool> rightUsed(rightSize, false);
                auto &entries = rowMergeEntries[row];

                for (int64_t i = 0; i < leftSize; ++i) {
                    int32_t leftIdx = static_cast<int32_t>(leftOffset + i);
                    int32_t matchedRight = -1;

                    for (int64_t j = 0; j < rightSize; ++j) {
                        if (rightUsed[j]) continue;
                        int32_t rightIdx = static_cast<int32_t>(rightOffset + j);
                        if (KeysEqual(srcLeftKeys, leftIdx, srcRightKeys, rightIdx, keyTypeId)) {
                            matchedRight = rightIdx;
                            rightUsed[j] = true;
                            break;
                        }
                    }

                    MergeEntry entry;
                    entry.keySourceIdx = leftIdx;
                    entry.keyFromLeft = true;
                    entry.leftValueIdx = leftIdx;
                    entry.rightValueIdx = matchedRight;
                    entries.push_back(entry);
                }

                for (int64_t j = 0; j < rightSize; ++j) {
                    if (rightUsed[j]) continue;
                    int32_t rightIdx = static_cast<int32_t>(rightOffset + j);

                    MergeEntry entry;
                    entry.keySourceIdx = rightIdx;
                    entry.keyFromLeft = false;
                    entry.leftValueIdx = -1;
                    entry.rightValueIdx = rightIdx;
                    entries.push_back(entry);
                }

                totalMergedElements += static_cast<int32_t>(entries.size());
                dstMap->SetOffset(row + 1, totalMergedElements);
            }

            if (totalMergedElements == 0) {
                VectorHelper::EmptyMapProjection(dstMap, keyTypeId, leftValueTypeId);
                return;
            }

            std::unique_ptr<BaseVector> mergedKeysHolder(VectorHelper::CreateFlatVector(static_cast<int32_t>(keyTypeId), totalMergedElements));
            std::unique_ptr<BaseVector> mergedLeftValuesHolder(VectorHelper::CreateFlatVector(static_cast<int32_t>(leftValueTypeId), totalMergedElements));
            std::unique_ptr<BaseVector> mergedRightValuesHolder(VectorHelper::CreateFlatVector(static_cast<int32_t>(rightValueTypeId), totalMergedElements));
            BaseVector *mergedKeys = mergedKeysHolder.get();
            BaseVector *mergedLeftValues = mergedLeftValuesHolder.get();
            BaseVector *mergedRightValues = mergedRightValuesHolder.get();

            int32_t dstIdx = 0;
            for (int32_t row = 0; row < rowSize; ++row) {
                if (dstMap->IsNull(row)) continue;

                for (const auto &entry : rowMergeEntries[row]) {
                    if (entry.keyFromLeft) {
                        CopyElement(srcLeftKeys, entry.keySourceIdx, mergedKeys, dstIdx, keyTypeId);
                    } else {
                        CopyElement(srcRightKeys, entry.keySourceIdx, mergedKeys, dstIdx, keyTypeId);
                    }

                    if (entry.leftValueIdx >= 0) {
                        CopyElement(srcLeftValues, entry.leftValueIdx, mergedLeftValues, dstIdx, leftValueTypeId);
                    } else {
                        mergedLeftValues->SetNull(dstIdx);
                    }

                    if (entry.rightValueIdx >= 0) {
                        CopyElement(srcRightValues, entry.rightValueIdx, mergedRightValues, dstIdx, rightValueTypeId);
                    } else {
                        mergedRightValues->SetNull(dstIdx);
                    }

                    dstIdx++;
                }
            }

            mergedKeys->SetIsField(true);
            mergedLeftValues->SetIsField(true);
            mergedRightValues->SetIsField(true);

            ExprEval lambdaEval(context);
            lambdaEval.paramNameToIdxMap = lambdaExpr->paramNameToIdxMap_;
            lambdaEval.lambdaParams_.push_back(mergedKeys);
            lambdaEval.lambdaParams_.push_back(mergedLeftValues);
            lambdaEval.lambdaParams_.push_back(mergedRightValues);

            context->SetResultRowSize(totalMergedElements);
            lambdaBody->Accept(lambdaEval);
            context->SetResultRowSize(rowSize);
            BaseVector *lambdaResult = lambdaEval.GetResult();
            if (lambdaResult == nullptr) {
                throw OmniException("MAP_ZIP_WITH_ERROR", "Lambda execute return null result");
            }

            dstMap->AddKeys(mergedKeysHolder.release());
            dstMap->AddValues(lambdaResult);
        }

    private:
        bool KeysEqual(BaseVector *leftKeys, int32_t leftIdx, BaseVector *rightKeys, int32_t rightIdx,
                       DataTypeId typeId) const
        {
            if (leftKeys->IsNull(leftIdx) || rightKeys->IsNull(rightIdx)) {
                return false;
            }
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    return static_cast<Vector<int32_t> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<int32_t> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    return static_cast<Vector<int64_t> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<int64_t> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_DOUBLE:
                    return static_cast<Vector<double> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<double> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_FLOAT:
                    return static_cast<Vector<float> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<float> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_SHORT:
                    return static_cast<Vector<int16_t> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<int16_t> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_BYTE:
                    return static_cast<Vector<int8_t> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<int8_t> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_BOOLEAN:
                    return static_cast<Vector<bool> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<bool> *>(rightKeys)->GetValue(rightIdx);
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    return static_cast<Vector<LargeStringContainer<std::string_view>> *>(leftKeys)->GetValue(leftIdx) ==
                           static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightKeys)->GetValue(rightIdx);
                default:
                    throw OmniException("MAP_ZIP_WITH_ERROR",
                        "Unsupported key type: " + std::to_string(typeId));
            }
        }

        void CopyElement(BaseVector *src, int32_t srcIdx, BaseVector *dst, int32_t dstIdx,
                         DataTypeId typeId) const
        {
            if (src->IsNull(srcIdx)) {
                dst->SetNull(dstIdx);
                return;
            }
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    static_cast<Vector<int32_t> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<int32_t> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    static_cast<Vector<int64_t> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<int64_t> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_DOUBLE:
                    static_cast<Vector<double> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<double> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_FLOAT:
                    static_cast<Vector<float> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<float> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_SHORT:
                    static_cast<Vector<int16_t> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<int16_t> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_BYTE:
                    static_cast<Vector<int8_t> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<int8_t> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_BOOLEAN:
                    static_cast<Vector<bool> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<bool> *>(src)->GetValue(srcIdx));
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    static_cast<Vector<LargeStringContainer<std::string_view>> *>(dst)->SetValue(dstIdx,
                        static_cast<Vector<LargeStringContainer<std::string_view>> *>(src)->GetValue(srcIdx));
                    break;
                default:
                    throw OmniException("MAP_ZIP_WITH_ERROR",
                        "Unsupported element type: " + std::to_string(typeId));
            }
        }
    };
}
