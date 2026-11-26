/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: batch math functions registry
 */

#include "batch_func_registry_math.h"
#include "batch_functions/batch_mathfunctions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

namespace {
const std::string ABS_FN_STR = "batch_abs";
const std::string CAST_FN_STR = "batch_CAST";
const std::string ROUND_FN_STR = "batch_round";
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
const std::string PMOD_FN_STR = "batch_pmod";
const std::string NORMALIZE_ZERO_FN_STR = "batch_NormalizeNaNAndZero";
const std::string GREATEST_NUM_FN_STR = "batch_Greatest";
const std::string POWER_FN_STR = "batch_power";
} // namespace

std::vector<Function> BatchMathFunctionRegistry::GetFunctions()
{
    const std::vector<omniruntime::type::DataTypeId> doubleParams = {OMNI_DOUBLE, OMNI_DOUBLE};
    const std::vector<omniruntime::type::DataTypeId> longParams = {OMNI_LONG, OMNI_LONG};
    const std::vector<omniruntime::type::DataTypeId> intParams = {OMNI_INT, OMNI_INT};
    const std::vector<omniruntime::type::DataTypeId> boolParams = {OMNI_BOOLEAN, OMNI_BOOLEAN};

    std::vector<Function> batchMathFunctions = {
        Function(reinterpret_cast<void *>(BatchAbs<int32_t>), ABS_FN_STR, {}, {OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchAbs<int64_t>), ABS_FN_STR, {}, {OMNI_LONG}, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchAbs<double>), ABS_FN_STR, {}, {OMNI_DOUBLE}, OMNI_DOUBLE, INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchCastInt32ToDouble), CAST_FN_STR, {}, {OMNI_INT}, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastInt64ToDouble), CAST_FN_STR, {}, {OMNI_LONG}, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastInt32ToInt64), CAST_FN_STR, {}, {OMNI_INT}, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastInt64ToInt32), CAST_FN_STR, {}, {OMNI_LONG}, OMNI_INT, INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchAddDouble), ADD_FN_STR, {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchSubtractDouble), SUBTRACT_FN_STR, {}, doubleParams, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchMultiplyDouble), MULTIPLY_FN_STR, {}, doubleParams, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchDivideDouble), DIVIDE_FN_STR, {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchModulusDouble), MODULUS_FN_STR, {}, doubleParams, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanDouble), LESS_THAN_FN_STR, {}, doubleParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualDouble), LESS_THAN_EQUAL_FN_STR, {}, doubleParams,
                 OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanDouble), GREATER_THAN_FN_STR, {}, doubleParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualDouble), GREATER_THAN_EQUAL_FN_STR, {}, doubleParams,
                 OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualDouble), EQUAL_FN_STR, {}, doubleParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualDouble), NOT_EQUAL_FN_STR, {}, doubleParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNormalizeNaNAndZero), NORMALIZE_ZERO_FN_STR, {}, {OMNI_DOUBLE},
                 OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchPowerDouble), POWER_FN_STR, {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchAddInt64), ADD_FN_STR, {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchSubtractInt64), SUBTRACT_FN_STR, {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchMultiplyInt64), MULTIPLY_FN_STR, {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchDivideInt64), DIVIDE_FN_STR, {}, longParams, OMNI_LONG, INPUT_DATA,
                 true),
        Function(reinterpret_cast<void *>(BatchModulusInt64), MODULUS_FN_STR, {}, longParams, OMNI_LONG, INPUT_DATA,
                 true),
        Function(reinterpret_cast<void *>(BatchLessThanInt64), LESS_THAN_FN_STR, {}, longParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualInt64), LESS_THAN_EQUAL_FN_STR, {}, longParams,
                 OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanInt64), GREATER_THAN_FN_STR, {}, longParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualInt64), GREATER_THAN_EQUAL_FN_STR, {}, longParams,
                 OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualInt64), EQUAL_FN_STR, {}, longParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualInt64), NOT_EQUAL_FN_STR, {}, longParams, OMNI_BOOLEAN,
                 INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchAddInt32), ADD_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchSubtractInt32), SUBTRACT_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchMultiplyInt32), MULTIPLY_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchDivideInt32), DIVIDE_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(BatchModulusInt32), MODULUS_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA,
                 true),
        Function(reinterpret_cast<void *>(BatchLessThanInt32), LESS_THAN_FN_STR, {}, intParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchLessThanEqualInt32), LESS_THAN_EQUAL_FN_STR, {}, intParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanInt32), GREATER_THAN_FN_STR, {}, intParams, OMNI_BOOLEAN,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreaterThanEqualInt32), GREATER_THAN_EQUAL_FN_STR, {}, intParams,
                 OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchEqualInt32), EQUAL_FN_STR, {}, intParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchNotEqualInt32), NOT_EQUAL_FN_STR, {}, intParams, OMNI_BOOLEAN,
                 INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchEqualBool), EQUAL_FN_STR, {}, boolParams, OMNI_BOOLEAN, INPUT_DATA),

        Function(reinterpret_cast<void *>(BatchPmod), PMOD_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchRound<int32_t>), ROUND_FN_STR, {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchRoundLong), ROUND_FN_STR, {}, {OMNI_LONG, OMNI_INT}, OMNI_LONG,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchRound<double>), ROUND_FN_STR, {}, {OMNI_DOUBLE, OMNI_INT}, OMNI_DOUBLE,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchGreatest<int32_t>), GREATEST_NUM_FN_STR, {}, {OMNI_INT, OMNI_INT},
                 OMNI_INT, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchGreatest<int64_t>), GREATEST_NUM_FN_STR, {}, {OMNI_LONG, OMNI_LONG},
                 OMNI_LONG, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchGreatest<bool>), GREATEST_NUM_FN_STR, {}, {OMNI_BOOLEAN, OMNI_BOOLEAN},
                 OMNI_BOOLEAN, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(BatchGreatest<double>), GREATEST_NUM_FN_STR, {}, {OMNI_DOUBLE, OMNI_DOUBLE},
                 OMNI_DOUBLE, INPUT_DATA_AND_NULL_AND_RETURN_NULL)};

    return batchMathFunctions;
}

std::vector<Function> BatchMathFunctionRegistryHalfUp::GetFunctions()
{
    std::vector<Function> batchMathFunctions = {
        Function(reinterpret_cast<void *>(BatchCastDoubleToInt64HalfUp), CAST_FN_STR, {}, {OMNI_DOUBLE}, OMNI_LONG,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDoubleToInt32HalfUp), CAST_FN_STR, {}, {OMNI_DOUBLE}, OMNI_INT,
                 INPUT_DATA),
    };

    return batchMathFunctions;
}

std::vector<Function> BatchMathFunctionRegistryDown::GetFunctions()
{
    std::vector<Function> batchMathFunctions = {
        Function(reinterpret_cast<void *>(BatchCastDoubleToInt64Down), CAST_FN_STR, {}, {OMNI_DOUBLE}, OMNI_LONG,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(BatchCastDoubleToInt32Down), CAST_FN_STR, {}, {OMNI_DOUBLE}, OMNI_INT,
                 INPUT_DATA),
    };

    return batchMathFunctions;
}
} // namespace omniruntime::codegen
