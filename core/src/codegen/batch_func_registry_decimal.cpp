/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: Batch Decimal Function Registry
 */
#include "batch_func_registry_decimal.h"
#include "batch_functions/batch_decimal_arithmetic_functions.h"
#include "batch_functions/batch_decimal_cast_functions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace function;

namespace {
const std::string ABS_FN_STR = "batch_abs";
const std::string ROUND_FN_STR = "batch_round";
const std::string ROUND_NULL_FN_STR = "batch_round_null";
const std::string CAST_FN_STR = "batch_CAST";
const std::string CAST_NULL_FN_STR = "batch_CAST_null";
const std::string ADD_NULL_FN_STR = "batch_add_null";
const std::string SUBTRACT_NULL_FN_STR = "batch_subtract_null";
const std::string MULTIPLY_NULL_FN_STR = "batch_multiply_null";
const std::string DIVIDE_NULL_FN_STR = "batch_divide_null";
const std::string MODULUS_NULL_FN_STR = "batch_modulus_null";
const std::string ADD_FN_STR = "batch_add";
const std::string SUBTRACT_FN_STR = "batch_subtract";
const std::string MULTIPLY_FN_STR = "batch_multiply";
const std::string DIVIDE_FN_STR = "batch_divide";
const std::string MODULUS_FN_STR = "batch_modulus";
const std::string LESS_THAN_FN_STR = "batch_lessThan";
const std::string LESS_THAN_EQUAL_FN_STR = "batch_lessThanEqual";
const std::string GREATER_THAN_FN_STR = "batch_greaterThan";
const std::string GREATER_THAN_EQUAL_FN_STR = "batch_greaterThanEqual";
const std::string EQUAL_FN_STR = "batch_equal";
const std::string NOT_EQUAL_FN_STR = "batch_notEqual";
const std::string MAKE_DECIMAL_FN_STR = "batch_MakeDecimal";
const std::string MAKE_DECIMAL_NULL_FN_STR = "batch_MakeDecimal_null";
const std::string BATCH_DECIMAL128_COMPARE_STR = "batch_Decimal128Compare";
const std::string BATCH_DECIMAL64_COMPARE_STR = "batch_Decimal64Compare";
const std::string BATCH_UNSCALED_VALUE_STR = "batch_UnscaledValue";
const std::string GREATEST_DECIMAL_FN_STR = "batch_Greatest";
const std::string GREATEST_DECIMAL_NULL_FN_STR = "batch_Greatest_null";
}

