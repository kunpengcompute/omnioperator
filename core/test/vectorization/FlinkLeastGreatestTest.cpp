/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: unit tests for flink_greatest / flink_least
 *
 * Flink's GREATEST/LEAST (ScalarOperatorGens.generateGreatestLeast) return NULL as soon as one
 * argument is NULL, while Spark's "Greatest"/"Least" skip NULL arguments. Only the NULL
 * behaviour differs, so the value selection tests stay in LeastGreatestTest.cpp; the tests here
 * cover NULL propagation on each of the three comparison paths (numeric, string, boolean) plus
 * the constant encoding, which is how Flink literals reach the function.
 */

#include <gtest/gtest.h>
#include <limits>
#include <stack>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/LeastGreatest.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

namespace {
template <typename T>
BaseVector *MakeVector(DataTypeId typeId, const std::vector<T> &values, const std::vector<bool> &nulls)
{
    auto rowSize = static_cast<int32_t>(values.size());
    BaseVector *vec = VectorHelper::CreateFlatVector(typeId, rowSize);
    auto *typed = static_cast<Vector<T> *>(vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (nulls[i]) {
            vec->SetNull(i);
        } else {
            typed->SetValue(i, values[i]);
            vec->SetNotNull(i);
        }
    }
    return vec;
}

BaseVector *MakeStringVector(const std::vector<std::string> &values, const std::vector<bool> &nulls)
{
    auto rowSize = static_cast<int32_t>(values.size());
    BaseVector *vec = VectorHelper::CreateStringVector(rowSize);
    auto *typed = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    for (int32_t i = 0; i < rowSize; ++i) {
        if (nulls[i]) {
            vec->SetNull(i);
        } else {
            std::string_view sv(values[i]);
            typed->SetValue(i, sv);
            vec->SetNotNull(i);
        }
    }
    return vec;
}

/// Applies the function in one shot, the way Flink emits it: a single N-ary call with the
/// arguments pushed left to right, so the last operand is on top of the stack.
void ApplyLeastGreatest(const std::string &funcName, const std::vector<BaseVector *> &inputs,
    DataTypeId outputTypeId, BaseVector *&result)
{
    link_register_functions();

    std::vector<DataTypeId> inputTypes;
    inputTypes.reserve(inputs.size());
    for (const auto *input : inputs) {
        inputTypes.push_back(input->GetTypeId());
    }
    auto signature = std::make_shared<FunctionSignature>(funcName, inputTypes, outputTypeId);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << funcName << " not found for the given signature";

    ExecutionContext context;
    context.SetResultRowSize(inputs[0]->GetSize());
    std::stack<BaseVector *> args;
    for (auto *input : inputs) {
        args.push(input);
    }
    auto outputType = std::make_shared<DataType>(outputTypeId);
    ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    EXPECT_TRUE(args.empty()) << "all " << inputs.size() << " arguments should have been popped";
    ASSERT_NE(result, nullptr);
}

template <typename T>
void ExpectValues(BaseVector *result, const std::vector<T> &expected, const std::vector<bool> &expectedNulls)
{
    ASSERT_EQ(result->GetSize(), static_cast<int32_t>(expected.size()));
    auto *typed = static_cast<Vector<T> *>(result);
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
            continue;
        }
        ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(typed->GetValue(i), expected[i]) << "Row " << i << " mismatch";
    }
}

void ExpectStrings(BaseVector *result, const std::vector<std::string> &expected,
    const std::vector<bool> &expectedNulls)
{
    ASSERT_EQ(result->GetSize(), static_cast<int32_t>(expected.size()));
    auto *typed = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
            continue;
        }
        ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
        EXPECT_EQ(std::string(typed->GetValue(i)), expected[i]) << "Row " << i << " mismatch";
    }
}

