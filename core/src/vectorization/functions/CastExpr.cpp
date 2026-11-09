/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "CastExpr.h"

#include <algorithm>
#include "util/omni_exception.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;

namespace {
const tz::TimeZone *getTimeZoneFromConfig(const config::QueryConfig &config)
{
    if (config.AdjustTimestampToTimezone()) {
        const auto sessionTzName = config.SessionTimezone();
        if (!sessionTzName.empty()) {
            return tz::locateZone(sessionTzName);
        }
    }
    return nullptr;
}
}

std::string_view detail::extractDigits(const char *s, size_t start, size_t size)
{
    size_t pos = start;
    for (; pos < size; ++pos) {
        if (!std::isdigit(s[pos])) {
            break;
        }
    }
    return std::string_view(s + start, pos - start);
}

Status detail::parseDecimalComponents(const char *s, size_t size, detail::DecimalComponents &out)
{
    if (size == 0) {
        return Status::UserError("Input is empty.");
    }

    size_t pos = 0;

    // Sign of the number.
    if (s[pos] == '-') {
        out.sign = -1;
        ++pos;
    } else if (s[pos] == '+') {
        out.sign = 1;
        ++pos;
    }

    // Extract the whole digits.
    out.wholeDigits = detail::extractDigits(s, pos, size);
    pos += out.wholeDigits.size();
    if (pos == size) {
        return out.wholeDigits.empty() ? Status::UserError("Extracted digits are empty.") : Status::OK();
    }

    // Optional dot (if given in fractional form).
    if (s[pos] == '.') {
        // Extract the fractional digits.
        ++pos;
        out.fractionalDigits = detail::extractDigits(s, pos, size);
        pos += out.fractionalDigits.size();
    }

    if (out.wholeDigits.empty() && out.fractionalDigits.empty()) {
        return Status::UserError("Extracted digits are empty.");
    }
    if (pos == size) {
        return Status::OK();
    }
    // Optional exponent.
    if (s[pos] == 'e' || s[pos] == 'E') {
        ++pos;
        bool withSign = pos < size && (s[pos] == '+' || s[pos] == '-');
        if (withSign && pos == size - 1) {
            return Status::UserError("The exponent part only contains sign.");
        }
        // Make sure all chars after sign are digits, as as folly::tryTo allows
        // leading and trailing whitespaces.
        for (auto i = static_cast<size_t>(withSign); i < size - pos; ++i) {
            if (!std::isdigit(s[pos + i])) {
                return Status::UserError("Non-digit character '{}' is not allowed in the exponent part.", s[pos + i]);
            }
        }
        out.exponent = folly::to<int32_t>(folly::StringPiece(s + pos, size - pos));
        return Status::OK();
    }
    return pos == size ? Status::OK() : Status::UserError("Chars '{}' are invalid.", std::string(s + pos, size - pos));
}

Status detail::parseHugeInt(const DecimalComponents &decimalComponents, int128_t &out)
{
    // Parse the whole digits.
    if (decimalComponents.wholeDigits.size() > 0) {
        const auto tryValue = folly::tryTo<int128_t>(folly::StringPiece(decimalComponents.wholeDigits.data(),
            decimalComponents.wholeDigits.size()));
        if (tryValue.hasError()) {
            return Status::UserError("Value too large.");
        }
        out = tryValue.value();
    }

    // Parse the fractional digits.
    if (decimalComponents.fractionalDigits.size() > 0) {
        const auto length = decimalComponents.fractionalDigits.size();
        bool overflow = __builtin_mul_overflow(out, TenOfScaleMultipliers[length], &out);
        if (overflow) {
            return Status::UserError("Value too large.");
        }
        const auto tryValue = folly::tryTo<int128_t>(folly::StringPiece(decimalComponents.fractionalDigits.data(),
            length));
        if (tryValue.hasError()) {
            return Status::UserError("Value too large.");
        }
        overflow = __builtin_add_overflow(out, tryValue.value(), &out);
    }
    return Status::OK();
}

void CastExpr::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    auto input = args.top();
    args.pop();
    const auto size = input->GetSize();
    SelectivityVector row(size);
    const_cast<CastExpr *>(this)->apply(row, input, *context, fromType_, toType_, result);
    delete input;
}

