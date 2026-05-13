/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: Batch String Function Registry
 */
#include "batch_func_registry_string.h"
#include "batch_functions/batch_stringfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

namespace {
const std::string COUNT_CHAR_FN_STR = "batch_CountChar";
const std::string SPLIT_INDEX_FN_STR = "batch_SplitIndex";
const std::string LESS_THAN_FN_STR = "batch_lessThan";
const std::string LESS_THAN_EQUAL_FN_STR = "batch_lessThanEqual";
const std::string GREATER_THAN_FN_STR = "batch_greaterThan";
const std::string GREATER_THAN_EQUAL_FN_STR = "batch_greaterThanEqual";
const std::string EQUAL_FN_STR = "batch_equal";
const std::string NOT_EQUAL_FN_STR = "batch_notEqual";
const std::string SUBSTR_FN_STR = "batch_substr";
const std::string CONCAT_FN_STR = "batch_concat";
const std::string LIKE_FN_STR = "batch_LIKE";
const std::string CAST_FN_STR = "batch_CAST";
const std::string UPPER_FN_STR = "batch_upper";
const std::string LOWER_FN_STR = "batch_lower";
const std::string COMPARE_FN_STR = "batch_compare";
const std::string LENGTH_FN_STR = "batch_length";
const std::string REPLACE_FN_STR = "batch_replace";
const std::string CONCAT_FN_STR_RETNULL = "batch_concat_null";
const std::string CAST_FN_STR_RETNULL = "batch_CAST_null";
const std::string INSTR_FN_STR = "batch_instr";
const std::string STARTS_WITH_FN_STR = "batch_StartsWith";
const std::string ENDS_WITH_FN_STR = "batch_EndsWith";
const std::string MD5_STR = "batch_Md5";
const std::string EMPTY2NULL_STR = "batch_empty2null";
const std::string CONTAINS_FN_STR = "batch_Contains";
const std::string GREATEST_STR_FN_STR = "batch_Greatest";
const std::string JSON_VALUE_FN_STR = "batch_json_value";
const std::string JSON_QUERY_FN_STR = "batch_json_query";
const std::string JSON_SPLIT_FN_STR = "batch_json_split";
const std::string BATCH_STATIC_INVOKE_VARCHARTYPE_CHECK_FN_STR = "batch_StaticInvokeVarcharTypeWriteSideCheck";
const std::string BATCH_STATIC_INVOKE_CHAR_READ_PADDING_FN_STR = "batch_StaticInvokeCharReadPadding";
}

