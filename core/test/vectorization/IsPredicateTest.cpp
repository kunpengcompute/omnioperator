/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/IsBooleanFunction.h"
#include "vectorization/functions/IsNotUnknownFunction.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class IsPredicateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const is_predicate_test_env =
    ::testing::AddGlobalTestEnvironment(new IsPredicateTestEnvironment);

// Behavior of each IS [NOT] {TRUE|FALSE|UNKNOWN} function is fully described by
// the output produced for the three possible input states (TRUE / FALSE / NULL).
struct PredicateTestParam {
    std::string funcName;
    bool resultForTrue;
    bool resultForFalse;
    bool resultForNull;

    PredicateTestParam(std::string name, bool t, bool f, bool n)
        : funcName(std::move(name)), resultForTrue(t), resultForFalse(f), resultForNull(n) {}
};

class IsPredicateFunctionTest : public ::testing::TestWithParam<PredicateTestParam> {
protected:
    static BaseVector* CreateBoolVector(const std::vector<bool>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, values.size());
        auto* typedVec = static_cast<Vector<bool>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteFunction(const std::string& funcName, BaseVector* inputVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName,
            std::vector<DataTypeId>{OMNI_BOOLEAN}, OMNI_BOOLEAN);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found";

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    }

    // Builds the input vector from `inputValues`, marks the rows listed in
    // `nullRows` as NULL, executes the function under test, and validates the
    // output against the expected values derived from GetParam(). These
    // predicate functions never produce a NULL output.
    void RunScenario(const std::vector<bool>& inputValues, const std::vector<int>& nullRows) {
        const auto& param = GetParam();

        BaseVector* inputVec = CreateBoolVector(inputValues);
        for (int row : nullRows) {
            inputVec->SetNull(row);
        }

        BaseVector* resultVec = nullptr;
        ExecuteFunction(param.funcName, inputVec, resultVec);

        auto* resultTypedVec = dynamic_cast<Vector<bool>*>(resultVec);
        ASSERT_NE(resultTypedVec, nullptr);

        auto isNullRow = [&](int i) {
            return std::find(nullRows.begin(), nullRows.end(), i) != nullRows.end();
        };

        for (size_t i = 0; i < inputValues.size(); ++i) {
            EXPECT_FALSE(resultVec->IsNull(static_cast<int>(i)))
                << param.funcName << " row " << i << " should not be NULL";
            bool expected = isNullRow(static_cast<int>(i))
                ? param.resultForNull
                : (inputValues[i] ? param.resultForTrue : param.resultForFalse);
            EXPECT_EQ(resultTypedVec->GetValue(static_cast<int>(i)), expected)
                << param.funcName << " row " << i << " value mismatch";
        }

        delete resultVec;
    }
};

const std::vector<PredicateTestParam>& PredicateCases() {
    // name,            TRUE,  FALSE, NULL
    static const std::vector<PredicateTestParam> cases = {
        {"is_false",       false, true,  false}, // IS FALSE:        NULL->false, TRUE->false, FALSE->true
        {"is_not_false",   true,  false, true},  // IS NOT FALSE:    NULL->true,  TRUE->true,  FALSE->false
        {"is_not_true",    false, true,  true},  // IS NOT TRUE:     NULL->true,  TRUE->false, FALSE->true
        {"is_not_unknown", true,  true,  false}, // IS NOT UNKNOWN:  NULL->false, TRUE->true,  FALSE->true
    };
    return cases;
}

INSTANTIATE_TEST_SUITE_P(IsPredicateFunctions, IsPredicateFunctionTest,
    ::testing::ValuesIn(PredicateCases()),
    [](const testing::TestParamInfo<PredicateTestParam>& info) {
        return info.param.funcName;
    });

TEST_P(IsPredicateFunctionTest, BoolNormalValue) {
    RunScenario({true, false, true, false}, {});
}

TEST_P(IsPredicateFunctionTest, BoolAllTrue) {
    RunScenario({true, true, true}, {});
}

TEST_P(IsPredicateFunctionTest, BoolAllFalse) {
    RunScenario({false, false, false}, {});
}

TEST_P(IsPredicateFunctionTest, BoolNullInput) {
    RunScenario({false, false, false}, {0, 1, 2});
}

TEST_P(IsPredicateFunctionTest, BoolMixedValues) {
    RunScenario({true, false, true, false, true, false, true}, {5});
}

TEST_P(IsPredicateFunctionTest, BoolSingleTrue) {
    RunScenario({true}, {});
}

TEST_P(IsPredicateFunctionTest, BoolSingleFalse) {
    RunScenario({false}, {});
}

TEST_P(IsPredicateFunctionTest, BoolSingleNull) {
    RunScenario({false}, {0});
}

TEST_P(IsPredicateFunctionTest, BoolEmptyVector) {
    RunScenario({}, {});
}
