/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ParseUrl function unit tests
 *   parse_url(url, part[, key]) -> varchar
 */

#include <gtest/gtest.h>
#include <optional>
#include <stack>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
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

class ParseUrlTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const parse_url_test_env =
    ::testing::AddGlobalTestEnvironment(new ParseUrlTestEnvironment);

class ParseUrlFunctionTestHelper {
public:
    using StringVector = Vector<LargeStringContainer<std::string_view>>;

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vector = VectorHelper::CreateStringVector(values.size());
        vector->SetIsField(true);
        auto* typedVector = dynamic_cast<StringVector*>(vector);
        EXPECT_NE(typedVector, nullptr);
        for (size_t row = 0; row < values.size(); ++row) {
            typedVector->SetValue(row, std::string_view(values[row]));
        }
        return vector;
    }

    static void ExecuteParseUrl(const std::vector<BaseVector*>& inputVectors, BaseVector*& result) {
        std::vector<DataTypeId> inputTypes;
        std::stack<BaseVector*> inputs;
        for (BaseVector* input : inputVectors) {
            inputTypes.push_back(input->GetTypeId());
            inputs.push(input);
        }

        auto signature = std::make_shared<FunctionSignature>("parse_url", inputTypes, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "parse_url function not found";

        ExecutionContext context;
        context.SetResultRowSize(inputVectors.front()->GetSize());
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ASSERT_NO_THROW(function->Apply(inputs, outputType, result, &context));
    }

    static void ValidateStringResult(BaseVector* result,
        const std::vector<std::optional<std::string>>& expected) {
        auto* resultVector = dynamic_cast<StringVector*>(result);
        ASSERT_NE(resultVector, nullptr) << "Result vector type mismatch";
        for (size_t row = 0; row < expected.size(); ++row) {
            if (!expected[row].has_value()) {
                EXPECT_TRUE(result->IsNull(row)) << "Row " << row << " expected NULL";
                continue;
            }
            ASSERT_FALSE(result->IsNull(row)) << "Row " << row << " expected non-NULL";
            EXPECT_EQ(std::string(resultVector->GetValue(row)), expected[row].value())
                << "Row " << row;
        }
    }
};

TEST(ParseUrlTest, ExtractsAllSupportedParts) {
    const std::string url =
        "http://user:pw@facebook.com/path1/p.php?k1=v1&k2=v2#Ref1";
    std::vector<std::string> urls(8, url);
    std::vector<std::string> parts = {
        "HOST", "PATH", "QUERY", "REF", "PROTOCOL", "AUTHORITY", "FILE", "USERINFO"
    };
    std::vector<std::optional<std::string>> expected = {
        "facebook.com",
        "/path1/p.php",
        "k1=v1&k2=v2",
        "Ref1",
        "http",
        "user:pw@facebook.com",
        "/path1/p.php?k1=v1&k2=v2",
        "user:pw"
    };

    BaseVector* urlVec = ParseUrlFunctionTestHelper::CreateStringVector(urls);
    BaseVector* partVec = ParseUrlFunctionTestHelper::CreateStringVector(parts);
    BaseVector* result = nullptr;
    ParseUrlFunctionTestHelper::ExecuteParseUrl({urlVec, partVec}, result);
    ParseUrlFunctionTestHelper::ValidateStringResult(result, expected);
    delete urlVec;
    delete partVec;
    delete result;
}

