/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Time function implementation
 *
 * TIME(string VARCHAR) -> TIME64
 *
 * Parses a string in HH:mm:ss format and returns SQL time as microseconds since midnight.
 * Supports optional fractional seconds (e.g., HH:mm:ss.fff).
 * Returns NULL for invalid input or NULL input.
 */

#include "Time.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include <cstring>
#include <string>
#include <string_view>
#include <cctype>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

/// Microsecond constants
static constexpr int64_t kMicrosPerSecond = 1000000LL;
static constexpr int64_t kMicrosPerMinute = 60LL * kMicrosPerSecond;
static constexpr int64_t kMicrosPerHour = 60LL * kMicrosPerMinute;

/// Parse a non-negative integer from string, advancing the position.
/// Returns false if no digits found or value exceeds maxVal.
static bool ParseInt(const char* str, size_t len, size_t& pos, int32_t maxDigits, int32_t& result)
{
    if (pos >= len || !std::isdigit(static_cast<unsigned char>(str[pos]))) {
        return false;
    }
    result = 0;
    int32_t digits = 0;
    while (pos < len && std::isdigit(static_cast<unsigned char>(str[pos])) && digits < maxDigits) {
        result = result * 10 + (str[pos] - '0');
        ++pos;
        ++digits;
    }
    return true;
}

/// Parse time string in HH:mm:ss format with optional fractional seconds.
/// Returns microseconds since midnight, or -1 on failure.
static int64_t ParseTimeString(const char* str, size_t len)
{
    if (len == 0) {
        return -1;
    }

    size_t pos = 0;

    // Parse hours (1-2 digits, 0-23)
    int32_t hour = 0;
    if (!ParseInt(str, len, pos, 2, hour)) {
        return -1;
    }
    if (hour > 23) {
        return -1;
    }

    // Expect ':'
    if (pos >= len || str[pos] != ':') {
        return -1;
    }
    ++pos;

    // Parse minutes (1-2 digits, 0-59)
    int32_t minute = 0;
    if (!ParseInt(str, len, pos, 2, minute)) {
        return -1;
    }
    if (minute > 59) {
        return -1;
    }

    // Expect ':'
    if (pos >= len || str[pos] != ':') {
        return -1;
    }
    ++pos;

    // Parse seconds (1-2 digits, 0-59)
    int32_t second = 0;
    if (!ParseInt(str, len, pos, 2, second)) {
        return -1;
    }
    if (second > 59) {
        return -1;
    }

    // Parse optional fractional seconds
    int32_t micros = 0;
    if (pos < len && str[pos] == '.') {
        ++pos;
        // Parse up to 6 digits for microseconds
        int32_t fracDigits = 0;
        int32_t fracValue = 0;
        while (pos < len && std::isdigit(static_cast<unsigned char>(str[pos])) && fracDigits < 6) {
            fracValue = fracValue * 10 + (str[pos] - '0');
            ++pos;
            ++fracDigits;
        }
        // Pad to 6 digits (microseconds)
        for (int32_t i = fracDigits; i < 6; ++i) {
            fracValue *= 10;
        }
        micros = fracValue;

        // Skip any remaining fractional digits beyond 6
        while (pos < len && std::isdigit(static_cast<unsigned char>(str[pos]))) {
            ++pos;
        }
    }

    // Check that we consumed the entire string
    if (pos != len) {
        return -1;
    }

    // Calculate microseconds since midnight
    return static_cast<int64_t>(hour) * kMicrosPerHour +
           static_cast<int64_t>(minute) * kMicrosPerMinute +
           static_cast<int64_t>(second) * kMicrosPerSecond +
           micros;
}

/// Helper: extract string value from vector with different encodings
static std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    }
    return std::string_view();
}

class TimeFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        if (args.size() < 1) {
            return;
        }

        // Extract the input argument
        const auto inputArg = args.top();
        args.pop();

        const auto size = inputArg->GetSize();

        // Create result vector if it doesn't exist
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(outputType->GetId(), size);
        }

        auto *resultVector = reinterpret_cast<Vector<int64_t> *>(result);
        auto *resultRaw = unsafe::UnsafeVector::GetRawValues(resultVector);
        auto *resultNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));

        // Get input nulls
        const auto *inputNulls = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(inputArg));

        // Copy NULL bits from input to result
        auto nullsSize = BitUtil::Nbytes(size);
        memcpy(resultNulls, inputNulls, nullsSize);

        // Process only non-NULL rows
        SelectivityVector rows(size);
        rows.setFromBitsNegate(inputNulls, size);

        rows.applyToSelected([&](vector_size_t i) {
            std::string_view inputStr = GetStringValueFromVector(inputArg, i);

            int64_t timeMicros = ParseTimeString(inputStr.data(), inputStr.size());
            if (timeMicros >= 0) {
                resultRaw[i] = timeMicros;
                result->SetNotNull(i);
            } else {
                result->SetNull(i);
            }
        });

        // Clean up
        delete inputArg;
    }
};

} // namespace

void RegisterTimeFunction(const std::string &name)
{
    // time(string VARCHAR) -> BIGINT (microseconds since midnight)
    VectorFunction::RegisterVectorFunction(name,
        {OMNI_VARCHAR}, OMNI_LONG,
        std::make_shared<TimeFunction>());
}

} // namespace omniruntime::vectorization
