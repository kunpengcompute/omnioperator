/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for uuid function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <regex>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/Misc.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "codegen/func_signature.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class UuidTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }

    // Helper: validate UUID v4 format xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    static bool IsValidUuidV4(const std::string& uuid) {
        if (uuid.length() != 36) {
            return false;
        }
        // Check format: 8-4-4-4-12 hex digits separated by hyphens
        std::regex uuidPattern(
            "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
        return std::regex_match(uuid, uuidPattern);
    }

    // Helper: create the uuid VectorFunction with given seed and partition ID
    static std::shared_ptr<VectorFunction> CreateUuidFunction(
            int64_t seed, int32_t partitionId) {
        auto signature = std::make_shared<FunctionSignature>(
            "uuid", std::vector<DataTypeId>{OMNI_LONG}, OMNI_VARCHAR);

        // Create constant input for seed (used only during initialization)
        auto* seedConst = new ConstVector<int64_t>(seed, OMNI_LONG);

        // Create QueryConfig with partition ID
        std::unordered_map<std::string, std::string> configValues;
        configValues["spark.partition_id"] = std::to_string(partitionId);
        config::QueryConfig queryConfig(configValues);

        std::vector<BaseVector*> constantInputs = {seedConst};
        auto function = VectorFunction::Find(signature, constantInputs, queryConfig);

        delete seedConst;
        return function;
    }

    // Helper: create a ConstVector<int64_t> for use as Apply arg
    // Sets IsField(true) to prevent ConstVectorReader destructor from deleting it
    static ConstVector<int64_t>* CreateSeedArgVector(int64_t seed) {
        auto* seedVec = new ConstVector<int64_t>(seed, OMNI_LONG);
        seedVec->SetIsField(true);
        return seedVec;
    }
};

// Test basic uuid generation: output format must be valid UUID v4
TEST_F(UuidTest, BasicUuidFormat) {
    int64_t seed = 12345;
    int32_t partitionId = 0;
    constexpr int rowSize = 5;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr) << "uuid function not found";

    // Create seed input vector (constant) for Apply args
    auto* seedVec = CreateSeedArgVector(seed);

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    // Verify each row is a valid UUID v4 string
    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid))
            << "Row " << i << " is not a valid UUID v4: " << uuid;
    }

    delete resultVector;
    delete seedVec;
}

// Test that each row in the same batch gets a different UUID
TEST_F(UuidTest, UniqueUuidsPerRow) {
    int64_t seed = 42;
    int32_t partitionId = 0;
    constexpr int rowSize = 100;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);

    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    // Collect all UUIDs and verify uniqueness
    std::unordered_set<std::string> uuids;
    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid)) << "Invalid UUID at row " << i << ": " << uuid;
        auto [it, inserted] = uuids.insert(uuid);
        EXPECT_TRUE(inserted)
            << "Duplicate UUID found at row " << i << ": " << uuid;
    }

    delete resultVector;
    delete seedVec;
}

// Test determinism: same seed + same partition ID should produce the same sequence
TEST_F(UuidTest, DeterministicWithSameSeedAndPartition) {
    int64_t seed = 99999;
    int32_t partitionId = 7;
    constexpr int rowSize = 10;

    // First execution
    auto function1 = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function1, nullptr);

    auto* seedVec1 = CreateSeedArgVector(seed);
    ExecutionContext context1(4096, nullptr);
    context1.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args1;
    args1.push(seedVec1);

    BaseVector* result1 = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function1->Apply(args1, varcharType, result1, &context1);
    ASSERT_NE(result1, nullptr);

    auto* outVec1 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result1);
    ASSERT_NE(outVec1, nullptr);

    // Collect first batch results
    std::vector<std::string> firstBatch;
    for (int i = 0; i < rowSize; ++i) {
        firstBatch.emplace_back(outVec1->GetValue(i));
    }

    // Second execution with same seed and partition
    auto function2 = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function2, nullptr);

    auto* seedVec2 = CreateSeedArgVector(seed);
    ExecutionContext context2(4096, nullptr);
    context2.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args2;
    args2.push(seedVec2);

    BaseVector* result2 = nullptr;
    function2->Apply(args2, varcharType, result2, &context2);
    ASSERT_NE(result2, nullptr);

    auto* outVec2 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result2);
    ASSERT_NE(outVec2, nullptr);

    // Verify both batches produce the same sequence
    for (int i = 0; i < rowSize; ++i) {
        std::string uuid2(outVec2->GetValue(i));
        EXPECT_EQ(firstBatch[i], uuid2)
            << "UUID mismatch at row " << i
            << ": first=" << firstBatch[i] << ", second=" << uuid2;
    }

    delete result1;
    delete result2;
    delete seedVec1;
    delete seedVec2;
}

