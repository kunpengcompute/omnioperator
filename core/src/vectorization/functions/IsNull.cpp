//
// Created by root on 10/10/25.
//

#include "IsNull.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;

namespace {
class IsNullFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        auto *arg = args.top();
        args.pop();
        const auto raw = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<bool> *>(result));
        const auto size = arg->GetSize();

        memset(raw, 0, size * sizeof(bool));

        const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(arg));
        SelectivityVector rows(size);
        rows.setFromBits(nullBits, size);
        rows.applyToSelected([&](vector_size_t i) { raw[i] = true; });
        if (arg != nullptr && !arg->GetIsField()) {
            delete arg;
        }
    }
};
}

void RegisterIsNullFunction(const std::string &name)
{
    std::vector<std::vector<DataTypeId>> params = {
        {OMNI_INT},
        {OMNI_LONG},
        {OMNI_VARCHAR},
        {OMNI_DOUBLE},
        {OMNI_BOOLEAN},
        {OMNI_SHORT},
        {OMNI_DECIMAL64},
        {OMNI_DECIMAL128},
        {OMNI_DATE32},
        {OMNI_DATE64},
        {OMNI_TIME32},
        {OMNI_TIME64},
        {OMNI_TIMESTAMP},
        {OMNI_INTERVAL_MONTHS},
        {OMNI_INTERVAL_DAY_TIME},
        {OMNI_CHAR},
        {OMNI_CONTAINER},
        {OMNI_BYTE},
        {OMNI_FLOAT},
        {OMNI_VARBINARY},
        {OMNI_ARRAY},
        {OMNI_MAP},
        {OMNI_ROW}
    };
    for (const auto &param : params) {
        auto signature = std::make_shared<codegen::FunctionSignature>(name, param, OMNI_BOOLEAN);
        VectorFunction::functionMap_.insert(std::make_pair(signature, std::make_shared<IsNullFunction>()));
    }
}
}
