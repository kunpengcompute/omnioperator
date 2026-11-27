/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Timestamp conversion functions unit tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/TimestampConversion.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"
#include "util/type_util.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class TimestampConversionTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const timestamp_conversion_test_env = 
    ::testing::AddGlobalTestEnvironment(new TimestampConversionTestEnvironment);

class TimestampConversionTestHelper {
public:
    static void ValidateTimestampResult(BaseVector* result, const std::vector<int64_t>& expectedMicros, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            int64_t actualValue = resultVec->GetValue(i);
            int64_t expectedValue = expectedMicros[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }
    
    static BaseVector* CreateInputVector(DataTypeId typeId, const std::vector<int64_t>& intValues) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, intValues.size());
        
        // Handle different types correctly to avoid buffer overflow
        switch (typeId) {
            case OMNI_BYTE: {
                auto* typedVec = static_cast<Vector<int8_t>*>(vec);
                for (size_t i = 0; i < intValues.size(); ++i) {
                    typedVec->SetValue(i, static_cast<int8_t>(intValues[i]));
                }
                break;
            }
            case OMNI_SHORT: {
                auto* typedVec = static_cast<Vector<int16_t>*>(vec);
                for (size_t i = 0; i < intValues.size(); ++i) {
                    typedVec->SetValue(i, static_cast<int16_t>(intValues[i]));
                }
                break;
            }
            case OMNI_INT:
            case OMNI_DATE32: {
                auto* typedVec = static_cast<Vector<int32_t>*>(vec);
                for (size_t i = 0; i < intValues.size(); ++i) {
                    typedVec->SetValue(i, static_cast<int32_t>(intValues[i]));
                }
                break;
            }
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DATE64:
            case OMNI_DECIMAL64: {
                auto* typedVec = static_cast<Vector<int64_t>*>(vec);
                for (size_t i = 0; i < intValues.size(); ++i) {
                    typedVec->SetValue(i, intValues[i]);
                }
                break;
            }
            default: {
                // For unsupported types, try to use int64_t as fallback
                // This should not happen in normal test cases
                auto* typedVec = static_cast<Vector<int64_t>*>(vec);
                for (size_t i = 0; i < intValues.size(); ++i) {
                    typedVec->SetValue(i, intValues[i]);
                }
                break;
            }
        }
        return vec;
    }
    
    static BaseVector* CreateDoubleVector(const std::vector<double>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, values.size());
        auto* typedVec = static_cast<Vector<double>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteTimestampFunction(const std::string& funcName, BaseVector* inputVec, 
                                         DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName, 
            std::vector<DataTypeId>{inputTypeId}, OMNI_TIMESTAMP);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found for signature";
        
        auto outputType = TimestampType();
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << funcName << " function threw an exception";
    }
    
    // Helper to convert timestamp to microseconds for comparison
    static int64_t TimestampToMicros(int64_t seconds, int64_t nanos) {
        return seconds * 1000000LL + nanos / 1000LL;
    }
};

// ========== timestamp_micros Tests ==========