void CastExpr::apply(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType, const DataTypePtr &toType, VectorPtr &result)
{
    hooks_ = std::make_shared<CastHooks>(context.queryConfig());
    const auto size = input->GetSize();
    const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(input));
    SelectivityVector newRows(size);
    newRows.setFromBitsNegate(nullBits, size);

    VectorPtr localResult = nullptr;
    if (!newRows.hasSelections()) {
        localResult = VectorHelper::CreateFlatVector(toType->GetId(), context.GetResultRowSize());
        localResult->GetNullsBuffer()->SetNulls(0, true, context.GetResultRowSize());
    } else if (input->GetEncoding() == OMNI_FLAT) {
        applyPeeled(newRows, input, context, fromType, toType, localResult);
    } else {
        OMNI_THROW("Runtime error:", "Only support flat vector!");
    }
    // If there are nulls or rows that encountered errors in the input, add nulls
    // to the result at the same rows.
    result = localResult;
    OMNI_CHECK(result!=nullptr, "result can not be null!");
    // Preserve input nulls in cast result. The cast kernels only process selected
    // non-null rows, so we must explicitly mark input-null rows as null.
    for (int32_t row = 0; row < size; ++row) {
        if (input->IsNull(row)) {
            result->SetNull(row);
        }
    }
}

VectorPtr CastExpr::applyMap(const SelectivityVector &rows, const MapVector *input, ExecutionContext &context,
    const MapType &fromType, const MapType &toType)
{
    OMNI_FAIL("Not implemented yet!");
}

VectorPtr CastExpr::applyArray(const SelectivityVector &rows, const ArrayVector *input, ExecutionContext &context,
    const ArrayType &fromType, const ArrayType &toType)
{
    auto fromElementType = fromType.ElementType();
    auto toElementType = toType.ElementType();

    auto inputElementVector = input->GetElementVector();
    if (inputElementVector == nullptr) {
        auto *resultArray = new ArrayVector(input->GetSize());
        for (int32_t row = 0; row < input->GetSize(); ++row) {
            if (input->IsNull(row)) {
                resultArray->SetNull(row);
            } else {
                resultArray->SetOffset(row, 0);
                resultArray->SetSize(row, 0);
            }
        }
        return VectorPtr(resultArray);
    }

    int32_t elementCount = inputElementVector->GetSize();

    int32_t savedRowSize = context.GetResultRowSize();
    context.SetResultRowSize(elementCount);

    SelectivityVector elementRows(elementCount);
    VectorPtr castedElements = nullptr;
    apply(elementRows, inputElementVector.get(), context, fromElementType, toElementType, castedElements);

    context.SetResultRowSize(savedRowSize);

    auto *resultArray = new ArrayVector(input->GetSize(), std::shared_ptr<BaseVector>(castedElements));
    for (int32_t row = 0; row <= input->GetSize(); ++row) {
        resultArray->SetOffset(row, const_cast<ArrayVector *>(input)->GetOffset(row));
    }
    for (int32_t row = 0; row < input->GetSize(); ++row) {
        if (input->IsNull(row)) {
            resultArray->SetNull(row);
        }
    }

    return VectorPtr(resultArray);
}

VectorPtr CastExpr::applyRow(const SelectivityVector &rows, const RowVector *input, ExecutionContext &context,
    const RowType &fromType, const DataTypePtr &toType)
{
    OMNI_FAIL("Not implemented yet!");
}

template <typename toDecimalType>
VectorPtr CastExpr::applyDecimal(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType, const DataTypePtr &toType)
{
    VectorPtr castResult = VectorHelper::CreateFlatVector(toType->GetId(), context.GetResultRowSize());
    // toType is a decimal
    switch (fromType->GetId()) {
        case OMNI_BOOLEAN:
            applyIntToDecimalCastKernel<bool, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_BYTE:
            applyIntToDecimalCastKernel<int8_t, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_SHORT:
            applyIntToDecimalCastKernel<int16_t, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_INT:
            applyIntToDecimalCastKernel<int32_t, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_FLOAT:
            applyFloatingPointToDecimalCastKernel<float, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_DOUBLE:
            applyFloatingPointToDecimalCastKernel<double, toDecimalType>(rows, input, context, toType, castResult);
            break;
        case OMNI_LONG: {
            applyIntToDecimalCastKernel<int64_t, toDecimalType>(rows, input, context, toType, castResult);
            break;
        }
        case OMNI_DECIMAL64: {
            applyDecimalCastKernel<int64_t, toDecimalType>(rows, input, context, fromType, toType, castResult);
            break;
        }
        case OMNI_DECIMAL128: {
            applyDecimalCastKernel<int128_t, toDecimalType>(rows, input, context, fromType, toType, castResult);
            break;
        }
        case OMNI_VARCHAR:
            applyVarcharToDecimalCastKernel<toDecimalType>(rows, input, context, toType, castResult);
            break;
        default: {
            OMNI_FAIL("Cast from {} to {} is not supported", fromType->toString(), toType->toString());
        }
    }
    return castResult;
}