std::vector<Function> BatchDecimalFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = { OMNI_DECIMAL128, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes64 = { OMNI_DECIMAL64, OMNI_DECIMAL64 };
    std::vector<DataTypeId> paramTypes64Op128 = { OMNI_DECIMAL64, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes128Op64 = { OMNI_DECIMAL128, OMNI_DECIMAL64 };

    static std::vector<Function> batchDecimalFunctions = {
        Function(reinterpret_cast<void *>(BatchDecimal128Compare), BATCH_DECIMAL128_COMPARE_STR, {}, paramTypes128,
            OMNI_INT),
        Function(reinterpret_cast<void *>(BatchAbsDecimal128), ABS_FN_STR, {}, { OMNI_DECIMAL128 }, OMNI_DECIMAL128,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchDecimal64Compare), BATCH_DECIMAL64_COMPARE_STR, {}, paramTypes64,
            OMNI_INT),
        Function(reinterpret_cast<void *>(BatchAbsDecimal64), ABS_FN_STR, {}, { OMNI_DECIMAL64 }, OMNI_DECIMAL64,
            INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchRoundDecimal128), ROUND_FN_STR, {}, { OMNI_DECIMAL128, OMNI_INT },
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchRoundDecimal64), ROUND_FN_STR, {}, { OMNI_DECIMAL64, OMNI_INT },
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchRoundDecimal128WithoutRound), ROUND_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchRoundDecimal64WithoutRound), ROUND_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DECIMAL64, INPUT_DATA, true),

        // decimal64 compare
        Function(reinterpret_cast<void *>(BatchLessThanDecimal64), LESS_THAN_FN_STR, {}, paramTypes64, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualDecimal64), LESS_THAN_EQUAL_FN_STR, {}, paramTypes64,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanDecimal64), GREATER_THAN_FN_STR, {}, paramTypes64,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualDecimal64), GREATER_THAN_EQUAL_FN_STR, {}, paramTypes64,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualDecimal64), EQUAL_FN_STR, {}, paramTypes64, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualDecimal64), NOT_EQUAL_FN_STR, {}, paramTypes64, OMNI_BOOLEAN,
            INPUT_DATA),

        // decimal128 compare
        Function(reinterpret_cast<void *>(BatchLessThanDecimal128), LESS_THAN_FN_STR, {}, paramTypes128, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualDecimal128), LESS_THAN_EQUAL_FN_STR, {}, paramTypes128,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanDecimal128), GREATER_THAN_FN_STR, {}, paramTypes128,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualDecimal128), GREATER_THAN_EQUAL_FN_STR, {},
            paramTypes128, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualDecimal128), EQUAL_FN_STR, {}, paramTypes128, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualDecimal128), NOT_EQUAL_FN_STR, {}, paramTypes128, OMNI_BOOLEAN,
            INPUT_DATA),

        // Decimal Cast Function
        Function(reinterpret_cast<void *>(BatchCastDecimal64To64), CAST_FN_STR, {}, { OMNI_DECIMAL64 }, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal128To128), CAST_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal64To128), CAST_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal128To64), CAST_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DECIMAL64, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchCastIntToDecimal64), CAST_FN_STR, {}, { OMNI_INT }, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastLongToDecimal64), CAST_FN_STR, {}, { OMNI_LONG }, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDoubleToDecimal64), CAST_FN_STR, {}, { OMNI_DOUBLE }, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastIntToDecimal128), CAST_FN_STR, {}, { OMNI_INT }, OMNI_DECIMAL128,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastLongToDecimal128), CAST_FN_STR, {}, { OMNI_LONG }, OMNI_DECIMAL128,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDoubleToDecimal128), CAST_FN_STR, {}, { OMNI_DOUBLE },
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchCastDecimal128ToLong), CAST_FN_STR, {}, { OMNI_DECIMAL128 }, OMNI_LONG,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToInt), CAST_FN_STR, {}, { OMNI_DECIMAL128 }, OMNI_INT,
            INPUT_DATA, true),

        // Decimal Cast Function Return Null
        Function(reinterpret_cast<void *>(BatchRoundDecimal128RetNull), ROUND_NULL_FN_STR, {},
            { OMNI_DECIMAL128, OMNI_INT }, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchRoundDecimal64RetNull), ROUND_NULL_FN_STR, {},
            { OMNI_DECIMAL64, OMNI_INT }, OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchCastDecimal64To64RetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal128To128RetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal64To128RetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal128To64RetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchCastDecimal64ToDoubleRetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DOUBLE, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToLongRetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_LONG, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToDoubleRetNull), CAST_NULL_FN_STR, {},
            { OMNI_DECIMAL128 }, OMNI_DOUBLE, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToIntRetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_INT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToIntRetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_INT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToLongRetNull), CAST_NULL_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_LONG, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchCastIntToDecimal64RetNull), CAST_NULL_FN_STR, {}, { OMNI_INT },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastLongToDecimal64RetNull), CAST_NULL_FN_STR, {}, { OMNI_LONG },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDoubleToDecimal64RetNull), CAST_NULL_FN_STR, {}, { OMNI_DOUBLE },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastIntToDecimal128RetNull), CAST_NULL_FN_STR, {}, { OMNI_INT },
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastLongToDecimal128RetNull), CAST_NULL_FN_STR, {}, { OMNI_LONG },
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchCastDoubleToDecimal128RetNull), CAST_NULL_FN_STR, {}, { OMNI_DOUBLE },
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        // UnscaledValue
        Function(reinterpret_cast<void *>(BatchUnscaledValue64), BATCH_UNSCALED_VALUE_STR, {}, { OMNI_DECIMAL64 },
            OMNI_LONG, INPUT_DATA),
        // MakeDecimal
        Function(reinterpret_cast<void *>(BatchMakeDecimal64), MAKE_DECIMAL_FN_STR, {}, { OMNI_LONG }, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMakeDecimal64RetNull), MAKE_DECIMAL_NULL_FN_STR, {}, { OMNI_LONG },
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),

        // Return Null
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec64RetNull), ADD_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec128RetNull), ADD_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec128Dec128RetNull), ADD_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec128Dec128RetNull), ADD_NULL_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec64Dec128RetNull), ADD_NULL_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec64RetNull), SUBTRACT_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec128RetNull), SUBTRACT_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec128Dec128RetNull), SUBTRACT_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec128Dec128RetNull), SUBTRACT_NULL_FN_STR, {},
            paramTypes64Op128, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec64Dec128RetNull), SUBTRACT_NULL_FN_STR, {},
            paramTypes128Op64, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec64RetNull), MULTIPLY_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec128RetNull), MULTIPLY_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec128Dec128RetNull), MULTIPLY_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec128Dec128RetNull), MULTIPLY_NULL_FN_STR, {},
            paramTypes64Op128, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec64Dec128RetNull), MULTIPLY_NULL_FN_STR, {},
            paramTypes128Op64, OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec64RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec64RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec64RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec128RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec128Dec128RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec128RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec128RetNull), DIVIDE_NULL_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(BatchModDec64Dec64Dec64RetNull), MODULUS_NULL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec64RetNull), MODULUS_NULL_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec64RetNull), MODULUS_NULL_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec128RetNull), MODULUS_NULL_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec128RetNull), MODULUS_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec64RetNull), MODULUS_NULL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec128RetNull), MODULUS_NULL_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        // BatchDecimalGreatest
        Function(reinterpret_cast<void *>(BatchGreatestDecimal64), GREATEST_DECIMAL_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchGreatestDecimal128), GREATEST_DECIMAL_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(BatchGreatestDecimal64RetNull), GREATEST_DECIMAL_NULL_FN_STR, {},
            paramTypes64, OMNI_DECIMAL64, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchGreatestDecimal128RetNull), GREATEST_DECIMAL_NULL_FN_STR, {},
            paramTypes128, OMNI_DECIMAL128, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
    };

    return batchDecimalFunctions;
}

