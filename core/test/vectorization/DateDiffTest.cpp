/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DateDiff function unit tests
 */

#include <gtest/gtest.h>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DateDiff.h"
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

class DateDiffTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const date_diff_test_env = ::testing::AddGlobalTestEnvironment(new DateDiffTestEnvironment);

class DateDiffFunctionTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteDateDiff(BaseVector* endDateVec, BaseVector* startDateVec,
        DataTypeId endDateTypeId, DataTypeId startDateTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("date_diff",
            std::vector<DataTypeId>{endDateTypeId, startDateTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DateDiff function not found for signature ("
            << static_cast<int>(endDateTypeId) << ", " << static_cast<int>(startDateTypeId) << ")";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(endDateVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(endDateVec);
        args.push(startDateVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DateDiff function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: datediff with DATE32, DATE32 — basic positive/negative/zero differences
TEST(DateDiffTest, Date32BasicDifferences) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(1994, 4, 20)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 21),
        DateDiffFunctionTestHelper::DateToDays(1994, 4, 20)
    };

    // endDate - startDate: -1, -358, 0
    std::vector<int32_t> expected = {-1, -358, 0};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: datediff with INT, INT — equivalent type
TEST(DateDiffTest, IntIntBasicDifferences) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(1994, 4, 20)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 21),
        DateDiffFunctionTestHelper::DateToDays(1994, 4, 20)
    };

    std::vector<int32_t> expected = {-1, -358, 0};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateIntVector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateIntVector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_INT, OMNI_INT, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: datediff with mixed types DATE32/INT
TEST(DateDiffTest, MixedTypeDate32Int) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 29)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 1, 30)
    };

    // 2020-02-29 - 2019-01-30 = 395
    std::vector<int32_t> expected = {395};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateIntVector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_INT, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: datediff with mixed types INT/DATE32
TEST(DateDiffTest, MixedTypeIntDate32) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 29)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 1, 30)
    };

    std::vector<int32_t> expected = {395};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateIntVector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_INT, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: leap year handling
TEST(DateDiffTest, LeapYear) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 29),
        DateDiffFunctionTestHelper::DateToDays(2020, 3, 1)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 1, 30),
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 29)
    };

    // 2020-02-29 - 2019-01-30 = 395; 2020-03-01 - 2020-02-29 = 1
    std::vector<int32_t> expected = {395, 1};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: large date difference
TEST(DateDiffTest, LargeDateDiff) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2020, 2, 29)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(4040, 2, 29)
    };

    // 2020-02-29 - 4040-02-29 = -737790
    std::vector<int32_t> expected = {-737790};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: same date returns zero
TEST(DateDiffTest, SameDateReturnsZero) {
    std::vector<int32_t> dateValues = {
        DateDiffFunctionTestHelper::DateToDays(1994, 4, 20),
        DateDiffFunctionTestHelper::DateToDays(2024, 1, 29),
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1)
    };

    std::vector<int32_t> expected = {0, 0, 0};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(
        std::vector<int32_t>(dateValues));
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: epoch date (1970-01-01)
TEST(DateDiffTest, EpochDate) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1),
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 2),
        DateDiffFunctionTestHelper::DateToDays(2024, 1, 29)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1),
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1),
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1)
    };

    // 0, 1, 19751 (days from epoch to 2024-01-29)
    int32_t daysTo20240129 = DateDiffFunctionTestHelper::DateToDays(2024, 1, 29);
    std::vector<int32_t> expected = {0, 1, daysTo20240129};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: year boundary crossing
TEST(DateDiffTest, YearBoundary) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2020, 1, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 12, 31)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 12, 31),
        DateDiffFunctionTestHelper::DateToDays(2020, 1, 1)
    };

    // 1, -1
    std::vector<int32_t> expected = {1, -1};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}

// Test: NULL endDate
TEST(DateDiffTest, NullEndDate) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 4, 1)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1)
    };

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);

    endDateVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (endDate is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), -1) << "Row 0: 2019-02-28 - 2019-03-01 = -1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2: 2019-04-01 - 2019-03-01 = 31";

    delete resultVec;
}

// Test: NULL startDate
TEST(DateDiffTest, NullStartDate) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 4, 1)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1)
    };

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);

    startDateVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (startDate is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), -1) << "Row 0: 2019-02-28 - 2019-03-01 = -1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2: 2019-04-01 - 2019-03-01 = 31";

    delete resultVec;
}

// Test: both endDate and startDate NULL
TEST(DateDiffTest, BothNull) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 2, 28),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1),
        DateDiffFunctionTestHelper::DateToDays(2019, 3, 1)
    };

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);

    endDateVec->SetNull(0);
    startDateVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (both NULL)";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(1), 0) << "Row 1: 2019-03-01 - 2019-03-01 = 0";

    delete resultVec;
}

// Test: negative date values (dates before epoch)
TEST(DateDiffTest, DatesBeforeEpoch) {
    std::vector<int32_t> endDateValues = {
        DateDiffFunctionTestHelper::DateToDays(1969, 12, 31),
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1)
    };
    std::vector<int32_t> startDateValues = {
        DateDiffFunctionTestHelper::DateToDays(1970, 1, 1),
        DateDiffFunctionTestHelper::DateToDays(1969, 12, 31)
    };

    // 1969-12-31 - 1970-01-01 = -1; 1970-01-01 - 1969-12-31 = 1
    std::vector<int32_t> expected = {-1, 1};

    BaseVector* endDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(endDateValues);
    BaseVector* startDateVec = DateDiffFunctionTestHelper::CreateDate32Vector(startDateValues);
    BaseVector* resultVec = nullptr;

    DateDiffFunctionTestHelper::ExecuteDateDiff(endDateVec, startDateVec, OMNI_DATE32, OMNI_DATE32, resultVec);
    DateDiffFunctionTestHelper::ValidateResult(resultVec, expected, endDateValues.size());

    delete resultVec;
}
