/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateDiff function implementation
 * Computes the number of days between two dates (endDate - startDate).
 * Follows Velox Spark SQL behavior: datediff(endDate, startDate) -> int32
 */

#include "DateDiff.h"
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

/// DateDiff function
/// datediff(endDate, startDate) -> int32
/// Returns the number of days from startDate to endDate (endDate - startDate).
/// Both arguments are DATE32 (days since epoch, int32_t).
/// Integer overflow is allowed, consistent with Spark behavior.
class DateDiffFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 2) {
            OMNI_THROW("DateDiff error:", "datediff requires exactly 2 arguments");
        }

        const auto startDateArg = args.top();
        args.pop();
        const auto endDateArg = args.top();
        args.pop();

        int32_t size = 0;
        for (const auto *arg : {endDateArg, startDateArg}) {
            if (arg->GetEncoding() != OMNI_ENCODING_CONST) {
                size = arg->GetSize();
                break;
            }
        }
        if (size == 0) {
            size = endDateArg->GetSize();
        }

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int32_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);

        bool endDateIsConst = (endDateArg->GetEncoding() == OMNI_ENCODING_CONST);
        int32_t constEndDate = 0;
        const int32_t *endDateRaw = nullptr;
        const uint64_t *endDateNulls = nullptr;

        if (endDateIsConst) {
            if (endDateArg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                delete endDateArg;
                delete startDateArg;
                return;
            }
            constEndDate = static_cast<ConstVector<int32_t> *>(endDateArg)->GetConstValue();
        } else {
            auto *endDateVector = reinterpret_cast<Vector<int32_t> *>(endDateArg);
            endDateRaw = unsafe::UnsafeVector::GetRawValues(endDateVector);
            endDateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(endDateArg));
        }

        bool startDateIsConst = (startDateArg->GetEncoding() == OMNI_ENCODING_CONST);
        int32_t constStartDate = 0;
        const int32_t *startDateRaw = nullptr;
        const uint64_t *startDateNulls = nullptr;

        if (startDateIsConst) {
            if (startDateArg->IsNull(0)) {
                auto *resultNulls = unsafe::UnsafeBaseVector::GetNulls(result);
                auto nullsSize = BitUtil::Nbytes(size);
                memset(resultNulls, 0xFF, nullsSize);
                delete endDateArg;
                delete startDateArg;
                return;
            }
            constStartDate = static_cast<ConstVector<int32_t> *>(startDateArg)->GetConstValue();
        } else {
            auto *startDateVector = reinterpret_cast<Vector<int32_t> *>(startDateArg);
            startDateRaw = unsafe::UnsafeVector::GetRawValues(startDateVector);
            startDateNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(startDateArg));
        }

        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto nullsSize = BitUtil::Nbytes(size);

        if (endDateIsConst) {
            memset(resultNulls, 0x00, nullsSize);
        } else {
            memcpy(resultNulls, endDateNulls, nullsSize);
        }

        SelectivityVector rows(size);
        if (endDateIsConst) {
            rows.setAll();
        } else {
            rows.setFromBitsNegate(endDateNulls, size);
        }

        rows.applyToSelected([&](vector_size_t i) {
            if (!startDateIsConst && startDateNulls && BitUtil::IsBitSet(startDateNulls, i)) {
                result->SetNull(i);
                return;
            }

            int32_t endDate = endDateIsConst ? constEndDate : endDateRaw[i];
            int32_t startDate = startDateIsConst ? constStartDate : startDateRaw[i];

            int32_t diff = 0;
            __builtin_sub_overflow(endDate, startDate, &diff);
            resultRaw[i] = diff;
            result->SetNotNull(i);
        });

        delete endDateArg;
        delete startDateArg;
    }
};
} // namespace

void RegisterDateDiffFunction(const std::string &name)
{
    auto func = std::make_shared<DateDiffFunction>();

    // (DATE32, DATE32) -> INT
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_DATE32}, OMNI_INT, func);
    // (INT, INT) -> INT  (OMNI_DATE32 and OMNI_INT are equivalent at runtime)
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_INT}, OMNI_INT, func);
    // (DATE32, INT) -> INT
    VectorFunction::RegisterVectorFunction(name, {OMNI_DATE32, OMNI_INT}, OMNI_INT, func);
    // (INT, DATE32) -> INT
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT, OMNI_DATE32}, OMNI_INT, func);
}
}
