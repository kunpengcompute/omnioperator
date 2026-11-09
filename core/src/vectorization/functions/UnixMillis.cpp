/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixMillis function implementation
 */

#include "UnixMillis.h"
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
/// unix_millis function
/// unix_millis(timestamp) -> bigint
/// Returns the number of milliseconds since 1970-01-01 00:00:00 UTC.
/// Truncates higher levels of precision (microseconds, nanoseconds).
/// Input timestamp is stored as microseconds since epoch internally.
class UnixMillisFunction : public VectorFunction {
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

        // OMNI_TIMESTAMP and OMNI_LONG are equivalent types at runtime
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
                // Convert microseconds to Timestamp, then extract milliseconds.
                // Timestamp::fromMicros handles negative values correctly:
                // for pre-epoch timestamps, seconds_ is properly floored.
                Timestamp ts = Timestamp::fromMicros(microseconds);
                resultRaw[i] = ts.toMillis();
                result->SetNotNull(i);
            });
        }
        if (!inputArg->GetIsField()) {
            delete inputArg;
        }
    }
};
} // namespace

void RegisterUnixMillisFunction(const std::string &name)
{
    auto unixMillisFunction = std::make_shared<UnixMillisFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP}, OMNI_LONG, unixMillisFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_LONG, unixMillisFunction);
}
}
