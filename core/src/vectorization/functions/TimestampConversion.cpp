/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Timestamp conversion functions implementation for vectorized execution
 */

#include "TimestampConversion.h"
#include "vector/vector.h"
#include <cmath>
#include <limits>
#include <memory>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

    // Helper functions - must be defined before template functions use them
    template<typename T>
    T GetValueFromVectorHelper(BaseVector *vec, int32_t row) {
        Encoding encoding = vec->GetEncoding();

        if (encoding == OMNI_ENCODING_CONST) {
            auto *constVec = static_cast<ConstVector<T> *>(vec);
            return constVec->GetConstValue();
        } else if (encoding == OMNI_FLAT) {
            auto *flatVec = static_cast<Vector<T> *>(vec);
            return flatVec->GetValue(row);
        } else if (encoding == OMNI_DICTIONARY) {
            auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
            return dictVec->GetValue(row);
        } else {
            OMNI_THROW("TimestampConversion function Error", 
                       "Unsupported encoding type: " + std::to_string(static_cast<int>(encoding)));
        }
    }

    template<typename T>
    void SetValueToVectorHelper(BaseVector *vec, int32_t row, const T &value) {
        auto *resultVec = static_cast<Vector<T> *>(vec);
        resultVec->SetValue(row, value);
    }

    void TimestampMicrosFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                                         BaseVector *&result, ExecutionContext *context) const {
        if (args.empty()) {
            OMNI_THROW("TimestampMicros function Error", "No input arguments");
        }

        auto inputArg = args.top();
        args.pop();
        auto inputGuard = MakeVectorArgGuard(inputArg);

        DataTypeId inputTypeId = inputArg->GetTypeId();

        switch (inputTypeId) {
            case OMNI_BYTE:
                ProcessIntegral<int8_t>(inputArg, result);
                break;
            case OMNI_SHORT:
                ProcessIntegral<int16_t>(inputArg, result);
                break;
            case OMNI_INT:
                ProcessIntegral<int32_t>(inputArg, result);
                break;
            case OMNI_LONG:
                ProcessIntegral<int64_t>(inputArg, result);
                break;
            default:
                OMNI_THROW("TimestampMicros function Error",
                           "Unsupported input type: " + TypeUtil::TypeToString(inputTypeId));
        }
    }

    template<typename T>
    void TimestampMicrosFunction::ProcessIntegral(BaseVector *input, BaseVector *&result) const {
        auto size = input->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);

        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            T inputValue = GetValueFromVectorHelper<T>(input, row);
            int64_t micros = static_cast<int64_t>(inputValue);
            Timestamp timestamp = Timestamp::fromMicrosNoError(micros);
            int64_t timestampValue = timestamp.toMicros();
            SetValueToVectorHelper(result, row, timestampValue);
        }
    }

    void TimestampMillisFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                                         BaseVector *&result, ExecutionContext *context) const {
        if (args.empty()) {
            OMNI_THROW("TimestampMillis function Error", "No input arguments");
        }

        auto inputArg = args.top();
        args.pop();
        auto inputGuard = MakeVectorArgGuard(inputArg);

        DataTypeId inputTypeId = inputArg->GetTypeId();

        switch (inputTypeId) {
            case OMNI_BYTE:
                ProcessIntegral<int8_t>(inputArg, result);
                break;
            case OMNI_SHORT:
                ProcessIntegral<int16_t>(inputArg, result);
                break;
            case OMNI_INT:
                ProcessIntegral<int32_t>(inputArg, result);
                break;
            case OMNI_LONG:
                ProcessIntegral<int64_t>(inputArg, result);
                break;
            default:
                OMNI_THROW("TimestampMillis function Error",
                           "Unsupported input type: " + TypeUtil::TypeToString(inputTypeId));
        }
    }

    template<typename T>
    void TimestampMillisFunction::ProcessIntegral(BaseVector *input, BaseVector *&result) const {
        auto size = input->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);

        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            T inputValue = GetValueFromVectorHelper<T>(input, row);
            int64_t millis = static_cast<int64_t>(inputValue);
            Timestamp timestamp = Timestamp::fromMillisNoError(millis);
            int64_t timestampValue = timestamp.toMicros();
            SetValueToVectorHelper(result, row, timestampValue);
        }
    }

    void TimestampSecondsFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                                         BaseVector *&result, ExecutionContext *context) const {
        if (args.empty()) {
            OMNI_THROW("TimestampSeconds function Error", "No input arguments");
        }

        auto inputArg = args.top();
        args.pop();
        auto inputGuard = MakeVectorArgGuard(inputArg);

        DataTypeId inputTypeId = inputArg->GetTypeId();

        switch (inputTypeId) {
            case OMNI_BYTE:
                ProcessIntegral<int8_t>(inputArg, result);
                break;
            case OMNI_SHORT:
                ProcessIntegral<int16_t>(inputArg, result);
                break;
            case OMNI_INT:
                ProcessIntegral<int32_t>(inputArg, result);
                break;
            case OMNI_LONG:
                ProcessIntegral<int64_t>(inputArg, result);
                break;
            case OMNI_FLOAT:
                ProcessFloatingPoint<float>(inputArg, result);
                break;
            case OMNI_DOUBLE:
                ProcessFloatingPoint<double>(inputArg, result);
                break;
            default:
                OMNI_THROW("TimestampSeconds function Error",
                           "Unsupported input type: " + TypeUtil::TypeToString(inputTypeId));
        }
    }

    template<typename T>
    void TimestampSecondsFunction::ProcessIntegral(BaseVector *input, BaseVector *&result) const {
        auto size = input->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);

        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            T inputValue = GetValueFromVectorHelper<T>(input, row);
            Timestamp timestamp(static_cast<int64_t>(inputValue), 0);
            int64_t timestampValue = timestamp.toMicros();
            SetValueToVectorHelper(result, row, timestampValue);
        }
    }

    template<typename T>
    void TimestampSecondsFunction::ProcessFloatingPoint(BaseVector *input, BaseVector *&result) const {
        auto size = input->GetSize();
        result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);

        // Cast to double and check bounds to prevent ensuing overflow.
        static constexpr double kMaxSecondsD =
            static_cast<double>(std::numeric_limits<int64_t>::max()) / Timestamp::kMicrosecondsInSecond;
        static constexpr double kMinSecondsD =
            static_cast<double>(std::numeric_limits<int64_t>::min()) / Timestamp::kMicrosecondsInSecond;

        // Cutoff values are based on Java's Long.MAX_VALUE and Long.MIN_VALUE.
        static constexpr int64_t kMaxSeconds = 9223372036854LL;
        static constexpr int64_t kMaxNanoseconds = 775807000LL;
        static constexpr int64_t kMinSeconds = -9223372036855LL;
        static constexpr int64_t kMinNanoseconds = 224192000LL;

        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            T seconds = GetValueFromVectorHelper<T>(input, row);

            if (!std::isfinite(seconds)) {
                result->SetNull(row);
                continue;
            }

            double secondsD = static_cast<double>(seconds);

            Timestamp timestamp;
            if (secondsD >= kMaxSecondsD) {
                timestamp = Timestamp(kMaxSeconds, kMaxNanoseconds);
            } else if (secondsD <= kMinSecondsD) {
                timestamp = Timestamp(kMinSeconds, kMinNanoseconds);
            } else {
                // Scale to microseconds and truncate toward zero
                const double microsD = secondsD * Timestamp::kMicrosecondsInSecond;
                const int64_t micros = static_cast<int64_t>(microsD);

                // Split into whole seconds and remaining microseconds
                int64_t wholeSeconds = micros / Timestamp::kMicrosecondsInSecond;
                int64_t remainingMicros = micros % Timestamp::kMicrosecondsInSecond;
                if (remainingMicros < 0) {
                    wholeSeconds -= 1;
                    remainingMicros += Timestamp::kMicrosecondsInSecond;
                }

                const int64_t nano = remainingMicros * Timestamp::kNanosecondsInMicrosecond;
                timestamp = Timestamp(wholeSeconds, nano);
            }

            int64_t timestampValue = timestamp.toMicros();
            SetValueToVectorHelper(result, row, timestampValue);
        }
    }

    // Explicit template instantiations
    template void TimestampMicrosFunction::ProcessIntegral<int8_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMicrosFunction::ProcessIntegral<int16_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMicrosFunction::ProcessIntegral<int32_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMicrosFunction::ProcessIntegral<int64_t>(BaseVector *, BaseVector *&) const;

    template void TimestampMillisFunction::ProcessIntegral<int8_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMillisFunction::ProcessIntegral<int16_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMillisFunction::ProcessIntegral<int32_t>(BaseVector *, BaseVector *&) const;
    template void TimestampMillisFunction::ProcessIntegral<int64_t>(BaseVector *, BaseVector *&) const;

    template void TimestampSecondsFunction::ProcessIntegral<int8_t>(BaseVector *, BaseVector *&) const;
    template void TimestampSecondsFunction::ProcessIntegral<int16_t>(BaseVector *, BaseVector *&) const;
    template void TimestampSecondsFunction::ProcessIntegral<int32_t>(BaseVector *, BaseVector *&) const;
    template void TimestampSecondsFunction::ProcessIntegral<int64_t>(BaseVector *, BaseVector *&) const;
    template void TimestampSecondsFunction::ProcessFloatingPoint<float>(BaseVector *, BaseVector *&) const;
    template void TimestampSecondsFunction::ProcessFloatingPoint<double>(BaseVector *, BaseVector *&) const;

    template int8_t GetValueFromVectorHelper<int8_t>(BaseVector *, int32_t);
    template int16_t GetValueFromVectorHelper<int16_t>(BaseVector *, int32_t);
    template int32_t GetValueFromVectorHelper<int32_t>(BaseVector *, int32_t);
    template int64_t GetValueFromVectorHelper<int64_t>(BaseVector *, int32_t);
    template float GetValueFromVectorHelper<float>(BaseVector *, int32_t);
    template double GetValueFromVectorHelper<double>(BaseVector *, int32_t);

    template void SetValueToVectorHelper<int64_t>(BaseVector *, int32_t, const int64_t &);

    // Registration functions
    void RegisterTimestampMicrosFunction(const std::string &name)
    {
        auto timestampMicrosFunction = std::make_shared<TimestampMicrosFunction>();
        // Register for all integral types
        VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE}, OMNI_TIMESTAMP, timestampMicrosFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT}, OMNI_TIMESTAMP, timestampMicrosFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_TIMESTAMP, timestampMicrosFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_TIMESTAMP, timestampMicrosFunction);
    }

    void RegisterTimestampMillisFunction(const std::string &name)
    {
        auto timestampMillisFunction = std::make_shared<TimestampMillisFunction>();
        // Register for all integral types
        VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE}, OMNI_TIMESTAMP, timestampMillisFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT}, OMNI_TIMESTAMP, timestampMillisFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_TIMESTAMP, timestampMillisFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_TIMESTAMP, timestampMillisFunction);
    }

    void RegisterTimestampSecondsFunction(const std::string &name)
    {
        auto timestampSecondsFunction = std::make_shared<TimestampSecondsFunction>();
        // Register for all integral types
        VectorFunction::RegisterVectorFunction(name, {OMNI_BYTE}, OMNI_TIMESTAMP, timestampSecondsFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_SHORT}, OMNI_TIMESTAMP, timestampSecondsFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_INT}, OMNI_TIMESTAMP, timestampSecondsFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_LONG}, OMNI_TIMESTAMP, timestampSecondsFunction);
        // Register for floating point types
        VectorFunction::RegisterVectorFunction(name, {OMNI_FLOAT}, OMNI_TIMESTAMP, timestampSecondsFunction);
        VectorFunction::RegisterVectorFunction(name, {OMNI_DOUBLE}, OMNI_TIMESTAMP, timestampSecondsFunction);
    }
}
