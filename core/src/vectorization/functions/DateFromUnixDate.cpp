/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateFromUnixDate function implementation
 */

#include "DateFromUnixDate.h"
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
/// DateFromUnixDateFunction - Convert the number of days since 1970-01-01 to a date
/// date_from_unix_date(int) -> date
/// Returns a DATE value from the number of days since the Unix epoch (1970-01-01).
/// This is the inverse of unix_date.
/// In OmniOperator, OMNI_DATE32 is stored as days since epoch (int32_t),
/// so date_from_unix_date is a direct pass-through of the integer value to DATE32.
/// Supports OMNI_INT and OMNI_DATE32 as input (equivalent types at runtime).
/// For example: date_from_unix_date(0) returns '1970-01-01',
/// date_from_unix_date(1) returns '1970-01-02',
/// date_from_unix_date(-1) returns '1969-12-31'.
class DateFromUnixDateFunction : public VectorFunction {
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

        if (inputTypeId == OMNI_INT || inputTypeId == OMNI_DATE32) {
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

void RegisterDateFromUnixDateFunction(const std::string &name)
{
    auto dateFromUnixDateFunction = std::make_shared<DateFromUnixDateFunction>();
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_DATE32, dateFromUnixDateFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32}, OMNI_DATE32, dateFromUnixDateFunction);
}
}
