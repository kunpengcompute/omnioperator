/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixDate function implementation
 */

#include "UnixDate.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
/// UnixDateFunction - Convert a date to the number of days since 1970-01-01 (Unix epoch)
/// unix_date(date) -> int
/// Returns the number of days since the Unix epoch (1970-01-01).
/// In OmniOperator, OMNI_DATE32 is already stored as days since epoch (int32_t),
/// so unix_date is a direct pass-through of the internal date value.
/// Supports OMNI_DATE32 and OMNI_INT (equivalent types at runtime).
/// For example: unix_date('1970-01-01') returns 0, unix_date('1970-01-02') returns 1,
/// unix_date('1969-12-31') returns -1.
class UnixDateFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.empty()) {
            return;
        }

        const auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto inputTypeId = inputArg->GetTypeId();

        if (inputTypeId == OMNI_DATE32 || inputTypeId == OMNI_INT) {
            auto *inputVector = reinterpret_cast<Vector<int32_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));

            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, inputNulls, nullsSize);

            SelectivityVector rows(size);
            rows.setFromBitsNegate(inputNulls, size);

            rows.applyToSelected([&](vector_size_t i) {
                resultRaw[i] = inputRaw[i];
                result->SetNotNull(i);
            });
        }
        if (!inputArg->GetIsField()) {
            delete inputArg;
        }
    }
};
} // namespace

void RegisterUnixDateFunction(const std::string &name)
{
    auto unixDateFunction = std::make_shared<UnixDateFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_INT, unixDateFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_INT, unixDateFunction);
}
}
