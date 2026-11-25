/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
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
 * Description: Decimal Function Registry
 */
#include "func_registry_decimal.h"
#include "functions/decimal_arithmetic_functions.h"
#include "functions/decimal_cast_functions.h"

namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace codegen::function;

const std::string DecimalCastFnStr()
{
    const std::string decimalCastFnStr = "CAST";
    return decimalCastFnStr;
}

const std::string DecimalCastNullFnStr()
{
    const std::string decimalCastNullFnStr = "CAST_null";
    return decimalCastNullFnStr;
}

const std::string DecimalGreatestFnStr()
{
    const std::string decimalGreatestFnStr = "Greatest";
    return decimalGreatestFnStr;
}

const std::string DecimalGreatestNullFnStr()
{
    const std::string decimalGreatestNullFnStr = "Greatest_null";
    return decimalGreatestNullFnStr;
}

const std::string DecimalLeastFnStr()
{
    const std::string decimalLeastFnStr = "Least";
    return decimalLeastFnStr;
}

const std::string DecimalLeastNullFnStr()
{
    const std::string decimalLeastNullFnStr = "Least_null";
    return decimalLeastNullFnStr;
}

const std::string DecimalAbsFnStr()
{
    const std::string decimalAbsFnStr = "abs";
    return decimalAbsFnStr;
}

const std::string MakeDecimalFnStr()
{
    const std::string makeDecimalFnStr = "MakeDecimal";
    return makeDecimalFnStr;
}

const std::string MakeDecimalNullFnStr()
{
    const std::string makeDecimalNullFnStr = "MakeDecimal_null";
    return makeDecimalNullFnStr;
}

const std::string DecimalRoundFnStr()
{
    const std::string decimalRoundFnStr = "round";
    return decimalRoundFnStr;
}

const std::string RoundNullFnStr()
{
    const std::string roundNullFnStr = "round_null";
    return roundNullFnStr;
}

const std::string UnscaledValueFnStr()
{
    const std::string unscaledValueFnStr = "UnscaledValue";
    return unscaledValueFnStr;
}

const std::string Decimal64CompareFnStr()
{
    const std::string decimal64CompareFnStr = "Decimal64Compare";
    return decimal64CompareFnStr;
}

const std::string Decimal128CompareFnStr()
{
    const std::string decimal128CompareFnStr = "Decimal128Compare";
    return decimal128CompareFnStr;
}

const std::string AddDecimal128FnStr()
{
    const std::string addDecimal128FnStr = "Add_decimal128";
    return addDecimal128FnStr;
}

const std::string SubDecimal128FnStr()
{
    const std::string subDecimal128FnStr = "Sub_decimal128";
    return subDecimal128FnStr;
}

const std::string MulDecimal128FnStr()
{
    const std::string mulDecimal128FnStr = "Mul_decimal128";
    return mulDecimal128FnStr;
}

const std::string DivDecimal128FnStr()
{
    const std::string divDecimal128FnStr = "Div_decimal128";
    return divDecimal128FnStr;
}

const std::string ModDecimal128FnStr()
{
    const std::string modDecimal128FnStr = "Mod_decimal128";
    return modDecimal128FnStr;
}

const std::string AddDecimal64FnStr()
{
    const std::string addDecimal64FnStr = "Add_decimal64";
    return addDecimal64FnStr;
}

const std::string SubDecimal64FnStr()
{
    const std::string subDecimal64FnStr = "Sub_decimal64";
    return subDecimal64FnStr;
}

const std::string MulDecimal64FnStr()
{
    const std::string mulDecimal64FnStr = "Mul_decimal64";
    return mulDecimal64FnStr;
}

const std::string DivDecimal64FnStr()
{
    const std::string divDecimal64FnStr = "Div_decimal64";
    return divDecimal64FnStr;
}

const std::string ModDecimal64FnStr()
{
    const std::string modDecimal64FnStr = "Mod_decimal64";
    return modDecimal64FnStr;
}

const std::string AddDecimal128NullFnStr()
{
    const std::string addDecimal128NullFnStr = "Add_decimal128_null";
    return addDecimal128NullFnStr;
}

const std::string SubDecimal128NullFnStr()
{
    const std::string subDecimal128NullFnStr = "Sub_decimal128_null";
    return subDecimal128NullFnStr;
}

const std::string MulDecimal128NullFnStr()
{
    const std::string mulDecimal128NullFnStr = "Mul_decimal128_null";
    return mulDecimal128NullFnStr;
}