// Rows: no NULL, NULL in the middle argument, NULL in the first, all NULL.
const std::vector<int64_t> LONG_ARG1 = {100, 0, 0, 0};
const std::vector<int64_t> LONG_ARG2 = {5, 0, 0, 0};
const std::vector<int64_t> LONG_ARG3 = {50, 5, 5, 0};
const std::vector<bool> LONG_NULLS1 = {false, false, true, true};
const std::vector<bool> LONG_NULLS2 = {false, true, false, true};
const std::vector<bool> LONG_NULLS3 = {false, false, false, true};
const std::vector<bool> LONG_EXPECTED_NULLS = {false, true, true, true};
} // namespace

// ==================== Numeric path ====================

TEST(FlinkGreatestTest, LongNullPropagates)
{
    BaseVector *arg1 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG1, LONG_NULLS1);
    BaseVector *arg2 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG2, LONG_NULLS2);
    BaseVector *arg3 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG3, LONG_NULLS3);

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {arg1, arg2, arg3}, OMNI_LONG, result);
    ExpectValues<int64_t>(result, {100, 0, 0, 0}, LONG_EXPECTED_NULLS);

    delete arg1;
    delete arg2;
    delete arg3;
    delete result;
}

TEST(FlinkLeastTest, LongNullPropagates)
{
    BaseVector *arg1 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG1, LONG_NULLS1);
    BaseVector *arg2 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG2, LONG_NULLS2);
    BaseVector *arg3 = MakeVector<int64_t>(OMNI_LONG, LONG_ARG3, LONG_NULLS3);

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_least", {arg1, arg2, arg3}, OMNI_LONG, result);
    ExpectValues<int64_t>(result, {5, 0, 0, 0}, LONG_EXPECTED_NULLS);

    delete arg1;
    delete arg2;
    delete arg3;
    delete result;
}

TEST(FlinkGreatestTest, IntTwoArgsNullPropagates)
{
    std::vector<int32_t> vals1 = {7, 7, -1};
    std::vector<int32_t> vals2 = {3, 0, 0};
    BaseVector *arg1 = MakeVector<int32_t>(OMNI_INT, vals1, {false, false, true});
    BaseVector *arg2 = MakeVector<int32_t>(OMNI_INT, vals2, {false, true, false});

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {arg1, arg2}, OMNI_INT, result);
    ExpectValues<int32_t>(result, {7, 0, 0}, {false, true, true});

    delete arg1;
    delete arg2;
    delete result;
}

// ==================== String path ====================

TEST(FlinkGreatestTest, StringNullPropagates)
{
    std::vector<std::string> vals1 = {"apple", "apple", ""};
    std::vector<std::string> vals2 = {"cherry", "", "cherry"};
    BaseVector *arg1 = MakeStringVector(vals1, {false, false, true});
    BaseVector *arg2 = MakeStringVector(vals2, {false, true, false});

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {arg1, arg2}, OMNI_VARCHAR, result);
    ExpectStrings(result, {"cherry", "", ""}, {false, true, true});

    delete arg1;
    delete arg2;
    delete result;
}

TEST(FlinkLeastTest, StringNullPropagates)
{
    std::vector<std::string> vals1 = {"apple", "apple", ""};
    std::vector<std::string> vals2 = {"cherry", "", "cherry"};
    BaseVector *arg1 = MakeStringVector(vals1, {false, false, true});
    BaseVector *arg2 = MakeStringVector(vals2, {false, true, false});

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_least", {arg1, arg2}, OMNI_VARCHAR, result);
    ExpectStrings(result, {"apple", "", ""}, {false, true, true});

    delete arg1;
    delete arg2;
    delete result;
}

// ==================== Boolean path ====================

TEST(FlinkGreatestTest, BooleanNullPropagates)
{
    std::vector<bool> vals1 = {false, true, false};
    std::vector<bool> vals2 = {true, false, false};
    BaseVector *arg1 = MakeVector<bool>(OMNI_BOOLEAN, vals1, {false, false, true});
    BaseVector *arg2 = MakeVector<bool>(OMNI_BOOLEAN, vals2, {false, true, false});

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {arg1, arg2}, OMNI_BOOLEAN, result);
    ExpectValues<bool>(result, {true, false, false}, {false, true, true});

    delete arg1;
    delete arg2;
    delete result;
}

