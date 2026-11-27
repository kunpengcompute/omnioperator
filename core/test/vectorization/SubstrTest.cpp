/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Substr function unit tests
 *   substr(string, start) -> varchar
 *   substr(string, start, length) -> varchar
 *   Spark + Gluten: INT for start/length; Spark semantics (start=0 -> first char).
 */

 #include <gtest/gtest.h>
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
 
 class SubstrTestEnvironment : public ::testing::Environment {
 public:
     void SetUp() override {
         RegisterFunctions::RegisterAllFunctions("");
     }
 };
 
 ::testing::Environment* const substr_test_env =
     ::testing::AddGlobalTestEnvironment(new SubstrTestEnvironment);
 
 class SubstrFunctionTestHelper {
 public:
     static void ValidateStringResult(BaseVector* result,
                                     const std::vector<std::string>& expected,
                                     int rowSize) {
         auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
         ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
         for (int i = 0; i < rowSize; ++i) {
             if (result->IsNull(i)) {
                 continue;
             }
             std::string_view actualSv = resultVec->GetValue(i);
             std::string actual(actualSv);
             std::string exp = expected[i];
             EXPECT_EQ(actual, exp) << "Row " << i << " expected=\"" << exp << "\" actual=\"" << actual << "\"";
         }
     }
 
     static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
         BaseVector* vec = VectorHelper::CreateStringVector(values.size());
         vec->SetIsField(true);
         auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
         EXPECT_NE(typed, nullptr);
         for (size_t i = 0; i < values.size(); ++i) {
             std::string_view sv(values[i]);
             typed->SetValue(i, sv);
         }
         return vec;
     }
 
     static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
         BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
         vec->SetIsField(true);
         auto* typed = static_cast<Vector<int32_t>*>(vec);
         for (size_t i = 0; i < values.size(); ++i) {
             typed->SetValue(i, values[i]);
         }
         return vec;
     }
 
     static void ExecuteSubstrTwoArg(BaseVector* stringVec, BaseVector* startVec,
                                     BaseVector*& result) {
         // Lookup by OMNI_VARCHAR (Spark/Gluten actual type); CreateStringVector returns OMNI_CHAR
         std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, startVec->GetTypeId() };
         auto sig = std::make_shared<FunctionSignature>("substr", inputTypeIds, OMNI_VARCHAR);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr(string, start) not found";
         auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         // SimpleFunction pops last arg first: push (string, start) so pop order is (start, string)
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }
 
     static void ExecuteSubstrThreeArg(BaseVector* stringVec, BaseVector* startVec,
                                      BaseVector* lengthVec, BaseVector*& result) {
         // Lookup by OMNI_VARCHAR (Spark/Gluten actual type); CreateStringVector returns OMNI_CHAR
         std::vector<DataTypeId> inputTypeIds = {
             OMNI_VARCHAR, startVec->GetTypeId(), lengthVec->GetTypeId()
         };
         auto sig = std::make_shared<FunctionSignature>("substr", inputTypeIds, OMNI_VARCHAR);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr(string, start, length) not found";
         auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         // SimpleFunction pops last arg first: push (string, start, length) so pop order is (length, start, string)
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         args.push(lengthVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }

     // Builds a StringView (16B fixed-width) input column. Values <= 12 bytes are stored inline;
     // longer values are stored out-of-line (prefix + heap pointer into `values`, which must outlive
     // execution). Mirrors FillStringViewVector in the microbench.
     static BaseVector* CreateStringViewVector(const std::vector<std::string>& values) {
         auto* vec = new Vector<StringView>(values.size());
         vec->SetIsField(true);
         for (size_t i = 0; i < values.size(); ++i) {
             vec->SetValue(i, StringView(values[i]));
         }
         return vec;
     }

     // 2-arg SV: substr(SV string, INT start). Asserts the {OMNI_STRING_VIEW, OMNI_INT}->OMNI_VARCHAR
     // overload is selected (SV-in / VARCHAR-out).
     static void ExecuteSubstrTwoArgSV(BaseVector* stringVec, BaseVector* startVec, BaseVector*& result) {
         ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "string arg must be a StringView column";
         std::vector<DataTypeId> inputTypeIds = { OMNI_STRING_VIEW, startVec->GetTypeId() };
         auto sig = std::make_shared<FunctionSignature>("substr", inputTypeIds, OMNI_VARCHAR);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr(StringView, start) overload not found — SV registration missing";
         auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }

     // 3-arg SV: substr(SV string, INT start, INT length).
     static void ExecuteSubstrThreeArgSV(BaseVector* stringVec, BaseVector* startVec,
                                         BaseVector* lengthVec, BaseVector*& result) {
         ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "string arg must be a StringView column";
         std::vector<DataTypeId> inputTypeIds = {
             OMNI_STRING_VIEW, startVec->GetTypeId(), lengthVec->GetTypeId()
         };
         auto sig = std::make_shared<FunctionSignature>("substr", inputTypeIds, OMNI_VARCHAR);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr(StringView, start, length) overload not found — SV registration missing";
         auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         args.push(lengthVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }

     // ---- SV-out (SV-in / SV-OUT, zero-copy sub-view) helpers ----
     // Validates an OMNI_STRING_VIEW result. Uses GetValueRef (by-value GetValue would be a compile
     // error: StringView::operator std::string_view() const&& is deleted). Callers validate AFTER
     // Apply, which frees the input vectors internally — so a passing read here also proves the
     // zero-copy output retained the input's string buffer (esp. for >12B non-inline results).
     static void ValidateStringViewResult(BaseVector* result, const std::vector<std::string>& expected,
                                          int rowSize) {
         ASSERT_NE(result, nullptr);
         ASSERT_EQ(result->GetTypeId(), OMNI_STRING_VIEW) << "SV-out result must be a StringView vector";
         auto* resultVec = dynamic_cast<Vector<StringView>*>(result);
         ASSERT_NE(resultVec, nullptr) << "result is not Vector<StringView>";
         for (int i = 0; i < rowSize; ++i) {
             if (result->IsNull(i)) {
                 continue;
             }
             const StringView& sv = resultVec->GetValueRef(i);
             std::string actual(sv.data(), sv.size());
             EXPECT_EQ(actual, expected[i]) << "Row " << i << " expected=\"" << expected[i]
                                            << "\" actual=\"" << actual << "\"";
         }
     }

     // NOTE: the SV-out VectorFunction takes ownership of and deletes the pushed arg vectors
     // (same convention as ConcatFunction), so callers must NOT delete stringVec/startVec/lengthVec.
     static void ExecuteSubstrTwoArgSVOut(BaseVector* stringVec, BaseVector* startVec, BaseVector*& result) {
         ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "string arg must be a StringView column";
         auto sig = std::make_shared<FunctionSignature>("substr",
             std::vector<DataTypeId>{OMNI_STRING_VIEW, startVec->GetTypeId()}, OMNI_STRING_VIEW);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr SV-out {SV,INT}->OMNI_STRING_VIEW not found";
         auto outputType = std::make_shared<DataType>(OMNI_STRING_VIEW);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }

     static void ExecuteSubstrThreeArgSVOut(BaseVector* stringVec, BaseVector* startVec,
                                            BaseVector* lengthVec, BaseVector*& result) {
         ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "string arg must be a StringView column";
         auto sig = std::make_shared<FunctionSignature>("substr",
             std::vector<DataTypeId>{OMNI_STRING_VIEW, startVec->GetTypeId(), lengthVec->GetTypeId()},
             OMNI_STRING_VIEW);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr) << "substr SV-out {SV,INT,INT}->OMNI_STRING_VIEW not found";
         auto outputType = std::make_shared<DataType>(OMNI_STRING_VIEW);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(stringVec->GetSize());
         std::stack<BaseVector*> args;
         args.push(stringVec);
         args.push(startVec);
         args.push(lengthVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }
 };
 
 TEST(SubstrTest, BasicSubstrTwoArg) {
     std::vector<std::string> strings = {"hello", "world", "apple"};
     std::vector<int32_t> starts = {1, 2, 3};
     std::vector<std::string> expected = {"hello", "orld", "ple"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrTwoArg(strVec, startVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete result;
 }
 
 TEST(SubstrTest, BasicSubstrThreeArg) {
     std::vector<std::string> strings = {"hello", "world", "apple"};
     std::vector<int32_t> starts = {1, 1, 2};
     std::vector<int32_t> lengths = {2, 5, 3};
     std::vector<std::string> expected = {"he", "world", "ppl"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }
 
 TEST(SubstrTest, NegativeStart) {
     std::vector<std::string> strings = {"my string here", "my string here", "my string here"};
     std::vector<int32_t> starts = {-3, -1, -10};
     std::vector<int32_t> lengths = {3, 3, 100};
     std::vector<std::string> expected = {"ere", "e", "tring here"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }
 
 TEST(SubstrTest, NegativeStartTwoArg) {
     std::vector<std::string> strings = {"my string here"};
     std::vector<int32_t> starts = {-3};
     std::vector<std::string> expected = {"ere"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrTwoArg(strVec, startVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 1);
     delete strVec;
     delete startVec;
     delete result;
 }
 
 TEST(SubstrTest, SparkStartZero) {
     std::vector<std::string> strings = {"example", "apple"};
     std::vector<int32_t> starts = {0, 0};
     std::vector<int32_t> lengths = {2, 3};
     std::vector<std::string> expected = {"ex", "app"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 2);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }
 
 TEST(SubstrTest, EmptyResult) {
     std::vector<std::string> strings = {"apple", "apple"};
     std::vector<int32_t> starts = {10, 1};
     std::vector<int32_t> lengths = {2, 0};
     std::vector<std::string> expected = {"", ""};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 2);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }
 
 TEST(SubstrTest, LengthClampedToEnd) {
     std::vector<std::string> strings = {"hello"};
     std::vector<int32_t> starts = {2};
     std::vector<int32_t> lengths = {100};
     std::vector<std::string> expected = {"ello"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 1);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }
 
// ---------------- StringView (SV-in / VARCHAR-out) variants ----------------

#ifdef STRINGVIEW_ENABLE
TEST(SubstrTest, SVTwoArgInlineAndNonInline) {
     // "hi" (inline), "my string here" (14B non-inline), "apple" (inline)
     std::vector<std::string> strings = {"hi", "my string here", "apple"};
     std::vector<int32_t> starts = {1, 4, 3};
     std::vector<std::string> expected = {"hi", "string here", "ple"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrTwoArgSV(strVec, startVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete result;
 }

 TEST(SubstrTest, SVThreeArgAndNegativeStart) {
     std::vector<std::string> strings = {"hello world today", "hello world today", "apple"};
     std::vector<int32_t> starts = {1, -5, 0};
     std::vector<int32_t> lengths = {5, 100, 3};
     std::vector<std::string> expected = {"hello", "today", "app"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArgSV(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }

 TEST(SubstrTest, SVNullPropagation) {
     std::vector<std::string> strings = {"alpha_bravo_charlie", "delta", "echo_foxtrot_golf"};
     std::vector<int32_t> starts = {1, 1, 1};
     std::vector<int32_t> lengths = {5, 5, 4};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     strVec->SetNull(1);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArgSV(strVec, startVec, lengthVec, result);
     auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
     ASSERT_NE(resultVec, nullptr);
     EXPECT_EQ(resultVec->GetValue(0), "alpha");
     EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
     EXPECT_EQ(resultVec->GetValue(2), "echo");
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }

 TEST(SubstrTest, Boundary) {
     // 空串、start 超出长度、单字符
     std::vector<std::string> strings = {"", "ab", "x"};
     std::vector<int32_t> starts = {1, 10, 1};
     std::vector<int32_t> lengths = {5, 5, 1};
     std::vector<std::string> expected = {"", "", "x"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArg(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringResult(result, expected, 3);
     delete strVec;
     delete startVec;
     delete lengthVec;
     delete result;
 }

 // ---------------- StringView SV-out (SV-in / SV-OUT, zero-copy sub-view) ----------------
 // Apply() frees the pushed input vectors internally; tests must NOT delete strVec/startVec/lengthVec.

 TEST(SubstrTest, SVOutTwoArgInlineAndNonInline) {
     // "hi" inline result; "string here is quite long" (25B) non-inline -> exercises arena aliasing
     // after the input vector is freed inside Apply; "ple" inline.
     std::vector<std::string> strings = {"hi", "my string here is quite long", "apple"};
     std::vector<int32_t> starts = {1, 4, 3};
     std::vector<std::string> expected = {"hi", "string here is quite long", "ple"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrTwoArgSVOut(strVec, startVec, result);
     SubstrFunctionTestHelper::ValidateStringViewResult(result, expected, 3);
     delete result;
 }

 TEST(SubstrTest, SVOutThreeArgAndNegativeStart) {
     std::vector<std::string> strings = {"hello world today", "hello world today", "apple"};
     std::vector<int32_t> starts = {1, -5, 0};
     std::vector<int32_t> lengths = {5, 100, 3};
     std::vector<std::string> expected = {"hello", "today", "app"};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArgSVOut(strVec, startVec, lengthVec, result);
     SubstrFunctionTestHelper::ValidateStringViewResult(result, expected, 3);
     delete result;
 }

 TEST(SubstrTest, SVOutNullPropagation) {
     std::vector<std::string> strings = {"alpha_bravo_charlie", "delta", "echo_foxtrot_golf"};
     std::vector<int32_t> starts = {1, 1, 1};
     std::vector<int32_t> lengths = {5, 5, 4};
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     strVec->SetNull(1);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* lengthVec = SubstrFunctionTestHelper::CreateInt32Vector(lengths);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrThreeArgSVOut(strVec, startVec, lengthVec, result);
     auto* resultVec = dynamic_cast<Vector<StringView>*>(result);
     ASSERT_NE(resultVec, nullptr);
     ASSERT_EQ(result->GetTypeId(), OMNI_STRING_VIEW);
     EXPECT_EQ(std::string(resultVec->GetValueRef(0).data(), resultVec->GetValueRef(0).size()), "alpha");
     EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL";
     EXPECT_EQ(std::string(resultVec->GetValueRef(2).data(), resultVec->GetValueRef(2).size()), "echo");
     delete result;
 }

 // Explicit zero-copy lifetime guard: a >12B result must stay valid after Apply frees the input SV
 // vector. Under ASAN this is a direct use-after-free check on the shared-string-buffer retention.
 TEST(SubstrTest, SVOutResultOutlivesInput) {
     std::vector<std::string> strings = {"abcdefghijklmnopqrstuvwxyz"}; // 26B input
     std::vector<int32_t> starts = {3};
     std::vector<std::string> expected = {"cdefghijklmnopqrstuvwxyz"};  // 24B non-inline result
     BaseVector* strVec = SubstrFunctionTestHelper::CreateStringViewVector(strings);
     BaseVector* startVec = SubstrFunctionTestHelper::CreateInt32Vector(starts);
     BaseVector* result = nullptr;
     SubstrFunctionTestHelper::ExecuteSubstrTwoArgSVOut(strVec, startVec, result); // frees strVec/startVec
    SubstrFunctionTestHelper::ValidateStringViewResult(result, expected, 1);
    delete result;
}
#endif