TEST(ParseUrlTest, MatchesJavaUrlParsingBoundaries) {
    std::vector<std::string> urls = {
        " \thttp://host/path \r\n",
        "url:HTTP://host/path",
        "http://a@b@host/path",
        "http://a@b@host/path",
        "http://[2001:db8::1]:+80/path",
        "http://[2001:db8:::1]/path",
        "http://host:2147483648/path",
        "http://host:-1/path",
        "mailto:user@example.com",
        "jar:http://host/a.jar!/entry.txt",
        "jar:http://host/a.jar/entry.txt",
        "http:////server/share"
    };
    std::vector<std::string> parts = {
        "PATH", "PROTOCOL", "HOST", "USERINFO", "HOST", "HOST",
        "HOST", "HOST", "PATH", "PATH", "PATH", "AUTHORITY"
    };
    std::vector<std::optional<std::string>> expected = {
        "/path",
        "http",
        "",
        std::nullopt,
        "[2001:db8::1]",
        std::nullopt,
        std::nullopt,
        "host",
        "user@example.com",
        "http://host/a.jar!/entry.txt",
        std::nullopt,
        std::nullopt
    };

    BaseVector* urlVec = ParseUrlFunctionTestHelper::CreateStringVector(urls);
    BaseVector* partVec = ParseUrlFunctionTestHelper::CreateStringVector(parts);
    BaseVector* result = nullptr;
    ParseUrlFunctionTestHelper::ExecuteParseUrl({urlVec, partVec}, result);
    ParseUrlFunctionTestHelper::ValidateStringResult(result, expected);
    delete urlVec;
    delete partVec;
    delete result;
}

TEST(ParseUrlTest, QueryKeyUsesFlinkLiteralPatternSemantics) {
    const std::string url = "http://host/path?a=b=1&a&b=2&empty=";
    std::vector<std::string> urls(4, url);
    std::vector<std::string> parts(4, "QUERY");
    std::vector<std::string> keys = {"a=b", "a&b", "empty", "missing"};
    std::vector<std::optional<std::string>> expected = {"1", "2", "", std::nullopt};

    BaseVector* urlVec = ParseUrlFunctionTestHelper::CreateStringVector(urls);
    BaseVector* partVec = ParseUrlFunctionTestHelper::CreateStringVector(parts);
    BaseVector* keyVec = ParseUrlFunctionTestHelper::CreateStringVector(keys);
    BaseVector* result = nullptr;
    ParseUrlFunctionTestHelper::ExecuteParseUrl({urlVec, partVec, keyVec}, result);
    ParseUrlFunctionTestHelper::ValidateStringResult(result, expected);
    delete urlVec;
    delete partVec;
    delete keyVec;
    delete result;
}

TEST(ParseUrlTest, PropagatesNullArguments) {
    std::vector<std::string> urls(3, "http://host/path?k=v");
    std::vector<std::string> parts(3, "QUERY");
    std::vector<std::string> keys(3, "k");
    std::vector<std::optional<std::string>> expected(3, std::nullopt);

    BaseVector* urlVec = ParseUrlFunctionTestHelper::CreateStringVector(urls);
    BaseVector* partVec = ParseUrlFunctionTestHelper::CreateStringVector(parts);
    BaseVector* keyVec = ParseUrlFunctionTestHelper::CreateStringVector(keys);
    urlVec->SetNull(0);
    partVec->SetNull(1);
    keyVec->SetNull(2);
    BaseVector* result = nullptr;
    ParseUrlFunctionTestHelper::ExecuteParseUrl({urlVec, partVec, keyVec}, result);
    ParseUrlFunctionTestHelper::ValidateStringResult(result, expected);
    delete urlVec;
    delete partVec;
    delete keyVec;
    delete result;
}

TEST(ParseUrlTest, RegistersCharAndVarcharSignatures) {
    const std::vector<DataTypeId> stringTypes = {OMNI_CHAR, OMNI_VARCHAR};
    for (DataTypeId first : stringTypes) {
        for (DataTypeId second : stringTypes) {
            auto twoArgumentSignature = std::make_shared<FunctionSignature>(
                "parse_url", std::vector<DataTypeId>{first, second}, OMNI_VARCHAR);
            EXPECT_NE(VectorFunction::Find(twoArgumentSignature), nullptr);
            for (DataTypeId third : stringTypes) {
                auto threeArgumentSignature = std::make_shared<FunctionSignature>(
                    "parse_url", std::vector<DataTypeId>{first, second, third}, OMNI_VARCHAR);
                EXPECT_NE(VectorFunction::Find(threeArgumentSignature), nullptr);
            }
        }
    }
}