TEST(FlinkLeastTest, BooleanNullPropagates)
{
    std::vector<bool> vals1 = {false, true, false};
    std::vector<bool> vals2 = {true, false, false};
    BaseVector *arg1 = MakeVector<bool>(OMNI_BOOLEAN, vals1, {false, false, true});
    BaseVector *arg2 = MakeVector<bool>(OMNI_BOOLEAN, vals2, {false, true, false});

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_least", {arg1, arg2}, OMNI_BOOLEAN, result);
    ExpectValues<bool>(result, {false, false, false}, {false, true, true});

    delete arg1;
    delete arg2;
    delete result;
}

// ==================== Constant encoding ====================

// A NULL literal arrives as a constant vector whose null flag only exists at index 0, so every
// row of the result must be NULL.
TEST(FlinkGreatestTest, ConstantNullArgMakesEveryRowNull)
{
    constexpr int32_t rowSize = 3;
    std::vector<int64_t> vals = {1, 2, 3};
    BaseVector *flatArg = MakeVector<int64_t>(OMNI_LONG, vals, {false, false, false});
    auto *constArg = new ConstVector<int64_t>(9, OMNI_LONG, rowSize);
    constArg->SetNull(0);

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {flatArg, constArg}, OMNI_LONG, result);
    ExpectValues<int64_t>(result, {0, 0, 0}, {true, true, true});

    delete flatArg;
    delete constArg;
    delete result;
}

// A non-NULL constant still takes part in the comparison on every row.
TEST(FlinkGreatestTest, ConstantArgComparedOnEveryRow)
{
    constexpr int32_t rowSize = 3;
    std::vector<int64_t> vals = {1, 20, 3};
    BaseVector *flatArg = MakeVector<int64_t>(OMNI_LONG, vals, {false, false, false});
    auto *constArg = new ConstVector<int64_t>(9, OMNI_LONG, rowSize);

    BaseVector *result = nullptr;
    ApplyLeastGreatest("flink_greatest", {flatArg, constArg}, OMNI_LONG, result);
    ExpectValues<int64_t>(result, {9, 20, 9}, {false, false, false});

    delete flatArg;
    delete constArg;
    delete result;
}

// ==================== Value selection unchanged ====================

// Without NULLs the Flink variants must pick exactly what Spark picks.
TEST(FlinkGreatestTest, WithoutNullsMatchesSpark)
{
    std::vector<int64_t> vals1 = {100, 1, std::numeric_limits<int64_t>::min()};
    std::vector<int64_t> vals2 = {5, 9999999999L, std::numeric_limits<int64_t>::max()};
    std::vector<int64_t> vals3 = {50, 5000000000L, 0};
    std::vector<bool> noNulls = {false, false, false};

    BaseVector *greatest = nullptr;
    BaseVector *arg1 = MakeVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector *arg2 = MakeVector<int64_t>(OMNI_LONG, vals2, noNulls);
    BaseVector *arg3 = MakeVector<int64_t>(OMNI_LONG, vals3, noNulls);
    ApplyLeastGreatest("flink_greatest", {arg1, arg2, arg3}, OMNI_LONG, greatest);
    ExpectValues<int64_t>(greatest, {100, 9999999999L, std::numeric_limits<int64_t>::max()}, noNulls);

    BaseVector *least = nullptr;
    BaseVector *arg4 = MakeVector<int64_t>(OMNI_LONG, vals1, noNulls);
    BaseVector *arg5 = MakeVector<int64_t>(OMNI_LONG, vals2, noNulls);
    BaseVector *arg6 = MakeVector<int64_t>(OMNI_LONG, vals3, noNulls);
    ApplyLeastGreatest("flink_least", {arg4, arg5, arg6}, OMNI_LONG, least);
    ExpectValues<int64_t>(least, {5, 1, std::numeric_limits<int64_t>::min()}, noNulls);

    delete arg1;
    delete arg2;
    delete arg3;
    delete arg4;
    delete arg5;
    delete arg6;
    delete greatest;
    delete least;
}
