/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: vectorized CountChar.
 *
 * Nexmark CountChar.eval(StringData, StringData) returns primitive long.
 * A NULL string is not propagated: the method returns 0. An empty string
 * also returns 0. A NULL search character cannot be dereferenced by the UDF;
 * native returns NULL for that input instead of throwing.
 */

#include "vectorization/VectorFunction.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

std::string_view FirstChar(std::string_view value)
{
    return value.empty() ? std::string_view{} : std::string_view(value.data(), 1);
}

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
            // UDF: if (str == null) return 0. This runs before the search character is read.
            if (isNullAt(strVec, row)) {
                out->SetValue(row, 0);
                result->SetNotNull(row);
                continue;
            }
            if (isNullAt(needleVec, row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view text = VectorHelper::GetStringValueFromVector(strVec, row);
            std::string_view needle = FirstChar(VectorHelper::GetStringValueFromVector(needleVec, row));
            if (needle.empty()) {
                out->SetValue(row, 0);
                result->SetNotNull(row);
                continue;
            }
            const char target = needle[0];
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