std::vector<Function> BatchStringFunctionRegistry::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        Function(reinterpret_cast<void *>(BatchSplitIndex), SPLIT_INDEX_FN_STR, {},
                 { OMNI_VARCHAR, OMNI_CHAR, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanStr), LESS_THAN_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualStr), LESS_THAN_EQUAL_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanStr), GREATER_THAN_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualStr), GREATER_THAN_EQUAL_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualStr), EQUAL_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualStr), NOT_EQUAL_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchStrCompare), COMPARE_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_INT),

        // concat functions
        Function(reinterpret_cast<void *>(BatchConcatStrStr), CONCAT_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchConcatCharChar), CONCAT_FN_STR, {}, { OMNI_CHAR, OMNI_CHAR }, OMNI_CHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchConcatCharStr), CONCAT_FN_STR, {}, { OMNI_CHAR, OMNI_VARCHAR },
            OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchConcatStrChar), CONCAT_FN_STR, {}, { OMNI_VARCHAR, OMNI_CHAR },
            OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchConcatStrStrRetNull), CONCAT_FN_STR_RETNULL, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_CHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchConcatCharCharRetNull), CONCAT_FN_STR_RETNULL, {},
            { OMNI_CHAR, OMNI_CHAR }, OMNI_CHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchConcatCharStrRetNull), CONCAT_FN_STR_RETNULL, {},
            { OMNI_CHAR, OMNI_VARCHAR }, OMNI_CHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchConcatStrCharRetNull), CONCAT_FN_STR_RETNULL, {},
            { OMNI_VARCHAR, OMNI_CHAR }, OMNI_CHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),

        Function(reinterpret_cast<void *>(BatchLikeStr), LIKE_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLikeChar), LIKE_FN_STR, {}, { OMNI_CHAR, OMNI_VARCHAR }, OMNI_BOOLEAN,
            INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchToUpperStr), UPPER_FN_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchToUpperChar), UPPER_FN_STR, {}, { OMNI_CHAR }, OMNI_CHAR, INPUT_DATA,
            true),
        Function(reinterpret_cast<void *>(BatchToLowerStr), LOWER_FN_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchToLowerChar), LOWER_FN_STR, {}, { OMNI_CHAR }, OMNI_CHAR, INPUT_DATA,
            true),

        // length functions
        Function(reinterpret_cast<void *>(BatchCountChar), COUNT_CHAR_FN_STR, {},{ OMNI_VARCHAR, OMNI_CHAR },
                 OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLengthChar), LENGTH_FN_STR, {}, { OMNI_CHAR }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLengthStr), LENGTH_FN_STR, {}, { OMNI_VARCHAR }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLengthCharReturnInt32), LENGTH_FN_STR, {}, { OMNI_CHAR }, OMNI_INT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLengthStrReturnInt32), LENGTH_FN_STR, {}, { OMNI_VARCHAR }, OMNI_INT,
            INPUT_DATA),

        // cast to string
        Function(reinterpret_cast<void *>(BatchCastIntToString), CAST_FN_STR, {}, { OMNI_INT }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastLongToString), CAST_FN_STR, {}, { OMNI_LONG }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDoubleToString), CAST_FN_STR, {}, { OMNI_DOUBLE }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToString), CAST_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToString), CAST_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastIntToStringRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_INT },
            OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchCastLongToStringRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_LONG },
            OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchCastDoubleToStringRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_DOUBLE },
            OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToStringRetNull), CAST_FN_STR_RETNULL, {},
            { OMNI_DECIMAL64 }, OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToStringRetNull), CAST_FN_STR_RETNULL, {},
            { OMNI_DECIMAL128 }, OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),

        // cast string to
        Function(reinterpret_cast<void *>(BatchCastStrWithDiffWidths), CAST_FN_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToIntRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_VARCHAR },
            OMNI_INT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastStringToLongRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_VARCHAR },
            OMNI_LONG, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastStringToDoubleRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_VARCHAR },
            OMNI_DOUBLE, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastStringToInt), CAST_FN_STR, {}, { OMNI_VARCHAR }, OMNI_INT,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToLong), CAST_FN_STR, {}, { OMNI_VARCHAR }, OMNI_LONG,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDouble), CAST_FN_STR, {}, { OMNI_VARCHAR }, OMNI_DOUBLE,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStrWithDiffWidthsRetNull), CAST_FN_STR_RETNULL, {}, { OMNI_VARCHAR },
            OMNI_VARCHAR, INPUT_DATA_AND_OVERFLOW_NULL, true),

        Function(reinterpret_cast<void *>(BatchInStr), INSTR_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_INT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchStartsWithStr), STARTS_WITH_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEndsWithStr), ENDS_WITH_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchMd5Str), MD5_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchContainsStr), CONTAINS_FN_STR, {}, { OMNI_VARCHAR, OMNI_VARCHAR },
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreatestStr), GREATEST_STR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchJsonValue), JSON_VALUE_FN_STR, {}, { OMNI_VARCHAR, OMNI_CHAR },
            OMNI_VARCHAR, INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchJsonValueWithBehaviors), JSON_VALUE_FN_STR, {},
            { OMNI_VARCHAR, OMNI_CHAR, OMNI_INT, OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR },
            OMNI_VARCHAR, INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchJsonQuery), JSON_QUERY_FN_STR, {}, { OMNI_VARCHAR, OMNI_CHAR },
            OMNI_VARCHAR, INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchJsonSplitChar), JSON_SPLIT_FN_STR, {}, { OMNI_CHAR }, OMNI_VARCHAR,
            INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchJsonSplitStr), JSON_SPLIT_FN_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchEmptyToNull), EMPTY2NULL_STR, {}, { OMNI_VARCHAR }, OMNI_VARCHAR,
            INPUT_DATA, false),
        Function(reinterpret_cast<void *>(BatchStaticInvokeVarcharTypeWriteSideCheck),
            BATCH_STATIC_INVOKE_VARCHARTYPE_CHECK_FN_STR, {}, { OMNI_VARCHAR, OMNI_INT },
            OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchStaticInvokeCharReadPadding),
            BATCH_STATIC_INVOKE_CHAR_READ_PADDING_FN_STR, {}, { OMNI_VARCHAR, OMNI_INT },
            OMNI_VARCHAR, INPUT_DATA, true)};

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistryNotAllowReducePrecison::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        Function(reinterpret_cast<void *>(BatchCastStringToDateNotAllowReducePrecison), CAST_FN_STR, {},
            { OMNI_VARCHAR }, OMNI_DATE32, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDateRetNullNotAllowReducePrecison), CAST_FN_STR_RETNULL, {},
            { OMNI_VARCHAR }, OMNI_DATE32, INPUT_DATA_AND_OVERFLOW_NULL),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistryAllowReducePrecison::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        Function(reinterpret_cast<void *>(BatchCastStringToDateAllowReducePrecison), CAST_FN_STR, {}, { OMNI_VARCHAR },
            OMNI_DATE32, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDateRetNullAllowReducePrecison), CAST_FN_STR_RETNULL, {},
            { OMNI_VARCHAR }, OMNI_DATE32, INPUT_DATA_AND_OVERFLOW_NULL),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistryNotReplace::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // replace functions
        Function(reinterpret_cast<void *>(BatchReplaceStrStrStrWithRepNotReplace), REPLACE_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchReplaceStrStrWithoutRepNotReplace), REPLACE_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistryReplace::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // replace functions
        Function(reinterpret_cast<void *>(BatchReplaceStrStrStrWithRepReplace), REPLACE_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchReplaceStrStrWithoutRepReplace), REPLACE_FN_STR, {},
            { OMNI_VARCHAR, OMNI_VARCHAR }, OMNI_VARCHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistrySupportNegativeAndZeroIndex::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // substr functions
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int32_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int32_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int64_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int64_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),

        // substr with start index functions
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int32_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int32_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int64_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int64_t, true, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistrySupportNotNegativeAndZeroIndex::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // substr functions
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int32_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int32_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int64_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int64_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),

        // substr with start index functions
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int32_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int32_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int64_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int64_t, false, true>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistrySupportNegativeAndNotZeroIndex::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // substr functions
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int32_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int32_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int64_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int64_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),

        // substr with start index functions
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int32_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int32_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int64_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int64_t, true, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringFunctionRegistrySupportNotNegativeAndNotZeroIndex::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        // substr functions
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int32_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int32_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarchar<int64_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrChar<int64_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),

        // substr with start index functions
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int32_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_INT }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int32_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_INT }, OMNI_CHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrVarcharWithStart<int64_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_VARCHAR, OMNI_LONG }, OMNI_VARCHAR, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubstrCharWithStart<int64_t, false, false>), SUBSTR_FN_STR, {},
            { OMNI_CHAR, OMNI_LONG }, OMNI_CHAR, INPUT_DATA, true),
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringToDecimalFunctionRegistryAllowRoundUp::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal64RoundUp), CAST_FN_STR, {}, {OMNI_VARCHAR},
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal128RoundUp), CAST_FN_STR, {}, {OMNI_VARCHAR},
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal64RoundUpRetNull), CAST_FN_STR_RETNULL, {},
            {OMNI_VARCHAR},
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal128RoundUpRetNull), CAST_FN_STR_RETNULL, {},
            {OMNI_VARCHAR}, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL)
    };

    return batchStringFnRegistry;
}

std::vector<Function> BatchStringToDecimalFunctionRegistry::GetFunctions()
{
    std::vector<Function> batchStringFnRegistry = {
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal64), CAST_FN_STR, {}, {OMNI_VARCHAR},
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal128), CAST_FN_STR, {}, {OMNI_VARCHAR},
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal64RetNull), CAST_FN_STR_RETNULL, {}, {OMNI_VARCHAR},
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastStringToDecimal128RetNull), CAST_FN_STR_RETNULL, {},
            {OMNI_VARCHAR}, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL)
    };

    return batchStringFnRegistry;
}
}