// Test that different seeds produce different UUIDs
TEST_F(UuidTest, DifferentSeedsProduceDifferentUuids) {
    int64_t seed1 = 111;
    int64_t seed2 = 222;
    int32_t partitionId = 0;
    constexpr int rowSize = 5;

    // First seed
    auto function1 = CreateUuidFunction(seed1, partitionId);
    ASSERT_NE(function1, nullptr);

    auto* seedVec1 = CreateSeedArgVector(seed1);
    ExecutionContext context1(4096, nullptr);
    context1.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args1;
    args1.push(seedVec1);

    BaseVector* result1 = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function1->Apply(args1, varcharType, result1, &context1);
    ASSERT_NE(result1, nullptr);

    auto* outVec1 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result1);
    ASSERT_NE(outVec1, nullptr);

    // Second seed
    auto function2 = CreateUuidFunction(seed2, partitionId);
    ASSERT_NE(function2, nullptr);

    auto* seedVec2 = CreateSeedArgVector(seed2);
    ExecutionContext context2(4096, nullptr);
    context2.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args2;
    args2.push(seedVec2);

    BaseVector* result2 = nullptr;
    function2->Apply(args2, varcharType, result2, &context2);
    ASSERT_NE(result2, nullptr);

    auto* outVec2 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result2);
    ASSERT_NE(outVec2, nullptr);

    // At least one UUID should differ between the two seeds
    bool anyDifferent = false;
    for (int i = 0; i < rowSize; ++i) {
        std::string u1(outVec1->GetValue(i));
        std::string u2(outVec2->GetValue(i));
        if (u1 != u2) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent)
        << "Different seeds should produce different UUID sequences";

    delete result1;
    delete result2;
    delete seedVec1;
    delete seedVec2;
}

// Test that different partition IDs with the same seed produce different UUIDs
TEST_F(UuidTest, DifferentPartitionsProduceDifferentUuids) {
    int64_t seed = 12345;
    int32_t partitionId1 = 0;
    int32_t partitionId2 = 1;
    constexpr int rowSize = 5;

    // Partition 0
    auto function1 = CreateUuidFunction(seed, partitionId1);
    ASSERT_NE(function1, nullptr);

    auto* seedVec1 = CreateSeedArgVector(seed);
    ExecutionContext context1(4096, nullptr);
    context1.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args1;
    args1.push(seedVec1);

    BaseVector* result1 = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function1->Apply(args1, varcharType, result1, &context1);
    ASSERT_NE(result1, nullptr);

    auto* outVec1 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result1);
    ASSERT_NE(outVec1, nullptr);

    // Partition 1
    auto function2 = CreateUuidFunction(seed, partitionId2);
    ASSERT_NE(function2, nullptr);

    auto* seedVec2 = CreateSeedArgVector(seed);
    ExecutionContext context2(4096, nullptr);
    context2.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args2;
    args2.push(seedVec2);

    BaseVector* result2 = nullptr;
    function2->Apply(args2, varcharType, result2, &context2);
    ASSERT_NE(result2, nullptr);

    auto* outVec2 = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result2);
    ASSERT_NE(outVec2, nullptr);

    // At least one UUID should differ between different partitions
    bool anyDifferent = false;
    for (int i = 0; i < rowSize; ++i) {
        std::string u1(outVec1->GetValue(i));
        std::string u2(outVec2->GetValue(i));
        if (u1 != u2) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifferent)
        << "Different partitions should produce different UUID sequences";

    delete result1;
    delete result2;
    delete seedVec1;
    delete seedVec2;
}