TEST(TimestampConversionTest, TimestampMicros_Basic) {
    // Test values: 1 second, 2008-12-25 15:30:00.123123, epoch
    std::vector<int64_t> inputValues = {1000000LL, 1230219000123123LL, 0LL};
    std::vector<int64_t> expectedMicros = {1000000LL, 1230219000123123LL, 0LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", inputVec, OMNI_LONG, resultVec);
    TimestampConversionTestHelper::ValidateTimestampResult(resultVec, expectedMicros, inputValues.size());
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampMicros_WithNull) {
    std::vector<int64_t> inputValues = {1000000LL, 0LL, 1230219000123123LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    inputVec->SetNull(1);  // Set middle value to NULL
    
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", inputVec, OMNI_LONG, resultVec);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampMicros_DifferentTypes) {
    // Test BYTE
    std::vector<int64_t> byteValues = {127LL};
    BaseVector* byteVec = TimestampConversionTestHelper::CreateInputVector(OMNI_BYTE, byteValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", byteVec, OMNI_BYTE, resultVec);
    EXPECT_FALSE(resultVec->IsNull(0));
    // byteVec is deleted by Apply function, don't delete here
    delete resultVec;
    
    // Test SHORT
    std::vector<int64_t> shortValues = {32767LL};
    BaseVector* shortVec = TimestampConversionTestHelper::CreateInputVector(OMNI_SHORT, shortValues);
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", shortVec, OMNI_SHORT, resultVec);
    EXPECT_FALSE(resultVec->IsNull(0));
    // shortVec is deleted by Apply function, don't delete here
    delete resultVec;
    
    // Test INT
    std::vector<int64_t> intValues = {2147483647LL};
    BaseVector* intVec = TimestampConversionTestHelper::CreateInputVector(OMNI_INT, intValues);
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", intVec, OMNI_INT, resultVec);
    EXPECT_FALSE(resultVec->IsNull(0));
    // intVec is deleted by Apply function, don't delete here
    delete resultVec;
}

// ========== timestamp_millis Tests ==========

TEST(TimestampConversionTest, TimestampMillis_Basic) {
    // Test values: 1 second, 2008-12-25 15:30:00.123
    std::vector<int64_t> inputValues = {1000LL, 1230219000123LL};
    // Expected: 1000 millis = 1000000 micros, 1230219000123 millis = 1230219000123000 micros
    std::vector<int64_t> expectedMicros = {1000000LL, 1230219000123000LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_millis", inputVec, OMNI_LONG, resultVec);
    TimestampConversionTestHelper::ValidateTimestampResult(resultVec, expectedMicros, inputValues.size());
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampMillis_WithNull) {
    std::vector<int64_t> inputValues = {1000LL, 0LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_millis", inputVec, OMNI_LONG, resultVec);
    
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampMillis_DifferentTypes) {
    // Test BYTE
    std::vector<int64_t> byteValues = {127LL};
    BaseVector* byteVec = TimestampConversionTestHelper::CreateInputVector(OMNI_BYTE, byteValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_millis", byteVec, OMNI_BYTE, resultVec);
    EXPECT_FALSE(resultVec->IsNull(0));
    // byteVec is deleted by Apply function, don't delete here
    delete resultVec;
}

// ========== timestamp_seconds Tests ==========

TEST(TimestampConversionTest, TimestampSeconds_Integral) {
    // Test values: 1 second, 2008-12-25 15:30:00
    std::vector<int64_t> inputValues = {1LL, 1230219000LL};
    // Expected: 1 second = 1000000 micros, 1230219000 seconds = 1230219000000000 micros
    std::vector<int64_t> expectedMicros = {1000000LL, 1230219000000000LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", inputVec, OMNI_LONG, resultVec);
    TimestampConversionTestHelper::ValidateTimestampResult(resultVec, expectedMicros, inputValues.size());
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampSeconds_FloatingPoint) {
    // Test values: 1.0, 1230219000.123, infinity
    std::vector<double> inputValues = {1.0, 1230219000.123, std::numeric_limits<double>::infinity()};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateDoubleVector(inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", inputVec, OMNI_DOUBLE, resultVec);
    
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_FALSE(resultVec->IsNull(1)) << "Row 1 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 (infinity) should be NULL";
    
    // Verify first value
    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t ts0 = resultVecTyped->GetValue(0);
    Timestamp timestamp0 = Timestamp::fromMicros(ts0);
    EXPECT_EQ(timestamp0.getSeconds(), 1);
    EXPECT_EQ(timestamp0.getNanos(), 0);
    
    // Verify second value (should have fractional part)
    int64_t ts1 = resultVecTyped->GetValue(1);
    Timestamp timestamp1 = Timestamp::fromMicros(ts1);
    EXPECT_EQ(timestamp1.getSeconds(), 1230219000);
    EXPECT_GT(timestamp1.getNanos(), 0) << "Fractional part should be preserved";
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampSeconds_WithNull) {
    std::vector<int64_t> inputValues = {1LL, 0LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", inputVec, OMNI_LONG, resultVec);
    
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampSeconds_SpecialFloatingPoint) {
    std::vector<double> inputValues = {
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()
    };
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateDoubleVector(inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", inputVec, OMNI_DOUBLE, resultVec);
    
    // All special values should result in NULL
    EXPECT_TRUE(resultVec->IsNull(0)) << "Infinity should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Negative infinity should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "NaN should be NULL";
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampSeconds_DifferentTypes) {
    // Test FLOAT
    std::vector<double> floatValues = {1.0};
    BaseVector* floatVec = VectorHelper::CreateFlatVector(nullptr, OMNI_FLOAT, 1);
    auto* floatTypedVec = static_cast<Vector<float>*>(floatVec);
    floatTypedVec->SetValue(0, 1.0f);
    
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", floatVec, OMNI_FLOAT, resultVec);
    EXPECT_FALSE(resultVec->IsNull(0));
    // floatVec is deleted by Apply function, don't delete here
    delete resultVec;
}

// ========== Edge Cases Tests ==========

TEST(TimestampConversionTest, TimestampMicros_EdgeCases) {
    // Test with large values
    std::vector<int64_t> inputValues = {
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::min()
    };
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_micros", inputVec, OMNI_LONG, resultVec);
    
    // Should not crash, but values might be clamped
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}

TEST(TimestampConversionTest, TimestampSeconds_NegativeValues) {
    // Test negative seconds (before epoch)
    std::vector<int64_t> inputValues = {-1LL, -1230219000LL};
    
    BaseVector* inputVec = TimestampConversionTestHelper::CreateInputVector(OMNI_LONG, inputValues);
    BaseVector* resultVec = nullptr;
    TimestampConversionTestHelper::ExecuteTimestampFunction("timestamp_seconds", inputVec, OMNI_LONG, resultVec);
    
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));
    
    // Verify first value (should be 1969-12-31 23:59:59)
    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t ts0 = resultVecTyped->GetValue(0);
    Timestamp timestamp0 = Timestamp::fromMicros(ts0);
    EXPECT_EQ(timestamp0.getSeconds(), -1);
    
    // inputVec is deleted by Apply function, don't delete here
    delete resultVec;
}
