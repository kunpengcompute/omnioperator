/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapConcat function implementation
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/map_vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class MapConcatFunction : public VectorFunction {
public:
    explicit MapConcatFunction(int32_t numArgs) : numArgs_(numArgs) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
               ExecutionContext *context) const override
    {
        if (static_cast<int32_t>(args.size()) < numArgs_) {
            OMNI_THROW("MapConcat Error:", "map_concat does not have enough arguments");
        }

        std::vector<MapVector *> mapArgs(numArgs_);
        std::vector<VectorArgGuard> argGuards;
        argGuards.reserve(numArgs_);
        for (int32_t i = numArgs_ - 1; i >= 0; --i) {
            BaseVector *vec = args.top();
            args.pop();
            argGuards.emplace_back(MakeVectorArgGuard(vec));
            mapArgs[i] = dynamic_cast<MapVector *>(vec);
            if (mapArgs[i] == nullptr) {
                OMNI_THROW("MapConcat Error:", "map_concat argument must be MAP type");
            }
        }

        int32_t totalRows = context->GetResultRowSize();
        result = new MapVector(totalRows);
        auto *dstMap = dynamic_cast<MapVector *>(result);

        DataTypeId keyTypeId = mapArgs[0]->GetKeyVector()->GetTypeId();
        DataTypeId valueTypeId = mapArgs[0]->GetValueVector()->GetTypeId();

        int64_t totalEntries = 0;
        for (int32_t row = 0; row < totalRows; ++row) {
            bool anyNull = false;
            for (int32_t m = 0; m < numArgs_; ++m) {
                if (mapArgs[m]->IsNull(row)) {
                    anyNull = true;
                    break;
                }
            }
            if (anyNull) {
                continue;
            }
            for (int32_t m = 0; m < numArgs_; ++m) {
                totalEntries += mapArgs[m]->GetSize(row);
            }
        }

        auto combinedKeys = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(static_cast<int32_t>(keyTypeId), totalEntries));
        auto combinedValues = std::shared_ptr<BaseVector>(
            VectorHelper::CreateFlatVector(static_cast<int32_t>(valueTypeId), totalEntries));

        int64_t offset = 0;
        for (int32_t row = 0; row < totalRows; ++row) {
            bool anyNull = false;
            for (int32_t m = 0; m < numArgs_; ++m) {
                if (mapArgs[m]->IsNull(row)) {
                    anyNull = true;
                    break;
                }
            }
            if (anyNull) {
                dstMap->SetNull(row);
                continue;
            }

            for (int32_t m = 0; m < numArgs_; ++m) {
                int64_t srcOffset = mapArgs[m]->GetOffset(row);
                int64_t srcSize = mapArgs[m]->GetSize(row);
                if (srcSize > 0) {
                    BaseVector *slicedKeys = mapArgs[m]->GetKeyVector()->Slice(srcOffset, srcSize);
                    BaseVector *slicedValues = mapArgs[m]->GetValueVector()->Slice(srcOffset, srcSize);
                    VectorHelper::AppendVector(combinedKeys.get(), offset, slicedKeys, srcSize);
                    VectorHelper::AppendVector(combinedValues.get(), offset, slicedValues, srcSize);
                    delete slicedKeys;
                    delete slicedValues;
                    offset += srcSize;
                }
            }
            dstMap->SetOffset(row + 1, offset);
        }

        dstMap->SetKeyVector(combinedKeys);
        dstMap->SetValueVector(combinedValues);
    }

private:
    int32_t numArgs_;
};

std::shared_ptr<VectorFunction> makeMapConcat(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &);

std::vector<std::shared_ptr<codegen::FunctionSignature>> MapConcatSignatures();

}
