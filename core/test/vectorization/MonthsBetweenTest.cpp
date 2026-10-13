/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MonthsBetween function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/MonthsBetween.h"
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

class MonthsBetweenTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const months_between_test_env =
    ::testing::AddGlobalTestEnvironment(new MonthsBetweenTestEnvironment);

class MonthsBetweenTestHelper {
public:
    static void ValidateDoubleResult(BaseVector* result, const std::vector<double>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<double>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            double actualValue = resultVec->GetValue(i);
            double expectedValue = expected[i];
            EXPECT_DOUBLE_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateBoolVector(const std::vector<bool>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, values.size());
        auto* typedVec = static_cast<Vector<bool>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteMonthsBetween(BaseVector* ts1Vec, BaseVector* ts2Vec, BaseVector* roundOffVec,
        DataTypeId ts1TypeId, DataTypeId ts2TypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("months_between",
            std::vector<DataTypeId>{ts1TypeId, ts2TypeId, OMNI_BOOLEAN}, OMNI_DOUBLE);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "months_between function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_DOUBLE);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(ts1Vec->GetSize());
        std::stack<BaseVector*> args;

        args.push(ts1Vec);
        args.push(ts2Vec);
        args.push(roundOffVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "months_between function threw an exception";
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static int64_t DateTimeToMicros(int year, int month, int day, int hour, int minute, int second) {
        int32_t days = DateToDays(year, month, day);
        int64_t daySeconds = static_cast<int64_t>(days) * 86400LL;
        int64_t timeSeconds = static_cast<int64_t>(hour) * 3600LL +
                              static_cast<int64_t>(minute) * 60LL +
                              static_cast<int64_t>(second);
        return (daySeconds + timeSeconds) * 1000000LL;
    }

    static int64_t DateToMicros(int year, int month, int day) {
        return DateTimeToMicros(year, month, day, 0, 0, 0);
    }
};

// ======================================================================
// Basic functionality tests (matching Velox test cases)
// ======================================================================

TEST(MonthsBetweenTest, BasicWithRoundOff) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {3.94959677};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

TEST(MonthsBetweenTest, BasicWithoutRoundOff) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {false};

    std::vector<double> expected = {3.9495967741935485};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

TEST(MonthsBetweenTest, WithSecondsComponent) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 10, 30, 0, 0, 5)
    };
    std::vector<bool> roundOffValues = {false};

    std::vector<double> expected = {3.9495949074074073};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Negative result test (reversed argument order)
// ======================================================================

TEST(MonthsBetweenTest, NegativeResult) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 10, 30, 0, 0, 5)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<bool> roundOffValues = {false};

    std::vector<double> expected = {-3.9495949074074073};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Both last day of month test
// ======================================================================

TEST(MonthsBetweenTest, BothLastDayOfMonth) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 3, 31, 11, 0, 0)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {11.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Same day of month tests
// ======================================================================

TEST(MonthsBetweenTest, SameDayOfMonth) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 21, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 3, 28, 11, 0, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 3, 21, 11, 0, 0)
    };
    std::vector<bool> roundOffValues = {true, true};

    std::vector<double> expected = {11.0, 11.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Same timestamp test (zero result)
// ======================================================================

TEST(MonthsBetweenTest, SameTimestamp) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2024, 6, 15, 12, 30, 45)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2024, 6, 15, 12, 30, 45)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {0.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Epoch timestamp test
// ======================================================================

TEST(MonthsBetweenTest, EpochTimestamp) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1970, 7, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1970, 1, 1, 0, 0, 0)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {6.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// OMNI_LONG type tests (equivalent to OMNI_TIMESTAMP)
// ======================================================================

TEST(MonthsBetweenTest, LongTypeInput) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {3.94959677};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateLongVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateLongVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_LONG, OMNI_LONG, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Mixed type tests (OMNI_TIMESTAMP + OMNI_LONG)
// ======================================================================

TEST(MonthsBetweenTest, MixedTypeTimestampAndLong) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {3.94959677};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateLongVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_LONG, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

TEST(MonthsBetweenTest, MixedTypeLongAndTimestamp) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {3.94959677};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateLongVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_LONG, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// NULL handling tests
// ======================================================================

TEST(MonthsBetweenTest, NullTimestamp1) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30),
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30),
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true, true, true};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);

    ts1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (timestamp1 is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(MonthsBetweenTest, NullTimestamp2) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30),
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30),
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30)
    };
    std::vector<bool> roundOffValues = {true, true, true};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);

    ts2Vec->SetNull(0);

    BaseVector* resultVec = nullptr;
    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (timestamp2 is NULL)";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(MonthsBetweenTest, AllNull) {
    std::vector<int64_t> ts1Values = {0, 0};
    std::vector<int64_t> ts2Values = {0, 0};
    std::vector<bool> roundOffValues = {true, true};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);

    ts1Vec->SetNull(0);
    ts1Vec->SetNull(1);

    BaseVector* resultVec = nullptr;
    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";

    delete resultVec;
}

// ======================================================================
// Multi-row batch test
// ======================================================================

TEST(MonthsBetweenTest, MultiRowBatch) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 10, 30, 0, 0, 5),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 21, 10, 30, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateToMicros(1996, 10, 30),
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 10, 30, 0, 0, 5),
        MonthsBetweenTestHelper::DateTimeToMicros(1997, 2, 28, 10, 30, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 3, 31, 11, 0, 0),
        MonthsBetweenTestHelper::DateTimeToMicros(1996, 3, 21, 11, 0, 0)
    };
    std::vector<bool> roundOffValues = {true, false, false, true, true};

    std::vector<double> expected = {
        3.94959677,
        3.9495949074074073,
        -3.9495949074074073,
        11.0,
        11.0
    };

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Cross-year boundary test
// ======================================================================

TEST(MonthsBetweenTest, CrossYearBoundary) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2025, 1, 15, 0, 0, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2024, 11, 15, 0, 0, 0)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {2.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Pre-epoch test
// ======================================================================

TEST(MonthsBetweenTest, PreEpochTimestamp) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1970, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(1969, 7, 1, 0, 0, 0)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {6.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}

// ======================================================================
// Single row test
// ======================================================================

TEST(MonthsBetweenTest, SingleRow) {
    std::vector<int64_t> ts1Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2024, 12, 31, 23, 59, 59)
    };
    std::vector<int64_t> ts2Values = {
        MonthsBetweenTestHelper::DateTimeToMicros(2024, 1, 31, 0, 0, 0)
    };
    std::vector<bool> roundOffValues = {true};

    std::vector<double> expected = {11.0};

    BaseVector* ts1Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts1Values);
    BaseVector* ts2Vec = MonthsBetweenTestHelper::CreateTimestampVector(ts2Values);
    BaseVector* roundOffVec = MonthsBetweenTestHelper::CreateBoolVector(roundOffValues);
    BaseVector* resultVec = nullptr;

    MonthsBetweenTestHelper::ExecuteMonthsBetween(ts1Vec, ts2Vec, roundOffVec,
        OMNI_TIMESTAMP, OMNI_TIMESTAMP, resultVec);
    MonthsBetweenTestHelper::ValidateDoubleResult(resultVec, expected, ts1Values.size());

    delete resultVec;
}