const std::string DivDecimal128NullFnStr()
{
    const std::string divDecimal128NullFnStr = "Div_decimal128_null";
    return divDecimal128NullFnStr;
}

const std::string ModDecimal128NullFnStr()
{
    const std::string modDecimal128NullFnStr = "Mod_decimal128_null";
    return modDecimal128NullFnStr;
}

const std::string AddDecimal64NullFnStr()
{
    const std::string addDecimal64NullFnStr = "Add_decimal64_null";
    return addDecimal64NullFnStr;
}

const std::string SubDecimal64NullFnStr()
{
    const std::string subDecimal64NullFnStr = "Sub_decimal64_null";
    return subDecimal64NullFnStr;
}

const std::string MulDecimal64NullFnStr()
{
    const std::string mulDecimal64NullFnStr = "Mul_decimal64_null";
    return mulDecimal64NullFnStr;
}

const std::string DivDecimal64NullFnStr()
{
    const std::string divDecimal64NullFnStr = "Div_decimal64_null";
    return divDecimal64NullFnStr;
}

const std::string ModDecimal64NullFnStr()
{
    const std::string modDecimal64NullFnStr = "Mod_decimal64_null";
    return modDecimal64NullFnStr;
}

const std::string TryAddDecimal64FnStr() { return "Try_add_decimal64"; }

const std::string TryAddDecimal128FnStr() { return "Try_add_decimal128"; }

const std::string TrySubDecimal64FnStr() { return "Try_sub_decimal64"; }

const std::string TrySubDecimal128FnStr() { return "Try_sub_decimal128"; }

const std::string TryMulDecimal64FnStr() { return "Try_mul_decimal64"; }

const std::string TryMulDecimal128FnStr() { return "Try_mul_decimal128"; }

const std::string TryDivDecimal64FnStr() { return "Try_div_decimal64"; }

const std::string TryDivDecimal128FnStr() { return "Try_div_decimal128"; }