std::vector<Function> BatchDecimalFunctionRegistryDown::GetFunctions()
{
    static std::vector<Function> batchDecimalFunctions = {
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToLongDown), CAST_FN_STR, {}, { OMNI_DECIMAL64 }, OMNI_LONG,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToIntDown), CAST_FN_STR, {}, { OMNI_DECIMAL64 }, OMNI_INT,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToDoubleDown), CAST_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToDoubleDown), CAST_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DOUBLE, INPUT_DATA),
    };

    return batchDecimalFunctions;
}

std::vector<Function> BatchDecimalFunctionRegistryHalfUp::GetFunctions()
{
    static std::vector<Function> batchDecimalFunctions = {
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToLongHalfUp), CAST_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToIntHalfUp), CAST_FN_STR, {}, { OMNI_DECIMAL64 }, OMNI_INT,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchCastDecimal64ToDoubleHalfUp), CAST_FN_STR, {}, { OMNI_DECIMAL64 },
            OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDecimal128ToDoubleHalfUp), CAST_FN_STR, {}, { OMNI_DECIMAL128 },
            OMNI_DOUBLE, INPUT_DATA),
    };

    return batchDecimalFunctions;
}

std::vector<Function> BatchDecimalFunctionRegistryReScale::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = { OMNI_DECIMAL128, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes64 = { OMNI_DECIMAL64, OMNI_DECIMAL64 };
    std::vector<DataTypeId> paramTypes64Op128 = { OMNI_DECIMAL64, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes128Op64 = { OMNI_DECIMAL128, OMNI_DECIMAL64 };

    static std::vector<Function> batchDecimalFunctions = {
        // decimal arith function
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec64ReScale), ADD_FN_STR, {}, paramTypes64, OMNI_DECIMAL64,
            INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec128ReScale), ADD_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec128Dec128ReScale), ADD_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec128Dec128ReScale), ADD_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec64Dec128ReScale), ADD_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec64ReScale), SUBTRACT_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec128ReScale), SUBTRACT_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec128Dec128ReScale), SUBTRACT_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec128Dec128ReScale), SUBTRACT_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec64Dec128ReScale), SUBTRACT_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec64ReScale), MULTIPLY_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec128ReScale), MULTIPLY_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec128Dec128ReScale), MULTIPLY_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec128Dec128ReScale), MULTIPLY_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec64Dec128ReScale), MULTIPLY_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec64ReScale), DIVIDE_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec64ReScale), DIVIDE_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec64ReScale), DIVIDE_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec128ReScale), DIVIDE_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec128Dec128ReScale), DIVIDE_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec128ReScale), DIVIDE_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec128ReScale), DIVIDE_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchModDec64Dec64Dec64ReScale), MODULUS_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec64ReScale), MODULUS_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec64ReScale), MODULUS_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec128ReScale), MODULUS_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec128ReScale), MODULUS_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec64ReScale), MODULUS_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec128ReScale), MODULUS_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
    };

    return batchDecimalFunctions;
}

