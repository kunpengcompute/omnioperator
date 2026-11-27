/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MakeDate function implementation - make_date(year, month, day) -> DATE32
 */

#include "MakeDate.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "type/date32.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>
#include "libboundscheck/include/securec.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {
class MakeDateFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 3) {
            return;
        }
        // Create result first (using size from stack top) so result does not reuse an arg's memory
        const auto size = args.top()->GetSize();
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }

        // Args are pushed in call order (year, month, day), so stack top = day; pop order = day, month, year
        BaseVector *dayVec = args.top();
        args.pop();
        BaseVector *monthVec = args.top();
        args.pop();
        BaseVector *yearVec = args.top();
        args.pop();

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto *yearRaw = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(yearVec));
        auto *monthRaw = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(monthVec));
        auto *dayRaw = unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(dayVec));
        const auto *yearNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(yearVec));
        const auto *monthNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(monthVec));
        const auto *dayNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(dayVec));
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy_s(resultNulls, nullsSize, yearNulls, nullsSize);
        for (vector_size_t i = 0; i < size; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            if (monthVec->IsNull(i) || dayVec->IsNull(i)) {
                result->SetNull(i);
                continue;
            }
            int32_t year = yearRaw[i];
            int32_t month = monthRaw[i];
            int32_t day = dayRaw[i];
            int64_t daysSinceEpoch = 0;
            if (!Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch)) {
                result->SetNull(i);
                continue;
            }
            if (daysSinceEpoch < std::numeric_limits<int32_t>::min() ||
                daysSinceEpoch > std::numeric_limits<int32_t>::max()) {
                result->SetNull(i);
                continue;
            }
            resultRaw[i] = static_cast<int32_t>(daysSinceEpoch);
            result->SetNotNull(i);
        }

        delete dayVec;
        delete monthVec;
        delete yearVec;
    }
};
} // namespace

void RegisterMakeDateFunction(const std::string &name)
{
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_INT, OMNI_INT}, OMNI_DATE32,
        std::make_shared<MakeDateFunction>());
}
}
