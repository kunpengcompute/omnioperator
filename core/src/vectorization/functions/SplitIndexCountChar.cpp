/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: vectorized SplitIndex and CountChar.
 * These keep a Calc that also contains LIKE on the vectorized path.
 * Vectorized LIKE implements SQL '%' / '_'; codegen LikeStr is a regex engine.
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

class SplitIndexFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        (void)context;
        (void)outputType;
        auto *indexVec = args.top();
        args.pop();
        auto *delimVec = args.top();
        args.pop();
        auto *strVec = args.top();
        args.pop();

        const int32_t size = strVec->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
        auto *out = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
        auto isNullAt = [](BaseVector *vec, int32_t row) {
            return vec->GetEncoding() == OMNI_ENCODING_CONST ? vec->IsNull(0) : vec->IsNull(row);
        };

        for (int32_t row = 0; row < size; ++row) {
            if (isNullAt(strVec, row) || isNullAt(delimVec, row) || isNullAt(indexVec, row)) {
                result->SetNull(row);
                continue;
            }
            std::string_view text = VectorHelper::GetStringValueFromVector(strVec, row);
            std::string_view delim = FirstChar(VectorHelper::GetStringValueFromVector(delimVec, row));
            int32_t index = VectorHelper::GetValueFromVector<int32_t>(indexVec, row);
            if (delim.empty() || index < 0) {
                result->SetNull(row);
                continue;
            }
            size_t start = 0;
            int32_t current = 0;
            bool found = false;
            std::string_view piece;
            const char sep = delim[0];
            for (size_t i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == sep) {
                    if (current == index) {
                        piece = text.substr(start, i - start);
                        found = true;
                        break;
                    }
                    start = i + 1;
                    ++current;
                }
            }
            if (!found) {
                result->SetNull(row);
            } else {
                out->SetValue(row, piece);
                result->SetNotNull(row);
            }
        }
        delete indexVec;
        delete delimVec;
        delete strVec;
    }
};

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

void RegisterStringTypeCombos(const std::string &name, DataTypeId ret, const std::shared_ptr<VectorFunction> &func,
    const std::vector<DataTypeId> &tail)
{
    const DataTypeId strings[] = {OMNI_VARCHAR, OMNI_CHAR};
    for (DataTypeId left : strings) {
        for (DataTypeId right : strings) {
            std::vector<DataTypeId> args{left, right};
            args.insert(args.end(), tail.begin(), tail.end());
            VectorFunction::RegisterVectorFunction(name, args, ret, func);
        }
    }
}

}  // namespace

void RegisterSplitIndexAndCountChar(const std::string &prefix)
{
    RegisterStringTypeCombos(prefix + "SplitIndex", OMNI_VARCHAR, std::make_shared<SplitIndexFunction>(), {OMNI_INT});
    RegisterStringTypeCombos(prefix + "CountChar", OMNI_LONG, std::make_shared<CountCharFunction>(), {});
}

}  // namespace omniruntime::vectorization