std::vector<Function> BatchDecimalFunctionRegistryNotReScale::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = { OMNI_DECIMAL128, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes64 = { OMNI_DECIMAL64, OMNI_DECIMAL64 };
    std::vector<DataTypeId> paramTypes64Op128 = { OMNI_DECIMAL64, OMNI_DECIMAL128 };
    std::vector<DataTypeId> paramTypes128Op64 = { OMNI_DECIMAL128, OMNI_DECIMAL64 };

    static std::vector<Function> batchDecimalFunctions = {
        // decimal arith function
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec64NotReScale), ADD_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec64Dec128NotReScale), ADD_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec128Dec128NotReScale), ADD_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec64Dec128Dec128NotReScale), ADD_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchAddDec128Dec64Dec128NotReScale), ADD_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec64NotReScale), SUBTRACT_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec64Dec128NotReScale), SUBTRACT_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec128Dec128NotReScale), SUBTRACT_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec64Dec128Dec128NotReScale), SUBTRACT_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchSubDec128Dec64Dec128NotReScale), SUBTRACT_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec64NotReScale), MULTIPLY_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec64Dec128NotReScale), MULTIPLY_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec128Dec128NotReScale), MULTIPLY_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec64Dec128Dec128NotReScale), MULTIPLY_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchMulDec128Dec64Dec128NotReScale), MULTIPLY_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec64NotReScale), DIVIDE_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec64NotReScale), DIVIDE_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec64NotReScale), DIVIDE_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec64Dec128NotReScale), DIVIDE_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec128Dec128NotReScale), DIVIDE_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec64Dec128Dec128NotReScale), DIVIDE_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchDivDec128Dec64Dec128NotReScale), DIVIDE_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(BatchModDec64Dec64Dec64NotReScale), MODULUS_FN_STR, {}, paramTypes64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec64NotReScale), MODULUS_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec64NotReScale), MODULUS_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec64Dec128NotReScale), MODULUS_FN_STR, {}, paramTypes128Op64,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec128NotReScale), MODULUS_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec128Dec128Dec64NotReScale), MODULUS_FN_STR, {}, paramTypes128,
            OMNI_DECIMAL64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModDec64Dec128Dec128NotReScale), MODULUS_FN_STR, {}, paramTypes64Op128,
            OMNI_DECIMAL128, INPUT_DATA, true),
    };

    return batchDecimalFunctions;
}
}
