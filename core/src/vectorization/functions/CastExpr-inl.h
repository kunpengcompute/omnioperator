/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */
#pragma once
#include "vectorization/SelectivityVector.h"
#include "type/Conversions.h"
#include "util/omni_exception.h"
#include "type/decimal_operations.h"

namespace omniruntime::vectorization {
namespace detail {
inline std::string makeErrorMessage(BaseVector *input, vector_size_t row, const DataTypePtr &toType,
    const std::string &details = "")
{
    return fmt::format("Cannot cast {} to {}. {}", std::to_string(input->GetTypeId()), toType->toString(), details);
}

/// Represent the varchar fragment.
///
/// For example:
/// | value | wholeDigits | fractionalDigits | exponent | sign |
/// | 9999999999.99 | 9999999999 | 99 | nullopt | 1 |
/// | 15 | 15 |  | nullopt | 1 |
/// | 1.5 | 1 | 5 | nullopt | 1 |
/// | -1.5 | 1 | 5 | nullopt | -1 |
/// | 31.523e-2 | 31 | 523 | -2 | 1 |
struct DecimalComponents {
    std::string_view wholeDigits;
    std::string_view fractionalDigits;
    std::optional<int32_t> exponent = std::nullopt;
    int8_t sign = 1;
};

// Extract a string view of continuous digits.
std::string_view extractDigits(const char *s, size_t start, size_t size);

/// Parse decimal components, including whole digits, fractional digits,
/// exponent and sign, from input chars. Returns error status if input chars
/// do not represent a valid value.
Status parseDecimalComponents(const char *s, size_t size, DecimalComponents &out);

/// Parse huge int from decimal components. The fractional part is scaled up by
/// required power of 10, and added with the whole part. Returns error status if
/// overflows.
Status parseHugeInt(const DecimalComponents &decimalComponents, int128_t &out);

/// Converts string view to decimal value of given precision and scale.
/// Derives from Arrow function DecimalFromString. Arrow implementation:
/// https://github.com/apache/arrow/blob/main/cpp/src/arrow/util/decimal.cc#L637.
///
/// Firstly, it parses the varchar to DecimalComponents which contains the
/// message that can represent a decimal value. Secondly, processes the exponent
/// to get the scale. Thirdly, compute the rescaled value. Returns status for
/// the outcome of computing.
template <typename T>
Status toDecimalValue(const std::string_view s, int toPrecision, int toScale, T &decimalValue)
{
    DecimalComponents decimalComponents;
    if (auto status = parseDecimalComponents(s.data(), s.size(), decimalComponents); !status.ok()) {
        return Status::UserError("Value is not a number. " + status.message());
    }

    // Count number of significant digits (without leading zeros).
    const size_t firstNonZero = decimalComponents.wholeDigits.find_first_not_of('0');
    size_t significantDigits = decimalComponents.fractionalDigits.size();
    if (firstNonZero != std::string::npos) {
        significantDigits += decimalComponents.wholeDigits.size() - firstNonZero;
    }
    int32_t parsedPrecision = static_cast<int32_t>(significantDigits);

    int32_t parsedScale = 0;
    bool roundUp = false;
    const int32_t fractionSize = decimalComponents.fractionalDigits.size();
    if (!decimalComponents.exponent.has_value()) {
        if (fractionSize > toScale) {
            if (decimalComponents.fractionalDigits[toScale] >= '5') {
                roundUp = true;
            }
            parsedScale = toScale;
            decimalComponents.fractionalDigits = std::string_view(decimalComponents.fractionalDigits.data(), toScale);
        } else {
            parsedScale = fractionSize;
        }
    } else {
        const auto exponent = decimalComponents.exponent.value();
        parsedScale = -exponent + fractionSize;
        // Truncate the fractionalDigits.
        if (parsedScale > toScale) {
            if (-exponent >= toScale) {
                // The fractional digits could be dropped.
                if (fractionSize > 0 && decimalComponents.fractionalDigits[0] >= '5') {
                    roundUp = true;
                }
                decimalComponents.fractionalDigits = "";
                parsedScale -= fractionSize;
            } else {
                const auto reduceDigits = exponent + toScale;
                if (fractionSize > reduceDigits && decimalComponents.fractionalDigits[reduceDigits] >= '5') {
                    roundUp = true;
                }
                decimalComponents.fractionalDigits = std::string_view(decimalComponents.fractionalDigits.data(),
                    std::min(reduceDigits, fractionSize));
                parsedScale -= fractionSize - decimalComponents.fractionalDigits.size();
            }
        }
    }

    int128_t out = 0;
    if (auto status = parseHugeInt(decimalComponents, out); !status.ok()) {
        return status;
    }

    if (roundUp) {
        bool overflow = __builtin_add_overflow(out, 1, &out);
        if (UNLIKELY(overflow)) {
            return Status::UserError("Value too large.");
        }
    }
    out *= decimalComponents.sign;

    if (parsedScale < 0) {
        /// Force the scale to be zero, to avoid negative scales (due to
        /// compatibility issues with external systems such as databases).
        if (-parsedScale + toScale > Decimal128Wrapper<>::kMaxPrecision) {
            return Status::UserError("Value too large.");
        }

        bool overflow = __builtin_mul_overflow(out, kPowersOfTen[-parsedScale + toScale], &out);
        if (UNLIKELY(overflow)) {
            return Status::UserError("Value too large.");
        }
        parsedPrecision -= parsedScale;
        parsedScale = toScale;
    }
    const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, T>(out,
        std::min(static_cast<uint8_t>(parsedPrecision), Decimal128Wrapper<>::kMaxPrecision), parsedScale, toPrecision,
        toScale, decimalValue);
    if (!status.ok()) {
        return Status::UserError("Value too large.");
    }
    return status;
}
}

template <typename Func>
void CastExpr::applyToSelectedNoThrowLocal(ExecutionContext &context, const SelectivityVector &rows, BaseVector *result,
    Func &&func)
{
    if (setNullInResultAtError()) {
        rows.applyToSelected([&](auto row) INLINE_LAMBDA {
            try {
                func(row);
            } catch (const OmniException &e) {
                result->SetNull(row);
            } catch (const std::exception &) {
                result->SetNull(row);
            }
        });
    } else {
        rows.applyToSelected([&](auto row) INLINE_LAMBDA {
            try {
                func(row);
            } catch (const OmniException &e) {
                if (!e.isUserError()) {
                    throw;
                }
                // Avoid double throwing.
                context.SetOmniExceptionError(row, std::current_exception());
            } catch (const std::exception &) {
                context.SetError(row, std::current_exception());
            }
        });
    }
}

template <DataTypeId ToKind, DataTypeId FromKind>
void CastExpr::applyCastPrimitives(const SelectivityVector &rows, ExecutionContext &context, BaseVector *input,
    VectorPtr &result)
{
    using To = typename NativeType<ToKind>::type;
    using From = typename NativeType<FromKind>::type;
    auto *resultFlatVector = reinterpret_cast<Vector<To> *>(result);

    applyToSelectedNoThrowLocal(context, rows, result, [&](int row) {
        applyCastKernel<ToKind, FromKind, omniruntime::type::util::SparkCastPolicy>(row, context, input,
            resultFlatVector);
    });
}

template <DataTypeId ToKind, DataTypeId FromKind, typename TPolicy>
void CastExpr::applyCastKernel(vector_size_t row, ExecutionContext &context, BaseVector *input,
    BaseVector *result)
{
    using TargetElementType = std::conditional_t<ToKind == OMNI_CHAR || ToKind == OMNI_VARCHAR,
            LargeStringContainer<std::string_view>, typename NativeType<ToKind>::type>;
    auto resultFlat = reinterpret_cast<Vector<TargetElementType> *>(result);
    bool wrapException = true;
    auto setError = [&](const std::string &details) INLINE_LAMBDA {
        if (setNullInResultAtError()) {
            result->SetNull(row);
        } else {
            wrapException = false;
            if (context.captureErrorDetails()) {
                const auto errorDetails =
                    detail::makeErrorMessage(input, row, std::make_shared<IntDataType>(), details);
                context.SetStatus(row, Status::UserError("{}", errorDetails));
            } else {
                context.SetStatus(row, Status::UserError());
            }
        }
    };

    // If castResult has an error, set the error in context. Otherwise, set the
    // value in castResult directly to result. This lambda should be called only
    // when ToKind is primitive and is not VARCHAR or VARBINARY.
    auto setResultOrError = [&](const auto &castResult, vector_size_t errorRow) INLINE_LAMBDA {
        if (castResult.hasError()) {
            setError(castResult.error().message());
        } else {
            resultFlat->SetValue(errorRow, castResult.value());
        }
    };

    try {
        using TargetElementType = std::conditional_t<FromKind == OMNI_CHAR || FromKind == OMNI_VARCHAR,
            LargeStringContainer<std::string_view>, typename NativeType<FromKind>::type>;
        auto inputFlat = reinterpret_cast<Vector<TargetElementType> *>(input);
        auto inputRowValue = inputFlat->GetValue(row);

        if constexpr ((FromKind == OMNI_BYTE || FromKind == OMNI_SHORT || FromKind == OMNI_INT || FromKind == OMNI_LONG)
            && ToKind == OMNI_TIMESTAMP) {
            const auto castResult = hooks_->castIntToTimestamp(static_cast<int64_t>(inputRowValue));
            setResultOrError(castResult, row);
            return;
        }

        if constexpr (FromKind == OMNI_DOUBLE && ToKind == OMNI_TIMESTAMP) {
            const auto castResult = hooks_->castDoubleToTimestamp(static_cast<double>(inputRowValue));
            if (castResult.hasError()) {
                setError(castResult.error().message());
            } else {
                if (castResult.value().has_value()) {
                    resultFlat->SetValue(row, castResult.value().value());
                } else {
                    result->SetNull(row);
                }
            }
            return;
        }

        // Optimize empty input strings casting by avoiding throwing exceptions.
        if constexpr (FromKind == OMNI_VARCHAR || FromKind == OMNI_VARBINARY) {
            if constexpr (NativeType<ToKind>::isPrimitiveType && NativeType<ToKind>::isFixedWidth) {
                inputRowValue = hooks_->removeWhiteSpaces(inputRowValue);
                if (inputRowValue.size() == 0) {
                    setError("Empty string");
                    return;
                }
            }
            if constexpr (ToKind == OMNI_TIMESTAMP) {
                const auto castResult = hooks_->castStringToTimestamp(inputRowValue);
                setResultOrError(castResult, row);
                return;
            }
            if constexpr (ToKind == OMNI_DOUBLE) {
                const auto castResult = hooks_->castStringToDouble(inputRowValue);
                setResultOrError(castResult, row);
                return;
            }
        }

        const auto castResult = omniruntime::type::util::Converter<ToKind, void, TPolicy>::tryCast(inputRowValue);
        if (castResult.hasError()) {
            setError(castResult.error().message());
            return;
        }

        const auto output = castResult.value();

        if constexpr (ToKind == OMNI_VARCHAR || ToKind == OMNI_VARBINARY) {
            // Write the result output to the output vector
            auto tmpStr = std::string_view(output);
            resultFlat->SetValue(row, tmpStr);
        } else {
            resultFlat->SetValue(row, output);
        }
    } catch (const OmniException &ue) {
        setError(ue.what());
    } catch (const std::exception &e) {
        setError(e.what());
    }
}

template <typename TInput, typename TOutput>
void CastExpr::applyDecimalCastKernel(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType, const DataTypePtr &toType, VectorPtr &castResult)
{
    auto sourceVector = reinterpret_cast<Vector<TInput> *>(input);
    auto result = reinterpret_cast<Vector<TOutput> *>(castResult);
    auto castResultRawBuffer = reinterpret_cast<TOutput *>(VectorHelper::UnsafeGetValues(result));
    const auto &fromPrecisionScale = GetDecimalPrecisionScale(*fromType);
    const auto &toPrecisionScale = GetDecimalPrecisionScale(*toType);

    applyToSelectedNoThrowLocal(context, rows, castResult, [&](vector_size_t row) {
        TOutput rescaledValue;
        const auto status = DecimalOperations::rescaleWithRoundUp<TInput, TOutput>(sourceVector->GetValue(row),
            fromPrecisionScale.first, fromPrecisionScale.second, toPrecisionScale.first, toPrecisionScale.second,
            rescaledValue);
        if (status.ok()) {
            castResultRawBuffer[row] = rescaledValue;
        } else {
            if (setNullInResultAtError()) {
                castResult->SetNull(row);
            } else {
                std::string errorMsg = "applyDecimalCastKernel Error";
                context.SetError(errorMsg);
            }
        }
    });
}

template <typename TInput, typename TOutput>
void CastExpr::applyIntToDecimalCastKernel(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &toType, VectorPtr &castResult)
{
    auto sourceVector = reinterpret_cast<Vector<TInput> *>(input);
    auto result = reinterpret_cast<Vector<TOutput> *>(castResult);
    auto castResultRawBuffer = reinterpret_cast<TOutput *>(VectorHelper::UnsafeGetValues(result));
    const auto &toPrecisionScale = GetDecimalPrecisionScale(*toType);
    applyToSelectedNoThrowLocal(context, rows, castResult, [&](vector_size_t row) {
        auto rescaledValue = DecimalOperations::rescaleInt<TInput, TOutput>(sourceVector->GetValue(row),
            toPrecisionScale.first, toPrecisionScale.second);
        if (rescaledValue.has_value()) {
            castResultRawBuffer[row] = rescaledValue.value();
        } else {
            castResult->SetNull(row);
        }
    });
}

template <typename FromNativeType>
VectorPtr CastExpr::applyDecimalToVarcharCast(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context, const DataTypePtr &fromType)
{
    VectorPtr result = VectorHelper::CreateFlatVector(context.GetPool(), OMNI_VARCHAR, context.GetResultRowSize());
    auto flatResult = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    const auto simpleInput = reinterpret_cast<Vector<FromNativeType> *>(input);
    int precision = GetDecimalPrecisionScale(*fromType).first;
    int scale = GetDecimalPrecisionScale(*fromType).second;
    auto rowSize = DecimalOperations::MaxStringViewSize(precision, scale);

    applyToSelectedNoThrowLocal(context, rows, result, [&](vector_size_t row) {
        if constexpr (std::is_same_v<FromNativeType, int64_t>) {
            std::string str = Decimal64<>(simpleInput->GetValue(row)).SetScale(scale).ToString();
            std::string_view tmp(str);
            flatResult->SetValue(row, tmp);
        } else {
            Decimal128 decimal = simpleInput->GetValue(row);
            std::string str = Decimal128Wrapper(decimal.HighBits(), decimal.LowBits()).SetScale(scale).ToString();
            std::string_view tmp(str);
            flatResult->SetValue(row, tmp);
        }
    });
    return result;
}

template <typename TInput, typename TOutput>
void CastExpr::applyFloatingPointToDecimalCastKernel(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context, const DataTypePtr &toType, VectorPtr &result)
{
    const auto floatingInput = reinterpret_cast<Vector<TInput> *>(input);
    auto rawResults = reinterpret_cast<TOutput *>(VectorHelper::UnsafeGetValues(result));
    const auto toPrecisionScale = GetDecimalPrecisionScale(*toType);

    applyToSelectedNoThrowLocal(context, rows, result, [&](vector_size_t row) {
        TOutput output;
        const auto status = DecimalOperations::rescaleFloatingPoint<TInput, TOutput>(floatingInput->GetValue(row),
            toPrecisionScale.first, toPrecisionScale.second, output);
        if (status.ok()) {
            rawResults[row] = output;
        } else {
            if (setNullInResultAtError()) {
                result->SetNull(row);
            } else {
                std::string errorMsg = "applyFloatingPointToDecimalCastKernel Error";
                context.SetError(errorMsg);
            }
        }
    });
}

template <typename T>
void CastExpr::applyVarcharToDecimalCastKernel(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context, const DataTypePtr &toType, VectorPtr &result)
{
    auto sourceVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(input);
    const auto floatingInput = reinterpret_cast<Vector<T> *>(result);
    auto rawResults = reinterpret_cast<T *>(VectorHelper::UnsafeGetValues(floatingInput));
    const auto toPrecisionScale = GetDecimalPrecisionScale(*toType);

    rows.applyToSelected([&](auto row) {
        T decimalValue;
        const auto status = detail::toDecimalValue<T>(hooks_->removeWhiteSpaces(sourceVector->GetValue(row)),
            toPrecisionScale.first, toPrecisionScale.second, decimalValue);
        if (status.ok()) {
            rawResults[row] = decimalValue;
        } else {
            if (setNullInResultAtError()) {
                result->SetNull(row);
            } else {
                std::string errorMsg = "applyVarcharToDecimalCastKernel Error";
                context.SetError(errorMsg);
            }
        }
    });
}

template <typename FromNativeType, DataTypeId ToKind>
VectorPtr CastExpr::applyDecimalToFloatCast(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType, const DataTypePtr &toType)
{
    using To = typename NativeType<ToKind>::type;

    VectorPtr result = VectorHelper::CreateFlatVector(context.GetPool(), ToKind, context.GetResultRowSize());
    auto resultBuffer = static_cast<To *>(VectorHelper::UnsafeGetValues(result));
    const auto precisionScale = GetDecimalPrecisionScale(*fromType);
    const auto simpleInput = reinterpret_cast<Vector<FromNativeType> *>(input);
    const auto scaleFactor = kPowersOfTen[precisionScale.second];
    applyToSelectedNoThrowLocal(context, rows, result, [&](int row) {
        const auto output = omniruntime::type::util::Converter<ToKind>::tryCast(simpleInput->GetValue(row)).thenOrThrow(
            folly::identity, [&](const Status &status) {
                OMNI_FAIL("{}", status.message());
            });
        resultBuffer[row] = output / scaleFactor;
    });
    return result;
}

template <typename FromNativeType, DataTypeId ToKind>
VectorPtr CastExpr::applyDecimalToIntegralCast(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context, const DataTypePtr &fromType, const DataTypePtr &toType)
{
    using To = typename NativeType<ToKind>::type;

    VectorPtr result = VectorHelper::CreateFlatVector(context.GetPool(), ToKind, context.GetResultRowSize());
    auto resultBuffer = static_cast<To *>(VectorHelper::UnsafeGetValues(result));
    const auto precisionScale = GetDecimalPrecisionScale(*fromType);
    const auto simpleInput = reinterpret_cast<Vector<FromNativeType> *>(input);
    const auto scaleFactor = kPowersOfTen[precisionScale.second];
    applyToSelectedNoThrowLocal(context, rows, result, [&](vector_size_t row) {
        resultBuffer[row] = static_cast<To>(simpleInput->GetValue(row) / scaleFactor);
    });
    return result;
}

template <typename FromNativeType>
VectorPtr CastExpr::applyDecimalToBooleanCast(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context)
{
    VectorPtr result = VectorHelper::CreateFlatVector(context.GetPool(), OMNI_BOOLEAN, context.GetResultRowSize());
    auto resultBuffer = static_cast<bool *>(VectorHelper::UnsafeGetValues(result));
    const auto simpleInput = reinterpret_cast<Vector<FromNativeType> *>(input);
    applyToSelectedNoThrowLocal(context, rows, result, [&](int row) {
        auto value = simpleInput->GetValue(row);
        resultBuffer[row] = value != 0;
    });
    return result;
}

template <typename FromNativeType>
VectorPtr CastExpr::applyDecimalToPrimitiveCast(const SelectivityVector &rows, BaseVector *input,
    ExecutionContext &context, const DataTypePtr &fromType, const DataTypePtr &toType)
{
    switch (toType->GetId()) {
        case OMNI_BOOLEAN:
            return applyDecimalToBooleanCast<FromNativeType>(rows, input, context);
        case OMNI_BYTE:
            return applyDecimalToIntegralCast<FromNativeType, OMNI_BOOLEAN>(rows, input, context, fromType, toType);
        case OMNI_SHORT:
            return applyDecimalToIntegralCast<FromNativeType, OMNI_SHORT>(rows, input, context, fromType, toType);
        case OMNI_INT:
            return applyDecimalToIntegralCast<FromNativeType, OMNI_INT>(rows, input, context, fromType, toType);
        case OMNI_LONG:
            return applyDecimalToIntegralCast<FromNativeType, OMNI_LONG>(rows, input, context, fromType, toType);
        case OMNI_FLOAT:
            return applyDecimalToFloatCast<FromNativeType, OMNI_FLOAT>(rows, input, context, fromType, toType);
        case OMNI_DOUBLE:
            return applyDecimalToFloatCast<FromNativeType, OMNI_DOUBLE>(rows, input, context, fromType, toType);
        default: OMNI_FAIL("Cast from {} to {} is not supported", fromType->toString(), toType->toString());
    }
}

template <DataTypeId ToKind>
void CastExpr::applyCastPrimitivesDispatch(const DataTypePtr &fromType, const DataTypePtr &toType,
    const SelectivityVector &rows, ExecutionContext &context, BaseVector *input, VectorPtr &result)
{
    result = VectorHelper::CreateFlatVector(context.GetPool(), toType->GetId(), context.GetResultRowSize());
    // This already excludes complex types, hugeInt and unknown from type kinds.
    switch (fromType->GetId()) {
        case OMNI_BOOLEAN: {
            return applyCastPrimitives<ToKind, OMNI_BOOLEAN>(rows, context, input, result);
        }
        case OMNI_INT: {
            return applyCastPrimitives<ToKind, OMNI_INT>(rows, context, input, result);
        }
        case OMNI_BYTE: {
            return applyCastPrimitives<ToKind, OMNI_BYTE>(rows, context, input, result);
        }
        case OMNI_SHORT: {
            return applyCastPrimitives<ToKind, OMNI_SHORT>(rows, context, input, result);
        }
        case OMNI_LONG: {
            return applyCastPrimitives<ToKind, OMNI_LONG>(rows, context, input, result);
        }
        case OMNI_FLOAT: {
            return applyCastPrimitives<ToKind, OMNI_FLOAT>(rows, context, input, result);
        }
        case OMNI_DOUBLE: {
            return applyCastPrimitives<ToKind, OMNI_DOUBLE>(rows, context, input, result);
        }
        case OMNI_VARCHAR: {
            return applyCastPrimitives<ToKind, OMNI_VARCHAR>(rows, context, input, result);
        }
        case OMNI_VARBINARY: {
            return applyCastPrimitives<ToKind, OMNI_VARBINARY>(rows, context, input, result);
        }
        case OMNI_TIMESTAMP: {
            return applyCastPrimitives<ToKind, OMNI_TIMESTAMP>(rows, context, input, result);
        }
        default: {
            OMNI_FAIL("Unsupported cast type!");
        }
    }
}
}
