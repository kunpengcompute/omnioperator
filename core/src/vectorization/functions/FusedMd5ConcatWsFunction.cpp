/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Fused concat_ws and MD5 vector function
 */

#include "vectorization/functions/FusedMd5ConcatWsFunction.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "type/string_Impl.h"
#include "util/compiler_util.h"
#include "util/debug.h"
#include "vector/vector_helper.h"
#include "vectorization/functions/StreamingMd5Context.h"

namespace omniruntime::vectorization {
using namespace omniruntime::codegen;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

ALWAYS_INLINE bool IsNullAt(BaseVector *vector, int32_t row)
{
    const int32_t index = vector->GetEncoding() == OMNI_ENCODING_CONST ? 0 : row;
    return vector->IsNull(index);
}

} // namespace

void FusedMd5ConcatWsFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &,
    BaseVector *&result, ExecutionContext *context) const
{
    if (numArgs_ == 0 || args.size() < numArgs_) {
        OMNI_THROW("FusedMd5ConcatWs function Error", "FusedMd5ConcatWs requires a separator");
    }

    std::vector<BaseVector *> inputs;
    inputs.reserve(numArgs_);
    for (size_t i = 0; i < numArgs_; ++i) {
        inputs.push_back(args.top());
        args.pop();
    }
    std::reverse(inputs.begin(), inputs.end());

    const int32_t rowSize = context->GetResultRowSize();
    auto *output = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateStringVector(rowSize));
    StreamingMd5Context md5;

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(inputs[0], row)) {
            output->SetNull(row);
            continue;
        }

        const std::string_view separator = VectorHelper::GetStringValueFromVector(inputs[0], row);
        md5.Reset();
        bool hasValue = false;
        for (size_t arg = 1; arg < inputs.size(); ++arg) {
            if (IsNullAt(inputs[arg], row)) {
                continue;
            }
            if (hasValue) {
                md5.Update(separator.data(), separator.size());
            }
            const std::string_view value = VectorHelper::GetStringValueFromVector(inputs[arg], row);
            md5.Update(value.data(), value.size());
            hasValue = true;
        }

        char digest[StreamingMd5Context::HEX_DIGEST_SIZE];
        md5.FinishHex(digest);
        output->SetValue(row, std::string_view(digest, sizeof(digest)));
        output->SetNotNull(row);
    }

    for (BaseVector *input : inputs) {
        delete input;
    }
    result = output;
}

std::vector<std::shared_ptr<FunctionSignature>> FusedMd5ConcatWsSignatures(const std::string &name)
{
    return {FunctionSignature::Variadic(name, OMNI_VARCHAR, OMNI_VARCHAR, 1)};
}

std::shared_ptr<VectorFunction> MakeFusedMd5ConcatWsFunction(const std::string &,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &)
{
    return std::make_shared<FusedMd5ConcatWsFunction>(inputArgs.size());
}

} // namespace omniruntime::vectorization