std::vector<Function> DecimalFunctionRegistry::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = {OMNI_DECIMAL128, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes64 = {OMNI_DECIMAL64, OMNI_DECIMAL64};
    std::vector<DataTypeId> paramTypes64Op128 = {OMNI_DECIMAL64, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes128Op64 = {OMNI_DECIMAL128, OMNI_DECIMAL64};
    DataTypeId retType128 = OMNI_DECIMAL128;
    DataTypeId retType64 = OMNI_DECIMAL64;

    static std::vector<Function> decimalFnRegistry = {
        Function(reinterpret_cast<void *>(CastDecimal64To64), DecimalCastFnStr(), {}, {OMNI_DECIMAL64}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128To128), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64To128), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DECIMAL128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128To64), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DECIMAL64, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(CastIntToDecimal64), DecimalCastFnStr(), {}, {OMNI_INT}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastInt16ToDecimal64), DecimalCastFnStr(), {}, {OMNI_SHORT}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastInt8ToDecimal64), DecimalCastFnStr(), {}, {OMNI_BYTE}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastLongToDecimal64), DecimalCastFnStr(), {}, {OMNI_LONG}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDoubleToDecimal64), DecimalCastFnStr(), {}, {OMNI_DOUBLE}, OMNI_DECIMAL64,
                 INPUT_DATA, true),

        Function(reinterpret_cast<void *>(CastIntToDecimal128), DecimalCastFnStr(), {}, {OMNI_INT}, OMNI_DECIMAL128,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastInt16ToDecimal128), DecimalCastFnStr(), {}, {OMNI_SHORT}, OMNI_DECIMAL128,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastInt8ToDecimal128), DecimalCastFnStr(), {}, {OMNI_BYTE}, OMNI_DECIMAL128,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastLongToDecimal128), DecimalCastFnStr(), {}, {OMNI_LONG}, OMNI_DECIMAL128,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDoubleToDecimal128), DecimalCastFnStr(), {}, {OMNI_DOUBLE},
                 OMNI_DECIMAL128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(AbsDecimal128), DecimalAbsFnStr(), {}, {OMNI_DECIMAL128}, retType128,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(AbsDecimal64), DecimalAbsFnStr(), {}, {OMNI_DECIMAL64}, OMNI_DECIMAL64,
                 INPUT_DATA),

        Function(reinterpret_cast<void *>(RoundDecimal128), DecimalRoundFnStr(), {}, {OMNI_DECIMAL128, OMNI_INT},
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(RoundDecimal64), DecimalRoundFnStr(), {}, {OMNI_DECIMAL64, OMNI_INT},
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(RoundDecimal128WithoutRound), DecimalRoundFnStr(), {}, {OMNI_DECIMAL128},
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(RoundDecimal64WithoutRound), DecimalRoundFnStr(), {}, {OMNI_DECIMAL64},
                 retType64, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(Decimal64Compare), Decimal64CompareFnStr(), {}, paramTypes64, OMNI_INT),
        Function(reinterpret_cast<void *>(Decimal128Compare), Decimal128CompareFnStr(), {}, paramTypes128, OMNI_INT),

        // Return Null
        Function(reinterpret_cast<void *>(RoundDecimal128RetNull), RoundNullFnStr(), {}, {OMNI_DECIMAL128, OMNI_INT},
                 retType128, INPUT_DATA),
        Function(reinterpret_cast<void *>(RoundDecimal64RetNull), RoundNullFnStr(), {}, {OMNI_DECIMAL64, OMNI_INT},
                 retType64, INPUT_DATA),

        Function(reinterpret_cast<void *>(AddDec64Dec64Dec64RetNull), AddDecimal64NullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec128RetNull), AddDecimal64NullFnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec128Dec128Dec128RetNull), AddDecimal128NullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec64Dec128Dec128RetNull), AddDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec128Dec64Dec128RetNull), AddDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec64RetNull), TryAddDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec128RetNull), TryAddDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec64Dec128Dec128RetNull), TryAddDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec128Dec128Dec128RetNull), TryAddDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(AddDec128Dec64Dec128RetNull), TryAddDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(SubDec64Dec64Dec64RetNull), SubDecimal64NullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec64Dec64Dec128RetNull), SubDecimal64NullFnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec128Dec128Dec128RetNull), SubDecimal128NullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec64Dec128Dec128RetNull), SubDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec128Dec64Dec128RetNull), SubDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec64Dec64Dec64RetNull), TrySubDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec64Dec64Dec128RetNull), TrySubDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec64Dec128Dec128RetNull), TrySubDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec128Dec128Dec128RetNull), TrySubDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(SubDec128Dec64Dec128RetNull), TrySubDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(MulDec64Dec64Dec64RetNull), MulDecimal64NullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec64Dec64Dec128RetNull), MulDecimal64NullFnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec128Dec128Dec128RetNull), MulDecimal128NullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec64Dec128Dec128RetNull), MulDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec128Dec64Dec128RetNull), MulDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec64Dec64Dec64RetNull), TryMulDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec64Dec64Dec128RetNull), TryMulDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec64Dec128Dec128RetNull), TryMulDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec128Dec128Dec128RetNull), TryMulDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(MulDec128Dec64Dec128RetNull), TryMulDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(DivDec64Dec64Dec64RetNull), DivDecimal64NullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec64RetNull), DivDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec64RetNull), DivDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec64Dec128RetNull), DivDecimal64NullFnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec128Dec128RetNull), DivDecimal128NullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec128RetNull), DivDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec128RetNull), DivDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec64Dec64RetNull), TryDivDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec64RetNull), TryDivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec64Dec128RetNull), TryDivDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec128RetNull), TryDivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec128Dec128RetNull), TryDivDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec64RetNull), TryDivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec128RetNull), TryDivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(ModDec64Dec64Dec64RetNull), ModDecimal64NullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec64RetNull), ModDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec64RetNull), ModDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec128RetNull), ModDecimal128NullFnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec128RetNull), ModDecimal128NullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec64RetNull), ModDecimal128NullFnStr(), {}, paramTypes128,
                 retType64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec128RetNull), ModDecimal64NullFnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(CastDecimal64To64RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128To128RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal64To128RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128To64RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(CastIntToDecimal64RetNull), DecimalCastNullFnStr(), {}, {OMNI_INT},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastInt16ToDecimal64RetNull), DecimalCastNullFnStr(), {}, {OMNI_SHORT},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastInt8ToDecimal64RetNull), DecimalCastNullFnStr(), {}, {OMNI_BYTE},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastLongToDecimal64RetNull), DecimalCastNullFnStr(), {}, {OMNI_LONG},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDoubleToDecimal64RetNull), DecimalCastNullFnStr(), {}, {OMNI_DOUBLE},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(CastIntToDecimal128RetNull), DecimalCastNullFnStr(), {}, {OMNI_INT},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastInt16ToDecimal128RetNull), DecimalCastNullFnStr(), {}, {OMNI_SHORT},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastInt8ToDecimal128RetNull), DecimalCastNullFnStr(), {}, {OMNI_BYTE},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastLongToDecimal128RetNull), DecimalCastNullFnStr(), {}, {OMNI_LONG},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDoubleToDecimal128RetNull), DecimalCastNullFnStr(), {}, {OMNI_DOUBLE},
                 OMNI_DECIMAL128, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(CastDecimal64ToIntRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_INT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt16RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_SHORT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt8RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_BYTE, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal64ToLongRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_LONG, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal64ToDoubleRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DOUBLE, INPUT_DATA_AND_OVERFLOW_NULL),

        Function(reinterpret_cast<void *>(CastDecimal128ToIntRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_INT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt16RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_SHORT, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt8RetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_BYTE, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128ToLongRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_LONG, INPUT_DATA_AND_OVERFLOW_NULL),
        Function(reinterpret_cast<void *>(CastDecimal128ToDoubleRetNull), DecimalCastNullFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DOUBLE, INPUT_DATA_AND_OVERFLOW_NULL),

        // UnscaledValue
        Function(reinterpret_cast<void *>(UnscaledValue64), UnscaledValueFnStr(), {}, {OMNI_DECIMAL64}, OMNI_LONG,
                 INPUT_DATA),
        // MakeDecimal
        Function(reinterpret_cast<void *>(MakeDecimal64), MakeDecimalFnStr(), {}, {OMNI_LONG}, OMNI_DECIMAL64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MakeDecimal64RetNull), MakeDecimalNullFnStr(), {}, {OMNI_LONG},
                 OMNI_DECIMAL64, INPUT_DATA_AND_OVERFLOW_NULL),
        // DecimalGreatest
        Function(reinterpret_cast<void *>(GreatestDecimal64), DecimalGreatestFnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(GreatestDecimal128), DecimalGreatestFnStr(), {}, paramTypes128, retType128,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(GreatestDecimal64RetNull), DecimalGreatestNullFnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(GreatestDecimal128RetNull), DecimalGreatestNullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        // DecimalLeast
        Function(reinterpret_cast<void *>(LeastDecimal64), DecimalLeastFnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(LeastDecimal128), DecimalLeastFnStr(), {}, paramTypes128, retType128,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL, true),
        Function(reinterpret_cast<void *>(LeastDecimal64RetNull), DecimalLeastNullFnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(LeastDecimal128RetNull), DecimalLeastNullFnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA_AND_NULL_AND_RETURN_NULL)};

    return decimalFnRegistry;
}

