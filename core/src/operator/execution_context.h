/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2026. All rights reserved.
 */
#ifndef EXECUTION_CONTEXT_H
#define EXECUTION_CONTEXT_H

#include <cstdint>
#include <memory>
#include "memory/aligned_buffer.h"
#include "memory/simple_arena_allocator.h"
#include "util/config/QueryConfig.h"
#include "vectorization/SelectivityVector.h"
#include "vectorization/Status.h"
#include "util/bit_util.h"
#include "util/TimeUtils.h"

namespace omniruntime {
namespace expressions {
class LambdaExpr;
}
namespace type {
class DataType;
}

namespace op {
using namespace mem;

/// Tracks per-row errors that occurred during expression evaluation.
/// Used when EvalCtx::throwOnError() is false.
class EvalErrors {
public:
    EvalErrors(int32_t size): size_{size}
    {
        errorFlags_ = std::make_shared<AlignedBuffer<bool>>(size, true);
        rawErrorFlags_ = reinterpret_cast<uint64_t *>(errorFlags_->GetBuffer());
    }

    int32_t size() const
    {
        return size_;
    }

    /// Similar to std::vector::reserve. Allocates internal buffers to fit at
    /// least 'size' rows. No-op if 'size()' is already at or exceeding requested.
    void ensureCapacity(int32_t size)
    {
        if (size_ >= size) {
            return;
        }

        errorFlags_->AllocateReuse(size, true);
        rawErrorFlags_ = reinterpret_cast<uint64_t *>(errorFlags_->GetBuffer());
        if (!exceptions_.empty()) {
            exceptions_.assign(size_, TError());
        }

        size_ = size;
    }

    /// Returns true if at least one row has an error.
    bool hasError() const
    {
        const auto firstErrorRow = BitUtil::FindFirstBit(rawErrorFlags_, 0, size_);
        return firstErrorRow >= 0;
    }

    /// Returns true if 'index' has an error.
    bool hasErrorAt(int32_t index) const
    {
        return index < size_ && BitUtil::IsBitSet(rawErrorFlags_, index);
    }

    /// Throws if 'index' has an error. The caller must ensure that error details
    /// are available.
    void throwIfErrorAt(int32_t index) const
    {
        auto error = errorAt(index);
        if (error.has_value()) {
            std::rethrow_exception(*error.value());
        }
    }

    /// Returns std::nullopt if 'index' doesn't have an error.
    /// Returns nullptr if 'index' has an error, but error details are not
    /// available. Returns std::exception_ptr if 'index' has an error and error
    /// details are available.
    std::optional<std::shared_ptr<std::exception_ptr>> errorAt(int32_t index) const
    {
        if (!hasErrorAt(index)) {
            return std::nullopt;
        }

        if (exceptions_.empty()) {
            return {nullptr};
        }

        return exceptions_[index];
    }

    /// Bitmask with bits set for rows with errors. Only first 'size()' bits are
    /// valid.
    const uint64_t *errorFlags() const
    {
        return rawErrorFlags_;
    }

    /// Returns the number of rows with errors.
    int32_t countErrors() const
    {
        return BitUtil::CountBits(rawErrorFlags_, 0, size_);
    }

    /// Marks 'index' as having an error. Doesn't specify error details.
    void setError(int32_t index)
    {
        ensureCapacity(index + 1);
        BitUtil::SetBit(rawErrorFlags_, index);
    }

    /// Clears error at 'index'.
    void clearError(int32_t index)
    {
        if (index < size_) {
            BitUtil::ClearBit(rawErrorFlags_, index);
        }
    }

    /// Marks 'index' as having an error and sets the exception_ptr. No-op if
    /// 'index' is already marked as having an error.
    void setError(int32_t index, const std::exception_ptr &exceptionPtr)
    {
        ensureCapacity(index + 1);
        if (!BitUtil::IsBitSet(rawErrorFlags_, index)) {
            BitUtil::SetBit(rawErrorFlags_, index);

            if (exceptions_.empty()) {
                exceptions_ = std::vector<TError>(size_);
            }
            exceptions_[index] = std::make_shared<std::exception_ptr>(exceptionPtr);
        }
    }

    /// Copies an error from 'from' at index 'fromIndex' to this at index
    /// 'toIndex'. No-op if 'from' at index 'fromIndex' doesn't have an error or
    /// this already has an error at 'toIndex'.
    void copyError(const EvalErrors &from, int32_t fromIndex, int32_t toIndex)
    {
        if (from.hasErrorAt(fromIndex)) {
            ensureCapacity(toIndex + 1);
            if (!BitUtil::IsBitSet(rawErrorFlags_, toIndex)) {
                BitUtil::SetBit(rawErrorFlags_, toIndex);

                if (!from.exceptions_.empty()) {
                    if (exceptions_.empty()) {
                        exceptions_ = std::vector<TError>(size_);
                    }

                    exceptions_[toIndex] = from.exceptions_[fromIndex];
                }
            }
        }
    }

    /// Copies all errors from 'from' to corresponding rows in this. Doesn't
    /// overwrite existing errors.
    void copyErrors(const EvalErrors &from)
    {
        ensureCapacity(from.size());
        BitUtil::ForEachSetBit(from.errorFlags(), 0, from.size(), [&](auto row) {
            copyError(from, row, row);
        });
    }

private:
    using TError = std::shared_ptr<std::exception_ptr>;

