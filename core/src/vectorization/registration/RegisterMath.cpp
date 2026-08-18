/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#include <string>
#include "../functions/Arithmetic.h"
#include "../functions/Comparisons.h"
#include "../functions/IsNull.h"
#include "../functions/MathFunctions.h"
#include "../functions/HexFunctions.h"
#include "../functions/BinFunction.h"
#include "../functions/ConvFunction.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterMathFunctions(const std::string &prefix)
{
    RegisterIsNullFunction(prefix + "isnull");
    RegisterBinaryNumeric<PlusFunction>({prefix + "add"});
    VectorFunction::RegisterVectorFunction("and", {OMNI_BOOLEAN, OMNI_BOOLEAN}, OMNI_BOOLEAN, std::make_shared<AndFunction>());
    VectorFunction::RegisterVectorFunction("or", {OMNI_BOOLEAN, OMNI_BOOLEAN}, OMNI_BOOLEAN, std::make_shared<OrFunction>());
    RegisterUnaryIntegral<Not>(prefix + "not");
    RegisterBinaryNumeric<MinusFunction>({prefix + "subtract"});
    RegisterBinaryNumeric<MultiplyFunction>({prefix + "multiply"});
    RegisterBinaryNumeric<DivideFunction>({prefix + "divide"});
    RegisterBinaryNumeric<RemainderFunction>({prefix + "modulus"});
    RegisterFunction<AbsFunction, int8_t, int8_t>(prefix + "abs", {OMNI_BYTE}, OMNI_BYTE);
    RegisterFunction<AbsFunction, int16_t, int16_t>(prefix + "abs", {OMNI_SHORT}, OMNI_SHORT);
    RegisterFunction<AbsFunction, int32_t, int32_t>(prefix + "abs", {OMNI_INT}, OMNI_INT);
    RegisterFunction<AbsFunction, int64_t, int64_t>(prefix + "abs", {OMNI_LONG}, OMNI_LONG);
    RegisterFunction<AbsFunction, float, float>(prefix + "abs", {OMNI_FLOAT}, OMNI_FLOAT);
    RegisterFunction<AbsFunction, double, double>(prefix + "abs", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<AcoshFunction, double, double>(prefix + "acosh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<AcosFunction, double, double>(prefix + "acos", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterUnaryNumeric<NegativeFunction>({prefix + "negative"});
	RegisterFunction<AsinFunction, double, double>(prefix + "asin", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<AsinhFunction, double, double>(prefix + "asinh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<AtanFunction, double, double>(prefix + "atan", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<AtanhFunction, double, double>(prefix + "atanh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<Atan2Function, double, double, double>(prefix + "atan2", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<CotFunction, double, double>(prefix + "cot", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<CosFunction, double, double>(prefix + "cos", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<CoshFunction, double, double>(prefix + "cosh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<SinFunction, double, double>(prefix + "sin", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<TanFunction, double, double>(prefix + "tan", {OMNI_DOUBLE}, OMNI_DOUBLE);
	RegisterFunction<CbrtFunction, double, double>(prefix + "cbrt", {OMNI_DOUBLE}, OMNI_DOUBLE);
	RegisterFunction<CeilFunction, int64_t, int64_t>(prefix + "ceil", {OMNI_LONG}, OMNI_LONG);
	RegisterFunction<CeilFunction, int64_t, double>(prefix + "ceil", {OMNI_DOUBLE}, OMNI_LONG);
    RegisterFunction<SignFunction, double, double>(prefix + "sign", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<SinhFunction, double, double>(prefix + "sinh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<TanhFunction, double, double>(prefix + "tanh", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<HypotFunction, double, double, double>(prefix + "hypot", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<SqrtFunction, double, double>(prefix + "sqrt", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<DegreesFunction, double, double>(prefix + "degrees", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<RadiansFunction, double, double>(prefix + "radians", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<ExpFunction, double, double>(prefix + "exp", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<SecFunction, double, double>(prefix + "sec", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<CscFunction, double, double>(prefix + "csc", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<LnFunction, double, double>(prefix + "ln", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<Log1pFunction, double, double>(prefix + "log1p", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<Log10Function, double, double>(prefix + "log10", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<Log2Function, double, double>(prefix + "log2", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<LogarithmFunction, double, double, double>(prefix + "log", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<Expm1Function, double, double>(prefix + "expm1", {OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterBinaryIntegral<PModIntFunction>({prefix + "pmod"});
    RegisterBinaryFloatingPoint<PModFloatFunction>({prefix + "pmod"});
    RegisterUnaryNumeric<PositiveFunction>(prefix + "positive");
    RegisterFunction<PowerFunction, double, double, double>(prefix + "power", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE);
    RegisterFunction<RintFunction, double, double>(prefix + "rint", {OMNI_DOUBLE}, OMNI_DOUBLE);
    // rand()/random() and rand(seed)/random(seed), aligned with Velox (rand + random as aliases)
    RegisterFunction<RandFunction, double>(prefix + "rand", {}, OMNI_DOUBLE);
    RegisterFunction<RandSeedFunctionInt32, double, int32_t>(prefix + "rand", {OMNI_INT}, OMNI_DOUBLE);
    RegisterFunction<RandSeedFunctionInt64, double, int64_t>(prefix + "rand", {OMNI_LONG}, OMNI_DOUBLE);
    RegisterFunction<RandFunction, double>(prefix + "random", {}, OMNI_DOUBLE);
    RegisterFunction<RandSeedFunctionInt32, double, int32_t>(prefix + "random", {OMNI_INT}, OMNI_DOUBLE);
    RegisterFunction<RandSeedFunctionInt64, double, int64_t>(prefix + "random", {OMNI_LONG}, OMNI_DOUBLE);
    // pi()/e() — 0-arg constant functions returning Math.PI / Math.E (DOUBLE).
    // Matches Flink PI()/E() (NILADIC, DOUBLE, deterministic constant).
    RegisterFunction<PiFunction, double>(prefix + "pi", {}, OMNI_DOUBLE);
    RegisterFunction<EFunction, double>(prefix + "e", {}, OMNI_DOUBLE);

    // Register round: round(expr) default scale=0, round(expr, scale)
    RegisterUnaryIntegralNumeric<RoundFunction>(prefix + "round");
    RegisterUnaryFloatingPoint<RoundFunction>(prefix + "round");
    RegisterRoundNumericWithScale<RoundFunction>(prefix + "round");

    // Register truncate: truncate(expr) default scale=0, truncate(expr, scale).
    // Mirrors Flink TRUNCATE(numeric, integer): truncates toward zero (DOWN) to `scale` decimals.
    // Same overload set as round (byte/short/int/long/float/double); DECIMAL not yet supported.
    RegisterUnaryIntegralNumeric<TruncateFunction>(prefix + "truncate");
    RegisterUnaryFloatingPoint<TruncateFunction>(prefix + "truncate");
    RegisterRoundNumericWithScale<TruncateFunction>(prefix + "truncate");

    // bin function: converts integer to its binary string (Flink BIN, Long.toBinaryString).
    // INT is sign-extended to 64 bits to match Flink's int->long widening.
    RegisterFunction<BinBigintFunction, std::string, int64_t>(prefix + "bin", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<BinIntFunction, std::string, int32_t>(prefix + "bin", {OMNI_INT}, OMNI_VARCHAR);

    // hex function: converts integer/string/binary to hexadecimal string
    RegisterFunction<HexBigintFunction, std::string, int64_t>(prefix + "hex", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<HexVarcharFunction, std::string, std::string_view>(prefix + "hex", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<HexVarcharFunction, std::string, std::string_view>(prefix + "hex", {OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<HexVarbinaryFunction, std::string, std::string_view>(prefix + "hex", {OMNI_VARBINARY}, OMNI_VARCHAR);

    // Register floor: floor(long) -> long, floor(double) -> long
    // In Spark, floor must return Long type
    RegisterFunction<FloorFunction, int64_t, int64_t>(prefix + "floor", {OMNI_LONG}, OMNI_LONG);
    RegisterFunction<FloorFunction, int64_t, double>(prefix + "floor", {OMNI_DOUBLE}, OMNI_LONG);

    // Register factorial: factorial(int) -> bigint
    // Input: int32 (OMNI_INT), Output: int64 (OMNI_LONG)
    RegisterFunction<FactorialFunction, int64_t, int32_t>(prefix + "factorial", {OMNI_INT}, OMNI_LONG);

    RegisterFunction<ConvFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "conv", {OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<ConvFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "conv", {OMNI_CHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);


    // Register div (integral division): div(a, b) -> int64_t
    // Supports: LONG, DECIMAL64, DECIMAL128
    // Returns NULL if divisor is 0
    // For Long.MIN_VALUE / -1, returns Long.MIN_VALUE (Java semantics)
    RegisterFunction<IntegralDivideFunction, int64_t, int64_t, int64_t>(prefix + "div", {OMNI_LONG, OMNI_LONG}, OMNI_LONG);
    RegisterFunction<IntegralDivideFunction, int64_t, int64_t, int64_t>(prefix + "div", {OMNI_DECIMAL64, OMNI_DECIMAL64}, OMNI_LONG);
    RegisterFunction<IntegralDivideFunction, int64_t, Decimal128, Decimal128>(prefix + "div", {OMNI_DECIMAL128, OMNI_DECIMAL128}, OMNI_LONG);

    // Register width_bucket: width_bucket(value, bound1, bound2, numBuckets) -> int64_t
    // Returns the bucket number (0-based) for value in an equiwidth histogram
    // Supports: DOUBLE for value/bound1/bound2, LONG for numBuckets
    RegisterFunction<WidthBucketFunction, int64_t, double, double, double, int64_t>(
        prefix + "width_bucket", {OMNI_DOUBLE, OMNI_DOUBLE, OMNI_DOUBLE, OMNI_LONG}, OMNI_LONG);
    RegisterFunction<NormalizeNaNAndZero, float, float>(prefix + "NormalizeNaNAndZero", {OMNI_FLOAT}, OMNI_FLOAT);
    RegisterFunction<NormalizeNaNAndZero, double, double>(prefix + "NormalizeNaNAndZero", {OMNI_DOUBLE}, OMNI_DOUBLE);
}
}