// Test uuid with single row
TEST_F(UuidTest, SingleRow) {
    int64_t seed = 0;
    int32_t partitionId = 0;
    constexpr int rowSize = 1;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    std::string uuid(outVec->GetValue(0));
    EXPECT_TRUE(IsValidUuidV4(uuid)) << "Single row UUID is invalid: " << uuid;
    EXPECT_EQ(uuid.length(), 36u);

    delete resultVector;
    delete seedVec;
}

// Test uuid with large batch
TEST_F(UuidTest, LargeBatch) {
    int64_t seed = 777;
    int32_t partitionId = 3;
    constexpr int rowSize = 1000;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    // Verify all UUIDs are valid and unique
    std::unordered_set<std::string> uuids;
    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid))
            << "Invalid UUID at row " << i << ": " << uuid;
        uuids.insert(uuid);
    }
    EXPECT_EQ(uuids.size(), static_cast<size_t>(rowSize))
        << "Expected all " << rowSize << " UUIDs to be unique, got " << uuids.size();

    delete resultVector;
    delete seedVec;
}

// Test UUID v4 version and variant bits
TEST_F(UuidTest, VersionAndVariantBits) {
    int64_t seed = 54321;
    int32_t partitionId = 0;
    constexpr int rowSize = 20;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        // UUID format: xxxxxxxx-xxxx-Vxxx-Txxx-xxxxxxxxxxxx
        // V should be '4' (version 4)
        EXPECT_EQ(uuid[14], '4')
            << "Row " << i << ": version nibble should be '4', got '" << uuid[14]
            << "' in UUID: " << uuid;

        // T should be one of '8', '9', 'a', 'b' (variant bits 10xx)
        char variant = uuid[19];
        EXPECT_TRUE(variant == '8' || variant == '9' || variant == 'a' || variant == 'b')
            << "Row " << i << ": variant nibble should be 8/9/a/b, got '"
            << variant << "' in UUID: " << uuid;
    }

    delete resultVector;
    delete seedVec;
}

// Test uuid with zero seed
TEST_F(UuidTest, ZeroSeed) {
    int64_t seed = 0;
    int32_t partitionId = 0;
    constexpr int rowSize = 5;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid))
            << "Row " << i << " with zero seed is not valid UUID v4: " << uuid;
    }

    delete resultVector;
    delete seedVec;
}

// Test uuid with negative seed
TEST_F(UuidTest, NegativeSeed) {
    int64_t seed = -12345;
    int32_t partitionId = 0;
    constexpr int rowSize = 5;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid))
            << "Row " << i << " with negative seed is not valid UUID v4: " << uuid;
    }

    delete resultVector;
    delete seedVec;
}

// Test uuid with large seed value
TEST_F(UuidTest, LargeSeedValue) {
    int64_t seed = INT64_MAX;
    int32_t partitionId = 100;
    constexpr int rowSize = 5;

    auto function = CreateUuidFunction(seed, partitionId);
    ASSERT_NE(function, nullptr);

    auto* seedVec = CreateSeedArgVector(seed);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    args.push(seedVec);

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    function->Apply(args, varcharType, resultVector, &context);
    ASSERT_NE(resultVector, nullptr);

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);

    for (int i = 0; i < rowSize; ++i) {
        std::string uuid(outVec->GetValue(i));
        EXPECT_TRUE(IsValidUuidV4(uuid))
            << "Row " << i << " with large seed is not valid UUID v4: " << uuid;
    }

    delete resultVector;
    delete seedVec;
}