CastOperatorPtr CastExpr::getCastOperator(const DataTypePtr &type)
{
    return nullptr;
}

template <typename TInput>
VectorPtr CastExpr::applyIntToBinaryCast(const SelectivityVector &rows, ExecutionContext &context, BaseVector *input)
{
    auto result = VectorHelper::CreateFlatVector(OMNI_VARBINARY, context.GetResultRowSize());
    const auto flatResult = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    const auto simpleInput = reinterpret_cast<const Vector<TInput> *>(input);

    // The created string view is always inlined for int types.
    char inlined[sizeof(TInput)];
    applyToSelectedNoThrowLocal(context, rows, result, [&](vector_size_t row) {
        TInput input = simpleInput->GetValue(row);
        if constexpr (std::is_same_v<TInput, int8_t>) {
            inlined[0] = static_cast<char>(input & 0xFF);
        } else {
            for (int i = sizeof(TInput) - 1; i >= 0; --i) {
                inlined[i] = static_cast<char>(input & 0xFF);
                input >>= 8;
            }
        }
        auto stringView = std::string_view(inlined, sizeof(TInput));
        flatResult->SetValue(row, stringView);
    });

    return result;
}

void CastExpr::applyPeeled(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType, const DataTypePtr &toType, VectorPtr &result)
{
    auto castFromOperator = getCastOperator(fromType);
    if (castFromOperator && !castFromOperator->isSupportedToType(toType)) {
        OMNI_FAIL("Cannot cast {} to {}.", fromType->toString(), toType->toString());
    }

    auto castToOperator = getCastOperator(toType);
    if (castToOperator && !castToOperator->isSupportedFromType(fromType)) {
        OMNI_FAIL("Cannot cast {} to {}.", fromType->toString(), toType->toString());
    }

    if (castFromOperator || castToOperator) {
        OMNI_FAIL("not support castFromOperator and castToOperator!");
    } else if (fromType->GetId() == OMNI_DATE32) {
        result = castFromDate(rows, input, context, toType);
    } else if (toType->GetId() == OMNI_DATE32) {
        result = castToDate(rows, input, context, fromType);
    } else if (fromType->GetId() == OMNI_DATE64) {
        result = castFromIntervalDayTime(rows, input, context, toType);
    } else if (toType->GetId() == OMNI_DATE64) {
        OMNI_FAIL("Cast from {} to {} is not supported", fromType->toString(), toType->toString());
    } else if (toType->GetId() == OMNI_DECIMAL64) {
        result = applyDecimal<int64_t>(rows, input, context, fromType, toType);
    } else if (toType->GetId() == OMNI_DECIMAL128) {
        result = applyDecimal<int128_t>(rows, input, context, fromType, toType);
    } else if (fromType->isDecimal()) {
        switch (toType->GetId()) {
            case OMNI_VARCHAR:
                if (fromType->GetId() == OMNI_DECIMAL128) {
                    result = applyDecimalToVarcharCast<Decimal128>(rows, input, context, fromType);
                } else {
                    result = applyDecimalToVarcharCast<int64_t>(rows, input, context, fromType);
                }
                break;
            default:
                if (fromType->GetId() == OMNI_DECIMAL128) {
                    result = applyDecimalToPrimitiveCast<int128_t>(rows, input, context, fromType, toType);
                } else {
                    result = applyDecimalToPrimitiveCast<int64_t>(rows, input, context, fromType, toType);
                }
        }
    } else if (fromType->GetId() == OMNI_TIMESTAMP && (toType->GetId() == OMNI_VARCHAR || toType->GetId() ==
        OMNI_VARBINARY)) {
        result = applyTimestampToVarcharCast(toType, rows, context, input);
    } else if (toType->GetId() == OMNI_VARBINARY) {
        switch (fromType->GetId()) {
            case OMNI_BYTE:
                result = applyIntToBinaryCast<int8_t>(rows, context, input);
                break;
            case OMNI_SHORT:
                result = applyIntToBinaryCast<int16_t>(rows, context, input);
                break;
            case OMNI_INT:
                result = applyIntToBinaryCast<int32_t>(rows, context, input);
                break;
            case OMNI_LONG:
                result = applyIntToBinaryCast<int64_t>(rows, context, input);
                break;
            default:
                // Handle primitive type conversions.
                applyCastPrimitivesDispatch<OMNI_VARCHAR>(fromType, toType, rows, context, input, result);
                break;
        }
    } else {
        switch (toType->GetId()) {
            case OMNI_MAP: {
                auto mapVector = reinterpret_cast<const MapVector *>(input);
                result = applyMap(rows, mapVector, context, fromType->asMap(), toType->asMap());
                break;
            }
            case OMNI_ARRAY: {
                auto arrayVector = reinterpret_cast<const ArrayVector *>(input);
                result = applyArray(rows, arrayVector, context, fromType->asArray(), toType->asArray());
                break;
            }
            case OMNI_ROW: {
                auto rowVector = reinterpret_cast<const RowVector *>(input);
                result = applyRow(rows, rowVector, context, fromType->asRow(), toType);
                break;
            }
            default: {
                // Handle primitive type conversions.
                switch (toType->GetId()) {
                    case OMNI_BOOLEAN: {
                        return applyCastPrimitivesDispatch<
                            OMNI_BOOLEAN>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_INT: {
                        return applyCastPrimitivesDispatch<OMNI_INT>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_BYTE: {
                        return applyCastPrimitivesDispatch<OMNI_BYTE>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_SHORT: {
                        return applyCastPrimitivesDispatch<OMNI_SHORT>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_LONG: {
                        return applyCastPrimitivesDispatch<OMNI_LONG>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_FLOAT: {
                        return applyCastPrimitivesDispatch<OMNI_FLOAT>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_DOUBLE: {
                        return applyCastPrimitivesDispatch<OMNI_DOUBLE>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_VARCHAR: {
                        return applyCastPrimitivesDispatch<
                            OMNI_VARCHAR>(fromType, toType, rows, context, input, result);
                    }
                    case OMNI_VARBINARY: {
                        return applyCastPrimitivesDispatch<OMNI_VARBINARY>(fromType, toType, rows, context, input,
                            result);
                    }
                    case OMNI_TIMESTAMP: {
                        return applyCastPrimitivesDispatch<OMNI_TIMESTAMP>(fromType, toType, rows, context, input,
                            result);
                    }
                    default: {
                        OMNI_FAIL("not a scalar type! kind");
                    }
                }
            }
        }
    }
}

VectorPtr CastExpr::castFromDate(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &toType)
{
    auto *castResult = VectorHelper::CreateFlatVector(toType->GetId(), rows.size());
    auto *inputFlatVector = reinterpret_cast<const Vector<int32_t> *>(input);

    switch (toType->GetId()) {
        case OMNI_VARCHAR: {
            auto *resultFlatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(castResult);
            applyToSelectedNoThrowLocal(context, rows, castResult, [&](int row) {
                try {
                    // TODO Optimize to avoid creating an intermediate string.
                    auto output = omniruntime::type::util::ToIso8601(inputFlatVector->GetValue(row));
                    std::string_view tmp(output);
                    resultFlatVector->SetValue(row, tmp);
                } catch (const OmniException &ue) {
                    if (!ue.isUserError()) {
                        throw;
                    }
                    OMNI_USER_FAIL(detail::makeErrorMessage(input, row, toType) + " " + ue.what());
                } catch (const std::exception &e) {
                    OMNI_USER_FAIL(detail::makeErrorMessage(input, row, toType) + " " + e.what());
                }
            });
            return castResult;
        }
        case OMNI_TIMESTAMP: {
            static const int64_t kMillisPerDay{86'400'000};
            const auto *timeZone = getTimeZoneFromConfig(context.queryConfig());
            auto *resultFlatVector = reinterpret_cast<Vector<int64_t> *>(castResult);
            applyToSelectedNoThrowLocal(context, rows, castResult, [&](int row) {
                auto timestamp = Timestamp::fromMillis(inputFlatVector->GetValue(row) * kMillisPerDay);
                if (timeZone) {
                    timestamp.toGMT(*timeZone);
                }
                resultFlatVector->SetValue(row, timestamp.toMicros());
            });

            return castResult;
        }
        default: OMNI_FAIL("Cast from DATE to {} is not supported", toType->toString());
    }
}

VectorPtr CastExpr::castToDate(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &fromType)
{
    VectorPtr castResult = VectorHelper::CreateFlatVector(OMNI_DATE32, context.GetResultRowSize());
    auto *resultFlatVector = reinterpret_cast<Vector<int32_t> *>(castResult);
    switch (fromType->GetId()) {
        case OMNI_VARCHAR: {
            auto *inputVector = reinterpret_cast<const Vector<LargeStringContainer<std::string_view>> *>(input);
            applyToSelectedNoThrowLocal(context, rows, castResult, [&](int row) {
                bool wrapException = true;
                try {
                    const auto result = hooks_->castStringToDate(inputVector->GetValue(row));
                    if (result.hasError()) {
                        wrapException = false;
                        if (setNullInResultAtError()) {
                            resultFlatVector->SetNull(row);
                        } else {
                            auto errorMsg = detail::makeErrorMessage(input, row, std::make_shared<Date32DataType>());
                            context.SetError(errorMsg);
                        }
                    } else {
                        resultFlatVector->SetValue(row, result.value());
                    }
                } catch (const OmniException &ue) {
                    if (!wrapException) {
                        throw;
                    }
                    OMNI_USER_FAIL(
                        detail::makeErrorMessage(input, row, std::make_shared<Date32DataType>()) + " " + ue.what());
                } catch (const std::exception &e) {
                    OMNI_USER_FAIL(
                        detail::makeErrorMessage(input, row, std::make_shared<Date32DataType>()) + " " + e.what());
                }
            });

            return castResult;
        }
        case OMNI_TIMESTAMP: {
            auto *inputVector = reinterpret_cast<const Vector<int64_t> *>(input);
            const auto *timeZone = getTimeZoneFromConfig(context.queryConfig());
            applyToSelectedNoThrowLocal(context, rows, castResult, [&](int row) {
                const auto days = omniruntime::type::util::toDate(Timestamp::fromMicros(inputVector->GetValue(row)),
                    timeZone);
                resultFlatVector->SetValue(row, days);
            });
            return castResult;
        }
        default: OMNI_FAIL("Cast from {} to DATE is not supported", fromType->toString());
    }
}

VectorPtr CastExpr::castFromIntervalDayTime(const SelectivityVector &rows, BaseVector *input, ExecutionContext &context,
    const DataTypePtr &toType)
{
    VectorPtr castResult = VectorHelper::CreateFlatVector(toType->GetId(), context.GetResultRowSize());

    auto *inputFlatVector = reinterpret_cast<const Vector<int64_t> *>(input);
    switch (toType->GetId()) {
        case OMNI_VARCHAR: {
            auto *resultFlatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(castResult);
            applyToSelectedNoThrowLocal(context, rows, castResult, [&](int row) {
                try {
                    // TODO Optimize to avoid creating an intermediate string.
                    auto output = omniruntime::type::util::valueToString(inputFlatVector->GetValue(row));
                    std::string_view tmp = output;
                    resultFlatVector->SetValue(row, tmp);
                } catch (const OmniException &ue) {
                    if (!ue.isUserError()) {
                        throw;
                    }
                    OMNI_USER_FAIL(detail::makeErrorMessage(input, row, toType) + " " + ue.what());
                } catch (const std::exception &e) {
                    OMNI_USER_FAIL(detail::makeErrorMessage(input, row, toType) + " " + e.what());
                }
            });
            return castResult;
        }
        default: OMNI_FAIL("Cast from {} to {} is not supported", "INTERVAL_DAY_TIME", toType->toString());
    }
}

VectorPtr CastExpr::applyTimestampToVarcharCast(const DataTypePtr &toType, const SelectivityVector &rows,
    ExecutionContext &context, BaseVector *input)
{
    VectorPtr result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, context.GetResultRowSize());
    auto flatResult = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    auto simpleInput = reinterpret_cast<Vector<int64_t> *>(input);

    const auto &options = hooks_->timestampToStringOptions();
    const auto bufferSize = std::max<size_t>(getMaxStringLength(options) + 1, 64);
    std::vector<char> buffer(bufferSize);

    applyToSelectedNoThrowLocal(context, rows, result, [&](vector_size_t row) {
        // Adjust input timestamp according the session timezone.
        Timestamp inputValue = Timestamp::fromMicros(simpleInput->GetValue(row));
        if (options.timeZone) {
            inputValue.toTimezone(*(options.timeZone));
        }
        auto stringView = Timestamp::tsToStringView(inputValue, options, buffer.data());
        flatResult->SetValue(row, stringView);
    });

    return result;
}
}
