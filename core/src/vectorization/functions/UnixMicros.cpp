/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixMicros function implementation
 */

#include "UnixMicros.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/Timestamp.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
/// unix_micros function
/// unix_micros(timestamp) -> bigint
/// Returns the number of microseconds since 1970-01-01 00:00:00 UTC.
/// Input timestamp is stored as microseconds since epoch internally,
/// so this function converts through Timestamp to ensure correctness.
class UnixMicrosFunction : public VectorFunction {
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

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        const auto inputTypeId = inputArg->GetTypeId();

        if (inputTypeId == OMNI_TIMESTAMP || inputTypeId == OMNI_LONG) {
            auto *inputVector = reinterpret_cast<Vector<int64_t> *>(inputArg);
            const auto *inputRaw = unsafe::UnsafeVector::GetRawValues(inputVector);
            const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));

            auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
            auto nullsSize = BitUtil::Nbytes(size);
            memcpy(resultNulls, inputNulls, nullsSize);

            SelectivityVector rows(size);
            rows.setFromBitsNegate(inputNulls, size);

            rows.applyToSelected([&](vector_size_t i) {
                int64_t microseconds = inputRaw[i];
                Timestamp ts = Timestamp::fromMicros(microseconds);
                resultRaw[i] = ts.toMicros();
                result->SetNotNull(i);
            });
        }
        if (!inputArg->GetIsField()) {
            delete inputArg;
        }
    }
};
} // namespace

void RegisterUnixMicrosFunction(const std::string &name)
{
    auto unixMicrosFunction = std::make_shared<UnixMicrosFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_LONG, unixMicrosFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_LONG, unixMicrosFunction);
}
}
