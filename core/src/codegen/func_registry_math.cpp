/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Math Functions Registry
 */
#include "func_registry_math.h"
#include "functions/mathfunctions.h"
namespace omniruntime::codegen {
using namespace omniruntime::type;
using namespace omniruntime::codegen::function;

const std::string AbsFnStr()
{
    const std::string absFnStr = "abs";
    return absFnStr;
}

const std::string RoundFnStr()
{
    const std::string roundFnStr = "round";
    return roundFnStr;
}

const std::string FloorFnStr()
{
    const std::string floorFnStr = "floor";
    return floorFnStr;
}

const std::string AddFnStr()
{
    const std::string addFnStr = "add";
    return addFnStr;
}

const std::string SubtractFnStr()
{
    const std::string subtractFnStr = "subtract";
    return subtractFnStr;
}

const std::string MultiplyFnStr()
{
    const std::string multiplyFnStr = "multiply";
    return multiplyFnStr;
}

const std::string DivideFnStr()
{
    const std::string divideFnStr = "divide";
    return divideFnStr;
}

const std::string DivFnStr()
{
    const std::string divFnStr = "div";
    return divFnStr;
}

const std::string ModulusFnStr()
{
    const std::string modulusFnStr = "modulus";
    return modulusFnStr;
}

const std::string LessThanFnStr()
{
    const std::string lessThanFnStr = "lessThan";
    return lessThanFnStr;
}

const std::string LessThanEqualFnStr()
{
    const std::string lessThanEqualFnStr = "lessThanEqual";
    return lessThanEqualFnStr;
}

const std::string GreaterThanFnStr()
{
    const std::string greaterThanFnStr = "greaterThan";
    return greaterThanFnStr;
}

const std::string GreaterThanEqualFnStr()
{
    const std::string greaterThanEqualFnStr = "greaterThanEqual";
    return greaterThanEqualFnStr;
}

const std::string EqualFnStr()
{
    const std::string equalFnStr = "equal";
    return equalFnStr;
}

const std::string NotEqualFnStr()
{
    const std::string notEqualFnStr = "notEqual";
    return notEqualFnStr;
}

const std::string MathCastFnStr()
{
    const std::string mathCastFnStr = "CAST";
    return mathCastFnStr;
}

const std::string PmodFnStr()
{
    const std::string pmodFnStr = "pmod";
    return pmodFnStr;
}

const std::string NormalizeNaNAndZeroFnStr()
{
    const std::string normalizeNaNAndZeroFnStr = "NormalizeNaNAndZero";
    return normalizeNaNAndZeroFnStr;
}

const std::string NanvlFnStr()
{
    const std::string nanvlFnStr = "nanvl";
    return nanvlFnStr;
}

const std::string GreatestFnStr()
{
    const std::string greatestFnStr = "Greatest";
    return greatestFnStr;
}

const std::string PowerFnStr()
{
    const std::string powerFnStr = "power";
    return powerFnStr;
}

const std::string TryAddFnStr()
{
    const std::string addFnStr = "try_add";
    return addFnStr;
}

const std::string TrySubtractFnStr()
{
    const std::string subtractFnStr = "try_subtract";
    return subtractFnStr;
}

const std::string TryMultiplyFnStr()
{
    const std::string multiplyFnStr = "try_multiply";
    return multiplyFnStr;
}

const std::string TryDivideFnStr()
{
    const std::string tryDivideFnStr = "try_divide";
    return tryDivideFnStr;
}

const std::string BitwiseAndFnStr()
{
    const std::string bitwiseAndStr = "bitwise_and";
    return bitwiseAndStr;
}

const std::string BitwiseOrFnStr()
{
    const std::string bitwiseOrStr = "bitwise_or";
    return bitwiseOrStr;
}

const std::string ShiftRightFnStr()
{
    const std::string shiftStr = "shiftright";
    return shiftStr;
}

const std::string ShiftLeftFnStr()
{
    const std::string shiftStr = "shiftleft";
    return shiftStr;
}

const std::string NegativeFnStr()
{
    const std::string shiftStr = "negative";
    return shiftStr;
}

std::vector<Function> MathFunctionRegistry::GetFunctions()
{
    const std::vector<omniruntime::type::DataTypeId> doubleParams = { OMNI_DOUBLE, OMNI_DOUBLE };
    const std::vector<omniruntime::type::DataTypeId> floatParams = { OMNI_FLOAT, OMNI_FLOAT };
    const std::vector<omniruntime::type::DataTypeId> longParams = { OMNI_LONG, OMNI_LONG };
    const std::vector<omniruntime::type::DataTypeId> intParams = { OMNI_INT, OMNI_INT };
    const std::vector<omniruntime::type::DataTypeId> shortParams = { OMNI_SHORT, OMNI_SHORT };
    const std::vector<omniruntime::type::DataTypeId> byteParams = { OMNI_BYTE, OMNI_BYTE} ;

    std::vector<Function> mathFnRegistry = {
        // insert native functions for each absolute math function
        Function(reinterpret_cast<void *>(CastInt32ToInt16), MathCastFnStr(), {}, { OMNI_INT }, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt32ToInt8), MathCastFnStr(), {}, { OMNI_INT }, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt64ToInt16), MathCastFnStr(), {}, { type::OMNI_LONG }, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt64ToInt8), MathCastFnStr(), {}, { type::OMNI_LONG }, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(Abs<int32_t>), AbsFnStr(), {}, { OMNI_INT }, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Abs<int64_t>), AbsFnStr(), {}, { OMNI_LONG }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(Abs<double>), AbsFnStr(), {}, { OMNI_DOUBLE }, OMNI_DOUBLE, INPUT_DATA),

        // insert native functions for each cast math function
        Function(reinterpret_cast<void *>(CastInt32ToDouble), MathCastFnStr(), {}, { OMNI_INT }, OMNI_DOUBLE,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt64ToDouble), MathCastFnStr(), {}, { OMNI_LONG }, OMNI_DOUBLE,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt32ToFloat), MathCastFnStr(), {}, { OMNI_INT }, OMNI_FLOAT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt64ToFloat), MathCastFnStr(), {}, { OMNI_LONG }, OMNI_FLOAT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt16ToFloat), MathCastFnStr(), {}, { OMNI_SHORT }, OMNI_FLOAT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt8ToFloat), MathCastFnStr(), {}, { OMNI_BYTE }, OMNI_FLOAT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDate32ToTimestampMicros), MathCastFnStr(), {},
            { OMNI_DATE32 }, OMNI_TIMESTAMP, INPUT_DATA),
        // Spark: cast(ts as double/float) uses seconds since epoch, not raw microseconds.
        Function(reinterpret_cast<void *>(CastTimestampMicrosToDoubleSeconds), MathCastFnStr(), {},
            { OMNI_TIMESTAMP }, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastTimestampMicrosToFloatSeconds), MathCastFnStr(), {},
            { OMNI_TIMESTAMP }, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastFloatToDouble), MathCastFnStr(), {}, { OMNI_FLOAT }, OMNI_DOUBLE,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt32ToInt64), MathCastFnStr(), {}, { OMNI_INT }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt64ToInt32), MathCastFnStr(), {}, { OMNI_LONG }, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt16ToInt32), MathCastFnStr(), {}, { OMNI_SHORT }, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt8ToInt32), MathCastFnStr(), {}, { OMNI_BYTE }, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt8ToInt16), MathCastFnStr(), {}, { OMNI_BYTE }, OMNI_SHORT, INPUT_DATA),

        Function(reinterpret_cast<void *>(CastInt16ToInt64), MathCastFnStr(), {}, { OMNI_SHORT }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt8ToInt64), MathCastFnStr(), {}, { OMNI_BYTE }, OMNI_LONG, INPUT_DATA),

        Function(reinterpret_cast<void *>(CastInt16ToDouble), MathCastFnStr(), {}, { OMNI_SHORT }, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(CastInt8ToDouble), MathCastFnStr(), {}, { OMNI_BYTE }, OMNI_DOUBLE, INPUT_DATA),

        // insert native function for each double operations
        Function(reinterpret_cast<void *>(AddDouble), AddFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractDouble), SubtractFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyDouble), MultiplyFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideDouble), DivideFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusDouble), ModulusFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanDouble), LessThanFnStr(), {}, doubleParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualDouble), LessThanEqualFnStr(), {}, doubleParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanDouble), GreaterThanFnStr(), {}, doubleParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualDouble), GreaterThanEqualFnStr(), {}, doubleParams,
            OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualDouble), EqualFnStr(), {}, doubleParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualDouble), NotEqualFnStr(), {}, doubleParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NormalizeNaNAndZero), NormalizeNaNAndZeroFnStr(), {}, { OMNI_DOUBLE },
            OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(PowerDouble), PowerFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),

        // insert native function for each float operations
        Function(reinterpret_cast<void *>(AddFloat), AddFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractFloat), SubtractFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyFloat), MultiplyFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideFloat), DivideFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusFloat), ModulusFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanFloat), LessThanFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualFloat), LessThanEqualFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanFloat), GreaterThanFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualFloat), GreaterThanEqualFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualFloat), EqualFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualFloat), NotEqualFnStr(), {}, floatParams, OMNI_BOOLEAN, INPUT_DATA),

        // insert native function for each long operations
        Function(reinterpret_cast<void *>(AddInt64), AddFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt64), SubtractFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt64), MultiplyFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt64), DivideFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivInt64), DivFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusInt64), ModulusFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(AddInt64RetNull), TryAddFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt64RetNull), TrySubtractFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt64RetNull), TryMultiplyFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt64), TryDivideFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanInt64), LessThanFnStr(), {}, longParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualInt64), LessThanEqualFnStr(), {}, longParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanInt64), GreaterThanFnStr(), {}, longParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualInt64), GreaterThanEqualFnStr(), {}, longParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualInt64), EqualFnStr(), {}, longParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualInt64), NotEqualFnStr(), {}, longParams, OMNI_BOOLEAN, INPUT_DATA),

        // insert native function for each int operations
        Function(reinterpret_cast<void *>(AddInt32), AddFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt32), SubtractFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt32), MultiplyFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt32), DivideFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusInt32), ModulusFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(AddInt32RetNull), TryAddFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt32RetNull), TrySubtractFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt32RetNull), TryMultiplyFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt32), TryDivideFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanInt32), LessThanFnStr(), {}, intParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualInt32), LessThanEqualFnStr(), {}, intParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanInt32), GreaterThanFnStr(), {}, intParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualInt32), GreaterThanEqualFnStr(), {}, intParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualInt32), EqualFnStr(), {}, intParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualInt32), NotEqualFnStr(), {}, intParams, OMNI_BOOLEAN, INPUT_DATA),

        // insert pmod function for project operator support
        Function(reinterpret_cast<void *>(Pmod<int16_t>), PmodFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Pmod<int32_t>), PmodFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Pmod<int64_t>), PmodFnStr(), {}, longParams, OMNI_LONG, INPUT_DATA),
        // insert native functions for each round math function
        Function(reinterpret_cast<void *>(Round<int32_t>), RoundFnStr(), {}, intParams, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(RoundLong), RoundFnStr(), {}, { OMNI_LONG, OMNI_INT }, OMNI_LONG,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(Round<double>), RoundFnStr(), {}, { OMNI_DOUBLE, OMNI_INT }, OMNI_DOUBLE,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(Greatest<int32_t>), GreatestFnStr(), {}, { OMNI_INT, OMNI_INT }, OMNI_INT,
            INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(Greatest<int64_t>), GreatestFnStr(), {}, { OMNI_LONG, OMNI_LONG }, OMNI_LONG,
            INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(Greatest<bool>), GreatestFnStr(), {}, { OMNI_BOOLEAN, OMNI_BOOLEAN },
            OMNI_BOOLEAN, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(Greatest<double>), GreatestFnStr(), {}, { OMNI_DOUBLE, OMNI_DOUBLE },
            OMNI_DOUBLE, INPUT_DATA_AND_NULL_AND_RETURN_NULL),
        Function(reinterpret_cast<void *>(Floor<int64_t>), FloorFnStr(), {}, { OMNI_LONG }, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(Floor<double>), FloorFnStr(), {}, { OMNI_DOUBLE }, OMNI_LONG, INPUT_DATA),

        // nanvl: returns expr1 if not NaN, otherwise returns expr2 (float and double)
        Function(reinterpret_cast<void *>(Nanvl<float>), NanvlFnStr(), {}, floatParams, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Nanvl<double>), NanvlFnStr(), {}, doubleParams, OMNI_DOUBLE, INPUT_DATA),

        // insert native function for each short operations
        Function(reinterpret_cast<void *>(AddInt16), AddFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt16), SubtractFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt16), MultiplyFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt16), DivideFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusInt16), ModulusFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(AddInt16RetNull), TryAddFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt16RetNull), TrySubtractFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt16RetNull), TryMultiplyFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt16), TryDivideFnStr(), {}, shortParams, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanInt16), LessThanFnStr(), {}, shortParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualInt16), LessThanEqualFnStr(), {}, shortParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanInt16), GreaterThanFnStr(), {}, shortParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualInt16), GreaterThanEqualFnStr(), {}, shortParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualInt16), EqualFnStr(), {}, shortParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualInt16), NotEqualFnStr(), {}, shortParams, OMNI_BOOLEAN, INPUT_DATA),

        // insert native function for each byte operations
        Function(reinterpret_cast<void *>(AddInt8), AddFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt8), SubtractFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt8), MultiplyFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt8), DivideFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(ModulusInt8), ModulusFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(AddInt8RetNull), TryAddFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(SubtractInt8RetNull), TrySubtractFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(MultiplyInt8RetNull), TryMultiplyFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(DivideInt8), TryDivideFnStr(), {}, byteParams, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanInt8), LessThanFnStr(), {}, byteParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(LessThanEqualInt8), LessThanEqualFnStr(), {}, byteParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanInt8), GreaterThanFnStr(), {}, byteParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(GreaterThanEqualInt8), GreaterThanEqualFnStr(), {}, byteParams, OMNI_BOOLEAN,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(EqualInt8), EqualFnStr(), {}, byteParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(NotEqualInt8), NotEqualFnStr(), {}, byteParams, OMNI_BOOLEAN, INPUT_DATA),
        Function(reinterpret_cast<void *>(ExpFunction), "exp", {}, {OMNI_DOUBLE}, OMNI_DOUBLE, INPUT_DATA),
        Function(reinterpret_cast<void *>(Expm1Function), "expm1", {}, {OMNI_DOUBLE}, OMNI_DOUBLE, INPUT_DATA),

        Function(reinterpret_cast<void *>(BitwiseAndFunction<int8_t>), BitwiseAndFnStr(), {}, { OMNI_BYTE, OMNI_BYTE}, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseAndFunction<int16_t>), BitwiseAndFnStr(), {}, { OMNI_SHORT, OMNI_SHORT}, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseAndFunction<int32_t>), BitwiseAndFnStr(), {}, { OMNI_INT, OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseAndFunction<int64_t>), BitwiseAndFnStr(), {}, { OMNI_LONG, OMNI_LONG}, OMNI_LONG, INPUT_DATA),

        Function(reinterpret_cast<void *>(BitwiseOrFunction<int8_t>), BitwiseOrFnStr(), {}, { OMNI_BYTE, OMNI_BYTE}, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseOrFunction<int16_t>), BitwiseOrFnStr(), {}, { OMNI_SHORT, OMNI_SHORT}, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseOrFunction<int32_t>), BitwiseOrFnStr(), {}, { OMNI_INT, OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(BitwiseOrFunction<int64_t>), BitwiseOrFnStr(), {}, { OMNI_LONG, OMNI_LONG}, OMNI_LONG, INPUT_DATA),

        Function(reinterpret_cast<void *>(ShiftRight<int32_t, int32_t>), ShiftRightFnStr(), {}, { OMNI_INT, OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(ShiftRight<int64_t, int32_t>), ShiftRightFnStr(), {}, { OMNI_LONG, OMNI_INT}, OMNI_LONG, INPUT_DATA),

        Function(reinterpret_cast<void *>(ShiftLeft<int32_t, int32_t>), ShiftLeftFnStr(), {}, { OMNI_INT, OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(ShiftLeft<int64_t, int32_t>), ShiftLeftFnStr(), {}, { OMNI_LONG, OMNI_INT}, OMNI_LONG, INPUT_DATA),

        Function(reinterpret_cast<void *>(Negative<int8_t>), NegativeFnStr(), {}, { OMNI_BYTE}, OMNI_BYTE, INPUT_DATA),
        Function(reinterpret_cast<void *>(Negative<int16_t>), NegativeFnStr(), {}, { OMNI_SHORT}, OMNI_SHORT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Negative<int32_t>), NegativeFnStr(), {}, { OMNI_INT}, OMNI_INT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Negative<int64_t>), NegativeFnStr(), {}, { OMNI_LONG}, OMNI_LONG, INPUT_DATA),
        Function(reinterpret_cast<void *>(Negative<float>), NegativeFnStr(), {}, { OMNI_FLOAT}, OMNI_FLOAT, INPUT_DATA),
        Function(reinterpret_cast<void *>(Negative<double>), NegativeFnStr(), {}, { OMNI_DOUBLE}, OMNI_DOUBLE, INPUT_DATA)
    };

    return mathFnRegistry;
}

std::vector<Function> MathFunctionRegistryHalfUp::GetFunctions()
{
    std::vector<Function> mathFnRegistry = {
        // insert native functions for each absolute math function
        Function(reinterpret_cast<void *>(CastDoubleToInt64HalfUp), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_LONG,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt32HalfUp), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_INT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt16HalfUp), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_SHORT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt8HalfUp), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_BYTE,
            INPUT_DATA),
    };

    return mathFnRegistry;
}

std::vector<Function> MathFunctionRegistryDown::GetFunctions()
{
    std::vector<Function> mathFnRegistry = {
        // insert native functions for each absolute math function
        Function(reinterpret_cast<void *>(CastDoubleToInt64Down), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_LONG,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt32Down), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_INT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt16Down), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_SHORT,
            INPUT_DATA),
        Function(reinterpret_cast<void *>(CastDoubleToInt8Down), MathCastFnStr(), {}, { OMNI_DOUBLE }, OMNI_BYTE,
            INPUT_DATA),
    };

    return mathFnRegistry;
}
}
