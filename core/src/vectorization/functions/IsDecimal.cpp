/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS_DECIMAL function implementation.
 *   IS_DECIMAL(string) -> boolean.
 *   Returns true if string can be parsed to a valid numeric, otherwise false.
 *   NULL input returns false (output is NOT null); empty string -> false.
 *   Aligned with Flink SqlFunctionUtils.isDecimal, which reduces to whether the
 *   string is accepted by Java Double.parseDouble (covers Integer/Long/Double).
 *
 * Uses Path B (VectorFunction) so NULL input rows can be mapped to a non-NULL
 * `false` output (Flink semantics), which the SimpleFunction default null
 * propagation (NULL in -> NULL out) cannot express.
 *
 * The parser is hand-written to match Java Double.parseDouble's accepted
 * grammar (NOT std::stod, which differs on NaN/Infinity case sensitivity,
 * "5." handling, and hex-float support). Java's `d/D/f/F` type suffixes and
 * hexadecimal floating literals (0x1p3) are intentionally NOT accepted; those
 * forms are effectively never present in SQL string data.
 */

#include "vectorization/functions/IsDecimal.h"
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"

#include <cctype>
#include <stack>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

namespace {

/// Returns true iff c is an ASCII whitespace char trimmed by Java Double.parseDouble.
ALWAYS_INLINE bool IsJavaAsciiWhitespace(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/// Case-insensitive ASCII compare of s against a lowercase literal.
ALWAYS_INLINE bool EqualsIgnoreCaseAscii(std::string_view s, const char *lowerLit)
{
    size_t i = 0;
    for (; i < s.size(); ++i) {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        if (lowerLit[i] == '\0' || c != lowerLit[i]) {
            return false;
        }
    }
    return lowerLit[i] == '\0';
}

/// Returns true iff s (already trimmed) is a Java Double.parseDouble special value:
///   NaN | Infinity | +Infinity | -Infinity  (case-insensitive)
bool IsSpecialValue(std::string_view s)
{
    if (EqualsIgnoreCaseAscii(s, "nan")) {
        return true;
    }
    if (EqualsIgnoreCaseAscii(s, "infinity")) {
        return true;
    }
    if (s.size() >= 1 && (s[0] == '+' || s[0] == '-')) {
        if (EqualsIgnoreCaseAscii(s.substr(1), "infinity")) {
            return true;
        }
    }
    return false;
}

/// Returns true iff s (already trimmed) matches the Java Double.parseDouble decimal grammar:
///   [+-]? ( Digits [. [Digits]] | . Digits ) [ (e|E) [+-]? Digits ]
/// At least one digit must appear in the significand and the whole string must be consumed.
bool IsNumericLiteral(std::string_view s)
{
    size_t i = 0;
    size_t len = s.size();

    // optional sign
    if (i < len && (s[i] == '+' || s[i] == '-')) {
        ++i;
    }

    bool seenDigitsBeforeDot = false;
    bool seenDot = false;
    bool seenDigitsAfterDot = false;

    while (i < len && s[i] >= '0' && s[i] <= '9') {
        seenDigitsBeforeDot = true;
        ++i;
    }

    if (i < len && s[i] == '.') {
        seenDot = true;
        ++i;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            seenDigitsAfterDot = true;
            ++i;
        }
    }

    // At least one digit must be present in the significand
    // (Java rejects ".", "+.", "e10", etc.).
    if (!(seenDigitsBeforeDot || seenDigitsAfterDot)) {
        return false;
    }
    if (!seenDot && !seenDigitsBeforeDot) {
        return false;
    }

    // optional exponent: (e|E) [+-]? Digits
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        bool seenExpDigits = false;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            seenExpDigits = true;
            ++i;
        }
        if (!seenExpDigits) {
            return false;
        }
    }

    return i == len;  // must consume the whole string
}

/// Returns true iff s is parseable by Java Double.parseDouble.
bool IsParseableAsDouble(std::string_view s)
{
    size_t start = 0;
    size_t end = s.size();
    while (start < end && IsJavaAsciiWhitespace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && IsJavaAsciiWhitespace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    if (start == end) {
        return false;  // empty or all-whitespace
    }
    std::string_view trimmed = s.substr(start, end - start);
    if (IsSpecialValue(trimmed)) {
        return true;
    }
    return IsNumericLiteral(trimmed);
}

/// Returns true iff typeId is a numeric type accepted by Flink's IS_DECIMAL
/// (OperandTypes.NUMERIC). For non-null numeric input Flink's isDecimal returns true.
ALWAYS_INLINE bool IsNumericType(DataTypeId typeId)
{
    return typeId == OMNI_BYTE || typeId == OMNI_SHORT || typeId == OMNI_INT ||
        typeId == OMNI_LONG || typeId == OMNI_FLOAT || typeId == OMNI_DOUBLE ||
        typeId == OMNI_DECIMAL64 || typeId == OMNI_DECIMAL128;
}

class IsDecimalFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, ExecutionContext *context) const override
    {
        if (args.empty()) {
            OMNI_THROW("IS_DECIMAL Error", "IS_DECIMAL requires 1 argument; got 0");
        }
        BaseVector *inputVec = args.top();
        args.pop();

        int32_t rowSize = inputVec->GetSize();
        result = VectorHelper::CreateFlatVector(outputType->GetId(), rowSize);
        auto *resultVec = static_cast<Vector<bool> *>(result);

        // Flink semantics: numeric input -> true (isDecimal: instanceof numeric),
        // except NULL input -> false. String input parsed as Java Double. Output never NULL.
        const bool numericInput = IsNumericType(inputVec->GetTypeId());

        for (int32_t row = 0; row < rowSize; ++row) {
            bool value = false;
            if (numericInput) {
                value = !inputVec->IsNull(row);
            } else if (!inputVec->IsNull(row)) {
                std::string_view s = GetStringValueFromVector(inputVec, row);
                // Flink semantics: empty string -> false
                value = !s.empty() && IsParseableAsDouble(s);
            }
            resultVec->SetValue(row, value);
            resultVec->SetNotNull(row);
        }

        delete inputVec;
    }

private:
    /// Get string value from a vector supporting flat / const / dictionary encodings.
    static std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
    {
        Encoding encoding = vec->GetEncoding();
        if (encoding == OMNI_ENCODING_CONST) {
            auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
            return constVec->GetConstValue();
        }
        if (encoding == OMNI_FLAT) {
            auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
            return flatVec->GetValue(row);
        }
        if (encoding == OMNI_DICTIONARY) {
            auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
            return dictVec->GetValue(row);
        }
        OMNI_THROW("IS_DECIMAL Error", "Unsupported encoding type for string input");
    }
};

}  // namespace

void RegisterIsDecimalFunction(const std::string &name)
{
    auto isDecimalFunction = std::make_shared<IsDecimalFunction>();
    // String input types (CHAR/VARCHAR): parse as numeric per Java Double.parseDouble grammar.
    VectorFunction::RegisterVectorFunction(name, {OMNI_VARCHAR}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_CHAR}, OMNI_BOOLEAN, isDecimalFunction);
    // Numeric input types (Flink OperandTypes.NUMERIC): non-null -> true.
    VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_FLOAT}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_DOUBLE}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL64}, OMNI_BOOLEAN, isDecimalFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_DECIMAL128}, OMNI_BOOLEAN, isDecimalFunction);
}

}  // namespace omniruntime::vectorization
