/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: vectorized CountChar.
 *
 * Matches codegen CountChar: a NULL string or a NULL search character returns 0.
 * An empty search character counts '\0', the same byte target[0] yields for "".
 */

#include "vectorization/VectorFunction.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

class CountCharFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        (void)context;
        (void)outputType;
        auto *needleVec = args.top();
        args.pop();
        auto *strVec = args.top();
        args.pop();

        const int32_t size = strVec->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
        auto *out = static_cast<Vector<int64_t> *>(result);
        auto isNullAt = [](BaseVector *vec, int32_t row) {
            return vec->GetEncoding() == OMNI_ENCODING_CONST ? vec->IsNull(0) : vec->IsNull(row);
        };

        for (int32_t row = 0; row < size; ++row) {
            if (isNullAt(strVec, row) || isNullAt(needleVec, row)) {
                out->SetValue(row, 0);
                result->SetNotNull(row);
                continue;
            }
            std::string_view text = VectorHelper::GetStringValueFromVector(strVec, row);
            std::string_view needle = VectorHelper::GetStringValueFromVector(needleVec, row);
            const char target = needle.empty() ? '\0' : needle[0];
            int64_t count = 0;
            for (char ch : text) {
                if (ch == target) {
                    ++count;
                }
            }
            out->SetValue(row, count);
            result->SetNotNull(row);
        }
        delete needleVec;
        delete strVec;
    }
};

void RegisterStringTypeCombos(const std::string &name, DataTypeId ret, const std::shared_ptr<VectorFunction> &func)
{
    const DataTypeId strings[] = {OMNI_VARCHAR, OMNI_CHAR};
    for (DataTypeId left : strings) {
        for (DataTypeId right : strings) {
            VectorFunction::RegisterVectorFunction(name, {left, right}, ret, func);
        }
    }
}

}  // namespace

void RegisterCountChar(const std::string &prefix)
{
    RegisterStringTypeCombos(prefix + "CountChar", OMNI_LONG, std::make_shared<CountCharFunction>());
}

}  // namespace omniruntime::vectorization