std::vector<Function> DecimalFunctionRegistryDown::GetFunctions()
{
    static std::vector<Function> decimalFnRegistry = {
        Function(reinterpret_cast<void *>(CastDecimal64ToLongDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL64}, OMNI_LONG,
                 INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDecimal64ToIntDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL64}, OMNI_INT,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt16Down), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_SHORT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt8Down), DecimalCastFnStr(), {}, {OMNI_DECIMAL64}, OMNI_BYTE,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToDoubleDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DOUBLE, INPUT_DATA),

        Function(reinterpret_cast<void *>(CastDecimal128ToLongDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_LONG, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToIntDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL128}, OMNI_INT,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt16Down), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_SHORT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt8Down), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_BYTE, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToDoubleDown), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DOUBLE, INPUT_DATA),
    };

    return decimalFnRegistry;
}

std::vector<Function> DecimalFunctionRegistryHalfUp::GetFunctions()
{
    static std::vector<Function> decimalFnRegistry = {
        Function(reinterpret_cast<void *>(CastDecimal64ToLongHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDecimal64ToIntHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL64}, OMNI_INT,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt16HalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_SHORT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToInt8HalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_BYTE, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal64ToDoubleHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL64},
                 OMNI_DOUBLE, INPUT_DATA),

        Function(reinterpret_cast<void *>(CastDecimal128ToLongHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_LONG, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToIntHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_INT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt16HalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_SHORT, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToInt8HalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_BYTE, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(CastDecimal128ToDoubleHalfUp), DecimalCastFnStr(), {}, {OMNI_DECIMAL128},
                 OMNI_DOUBLE, INPUT_DATA),
    };

    return decimalFnRegistry;
}

