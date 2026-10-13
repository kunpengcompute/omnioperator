/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: make_date(year, month, day) -> DATE32 unit tests
 */

#include <gtest/gtest.h>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class MakeDateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const make_date_test_env =
    ::testing::AddGlobalTestEnvironment(new MakeDateTestEnvironment);

class MakeDateTestHelper {
public:
    static int32_t ExpectedDays(int32_t year, int16_t month, int16_t day) {
        LocalDate d(year, month, day);
        return d.ToDays();
    }

    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
            vec->SetNotNull(i);
        }
        return vec;
    }

    static void ExecuteMakeDate(BaseVector* yearVec, BaseVector* monthVec, BaseVector* dayVec,
        BaseVector*& result) {
        auto sig = std::make_shared<FunctionSignature>("make_date",
            std::vector<DataTypeId>{OMNI_INT, OMNI_INT, OMNI_INT}, OMNI_DATE32);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "make_date function not found";
        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(yearVec->GetSize());
        std::stack<BaseVector*> args;
        // Push in call order (year, month, day) so stack top = day, matching Apply pop order
        args.push(yearVec);
        args.push(monthVec);
        args.push(dayVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateDateResult(BaseVector* result, const std::vector<int32_t>& expectedDays,
        int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr);
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actual = resultVec->GetValue(i);
            ASSERT_LT(i, static_cast<int>(expectedDays.size())) << "Row " << i << " no expected value";
            EXPECT_EQ(actual, expectedDays[static_cast<size_t>(i)]) << "Row " << i;
        }
    }
};

TEST(MakeDateTest, BasicValidDates) {
    std::vector<int32_t> years = {1970, 2000, 2024, 1920};
    std::vector<int32_t> months = {1, 1, 6, 1};
    std::vector<int32_t> days = {1, 1, 15, 25};

    BaseVector* yearVec = MakeDateTestHelper::CreateInt32Vector(years);
    BaseVector* monthVec = MakeDateTestHelper::CreateInt32Vector(months);
    BaseVector* dayVec = MakeDateTestHelper::CreateInt32Vector(days);

    BaseVector* result = nullptr;
    MakeDateTestHelper::ExecuteMakeDate(yearVec, monthVec, dayVec, result);
    ASSERT_NE(result, nullptr);

    std::vector<int32_t> expected;
    for (size_t i = 0; i < years.size(); ++i) {
        expected.push_back(MakeDateTestHelper::ExpectedDays(years[i], static_cast<int16_t>(months[i]), static_cast<int16_t>(days[i])));
    }
    MakeDateTestHelper::ValidateDateResult(result, expected, static_cast<int>(years.size()));

    // Input vectors (yearVec, monthVec, dayVec) are already deleted inside Apply
    delete result;
}

TEST(MakeDateTest, EpochDate) {
    std::vector<int32_t> years = {1970};
    std::vector<int32_t> months = {1};
    std::vector<int32_t> days = {1};

    BaseVector* yearVec = MakeDateTestHelper::CreateInt32Vector(years);
    BaseVector* monthVec = MakeDateTestHelper::CreateInt32Vector(months);
    BaseVector* dayVec = MakeDateTestHelper::CreateInt32Vector(days);

    BaseVector* result = nullptr;
    MakeDateTestHelper::ExecuteMakeDate(yearVec, monthVec, dayVec, result);
    ASSERT_NE(result, nullptr);

    auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVec->GetValue(0), 0) << "1970-01-01 should be 0 days since epoch";

    delete result;
}

TEST(MakeDateTest, InvalidDateReturnsNull) {
    // 2024-13-01 invalid month
    std::vector<int32_t> years = {2024};
    std::vector<int32_t> months = {13};
    std::vector<int32_t> days = {1};

    BaseVector* yearVec = MakeDateTestHelper::CreateInt32Vector(years);
    BaseVector* monthVec = MakeDateTestHelper::CreateInt32Vector(months);
    BaseVector* dayVec = MakeDateTestHelper::CreateInt32Vector(days);

    BaseVector* result = nullptr;
    MakeDateTestHelper::ExecuteMakeDate(yearVec, monthVec, dayVec, result);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "Invalid month 13 should yield NULL";

    delete result;
}

TEST(MakeDateTest, NullPropagation) {
    std::vector<int32_t> years = {2024, 2024};
    std::vector<int32_t> months = {1, 1};
    std::vector<int32_t> days = {1, 15};
    BaseVector* yearVec = MakeDateTestHelper::CreateInt32Vector(years);
    BaseVector* monthVec = MakeDateTestHelper::CreateInt32Vector(months);
    BaseVector* dayVec = MakeDateTestHelper::CreateInt32Vector(days);
    yearVec->SetNull(0);

    BaseVector* result = nullptr;
    MakeDateTestHelper::ExecuteMakeDate(yearVec, monthVec, dayVec, result);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0));
    EXPECT_FALSE(result->IsNull(1));

    delete result;
}