    int32_t size_;
    std::shared_ptr<AlignedBuffer<bool>> errorFlags_;
    uint64_t *rawErrorFlags_;
    std::vector<TError> exceptions_;
};

using EvalErrorsPtr = std::shared_ptr<EvalErrors>;

// execution context during operator
class ExecutionContext {
public:
    explicit ExecutionContext(int64_t minChunkSize = 4096) : arena(minChunkSize) {}

    ~ExecutionContext() = default;

    mem::SimpleArenaAllocator *GetArena()
    {
        return &arena;
    }

    void SetError(std::string &message)
    {
        hasError = true;
        errorMessage = message;
    }

    bool HasError() const
    {
        return hasError;
    }

    std::string GetError() const
    {
        return errorMessage;
    }

    void ResetError()
    {
        hasError = false;
    }

    void SetResultRowSize(const int32_t size)
    {
        resultRowSize = size;
    }

    int32_t GetResultRowSize() const
    {
        return resultRowSize;
    }

    std::string GetErrorMsg() const
    {
        return errorMessage;
    }

    bool hasFilter = false;

    bool *GetIsSelectRow() const
    {
        return isSelectRow;
    }

    void SetIsSelectRow(bool *inIsSelectRow)
    {
        isSelectRow = inIsSelectRow;
    }

    config::QueryConfig queryConfig()
    {
        return queryConfig_;
    }

    const config::QueryConfig &queryConfigRef() const
    {
        return queryConfig_;
    }

    /// Boolean indicating whether to capture details when storing exceptions for
    /// later processing (throwOnError_ == true).
    ///
    /// Conjunct expressions (AND, OR) require capturing error details, while TRY
    /// and TRY_CAST expressions do not.
    bool captureErrorDetails() const
    {
        return captureErrorDetails_;
    }

    void EnsureErrorsVectorSize(EvalErrorsPtr &vector, int32_t size) const
    {
        if (!vector) {
            vector = std::make_shared<EvalErrors>(size);
        } else {
            vector->ensureCapacity(size);
        }
    }

    void AddError(int32_t index, const std::exception_ptr &exceptionPtr, EvalErrorsPtr &errorsPtr) const
    {
        EnsureErrorsVectorSize(errorsPtr, index + 1);
        errorsPtr->setError(index, exceptionPtr);
    }

    void AddError(int32_t index, EvalErrorsPtr &errorsPtr) const
    {
        EnsureErrorsVectorSize(errorsPtr, index + 1);
        errorsPtr->setError(index);
    }

    void SetOmniExceptionError(int32_t index, const std::exception_ptr &exceptionPtr)
    {
        if (throwOnError_) {
            std::rethrow_exception(exceptionPtr);
        }

        if (captureErrorDetails_) {
            AddError(index, exceptionPtr, errors_);
        } else {
            AddError(index, errors_);
        }
    }

    void SetConfig(const config::QueryConfig &queryConfig)
    {
        queryConfig_ = queryConfig;
    }

    void SetThrowOnError(bool throwOnError)
    {
        throwOnError_ = throwOnError;
    }

    void SetError(int32_t index, const std::exception_ptr &exceptionPtr);

    void SetStatus(vector_size_t index, const vectorization::Status &status);

    void SetCurrentLambda(const omniruntime::expressions::LambdaExpr *lambda) { currentLambda_ = lambda; }
    const omniruntime::expressions::LambdaExpr *GetCurrentLambda() const { return currentLambda_; }

    int32_t getInputParamsNUms() const {
        return inputParamsNUms;
    }

    void setInputParamsNUms(int32_t inputParamsNUms) {
        ExecutionContext::inputParamsNUms = inputParamsNUms;
    }

    void SetCurrentWatermark(int64_t currentWatermark)
    {
        currentWatermark_ = currentWatermark;
    }

    int64_t GetCurrentWatermark() const
    {
        return currentWatermark_;
    }

    // Input argument DataType (with struct field names) for to_json. The vectorized
    // function interface only receives the output type, so the caller stashes the
    // input type here so ToJson can emit real struct field names instead of field{i}.
    void SetToJsonInputType(const type::DataType *type) { toJsonInputType_ = type; }
    const type::DataType *GetToJsonInputType() const { return toJsonInputType_; }

private:
    bool *isSelectRow;
    int32_t resultRowSize;
    mem::SimpleArenaAllocator arena;
    bool hasError = false;
    std::string errorMessage;
    const omniruntime::expressions::LambdaExpr *currentLambda_ = nullptr;
    config::QueryConfig queryConfig_;
    int32_t inputParamsNUms = 0;
    int64_t currentWatermark_ = INT64_MIN;
    const type::DataType *toJsonInputType_ = nullptr;

    // True if nulls in the input vectors were pruned (removed from the current
    // selectivity vector). Only possible is all expressions have default null
    // behavior.
    bool nullsPruned_{false};
    bool throwOnError_{true};

    bool captureErrorDetails_{true};

    // Stores exceptions encountered during expression evaluation.
    // If 'captureErrorDetails()' is false, stores flags indicating which rows had
    // errors without storing actual exceptions.
    EvalErrorsPtr errors_;
};
} // namespace op
} // namespace omniruntime
#endif // EXECUTION_CONTEXT_H