std::vector<Function> DecimalFunctionRegistryNotReScale::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = {OMNI_DECIMAL128, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes64 = {OMNI_DECIMAL64, OMNI_DECIMAL64};
    std::vector<DataTypeId> paramTypes64Op128 = {OMNI_DECIMAL64, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes128Op64 = {OMNI_DECIMAL128, OMNI_DECIMAL64};
    DataTypeId retType128 = OMNI_DECIMAL128;
    DataTypeId retType64 = OMNI_DECIMAL64;

    static std::vector<Function> decimalFnRegistry = {
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec64NotReScale), AddDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec128NotReScale), AddDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec128Dec128Dec128NotReScale), AddDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec64Dec128Dec128NotReScale), AddDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec128Dec64Dec128NotReScale), AddDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(SubDec64Dec64Dec64NotReScale), SubDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec64Dec64Dec128NotReScale), SubDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec128Dec128Dec128NotReScale), SubDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec64Dec128Dec128NotReScale), SubDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec128Dec64Dec128NotReScale), SubDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(MulDec64Dec64Dec64NotReScale), MulDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec64Dec64Dec128NotReScale), MulDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec128Dec128Dec128NotReScale), MulDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec64Dec128Dec128NotReScale), MulDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec128Dec64Dec128NotReScale), MulDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(DivDec64Dec64Dec64NotReScale), DivDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec64NotReScale), DivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec64NotReScale), DivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec64Dec128NotReScale), DivDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec128Dec128NotReScale), DivDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec128NotReScale), DivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec128NotReScale), DivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(ModDec64Dec64Dec64NotReScale), ModDecimal64FnStr(), {}, paramTypes64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec64NotReScale), ModDecimal64FnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec64NotReScale), ModDecimal128FnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec128ReScale), ModDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec128NotReScale), ModDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec64NotReScale), ModDecimal128FnStr(), {}, paramTypes128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec128NotReScale), ModDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
    };

    return decimalFnRegistry;
}

std::vector<Function> DecimalFunctionRegistryReScale::GetFunctions()
{
    std::vector<DataTypeId> paramTypes128 = {OMNI_DECIMAL128, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes64 = {OMNI_DECIMAL64, OMNI_DECIMAL64};
    std::vector<DataTypeId> paramTypes64Op128 = {OMNI_DECIMAL64, OMNI_DECIMAL128};
    std::vector<DataTypeId> paramTypes128Op64 = {OMNI_DECIMAL128, OMNI_DECIMAL64};
    DataTypeId retType128 = OMNI_DECIMAL128;
    DataTypeId retType64 = OMNI_DECIMAL64;

    static std::vector<Function> decimalFnRegistry = {
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec64ReScale), AddDecimal64FnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec64Dec64Dec128ReScale), AddDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec128Dec128Dec128ReScale), AddDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec64Dec128Dec128ReScale), AddDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(AddDec128Dec64Dec128ReScale), AddDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(SubDec64Dec64Dec64ReScale), SubDecimal64FnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec64Dec64Dec128ReScale), SubDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec128Dec128Dec128ReScale), SubDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec64Dec128Dec128ReScale), SubDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(SubDec128Dec64Dec128ReScale), SubDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(MulDec64Dec64Dec64ReScale), MulDecimal64FnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec64Dec64Dec128ReScale), MulDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec128Dec128Dec128ReScale), MulDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec64Dec128Dec128ReScale), MulDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(MulDec128Dec64Dec128ReScale), MulDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(DivDec64Dec64Dec64ReScale), DivDecimal64FnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec64ReScale), DivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec64ReScale), DivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec64Dec128ReScale), DivDecimal64FnStr(), {}, paramTypes64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec128Dec128ReScale), DivDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec64Dec128Dec128ReScale), DivDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(DivDec128Dec64Dec128ReScale), DivDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),

        Function(reinterpret_cast<void *>(ModDec64Dec64Dec64ReScale), ModDecimal64FnStr(), {}, paramTypes64, retType64,
                 INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec64ReScale), ModDecimal64FnStr(), {}, paramTypes64Op128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec64ReScale), ModDecimal128FnStr(), {}, paramTypes128Op64,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec64Dec128ReScale), ModDecimal128FnStr(), {}, paramTypes128Op64,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec128ReScale), ModDecimal128FnStr(), {}, paramTypes128,
                 retType128, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec128Dec128Dec64ReScale), ModDecimal128FnStr(), {}, paramTypes128,
                 retType64, INPUT_DATA, true),
        Function(reinterpret_cast<void *>(ModDec64Dec128Dec128ReScale), ModDecimal64FnStr(), {}, paramTypes64Op128,
                 retType128, INPUT_DATA, true),
    };

    return decimalFnRegistry;
}
} // namespace omniruntime::codegen
