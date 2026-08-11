/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Vectorized MD5 function with optional ISA-L Crypto backend
 */

#include "vectorization/functions/Md5VectorFunction.h"

#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#include <isa-l_crypto/md5_mb.h>

#include "type/string_Impl.h"
#include "util/compiler_util.h"
#include "util/debug.h"
#include "vectorization/functions/StreamingMd5Context.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

constexpr int32_t SCALAR_THRESHOLD = 55;
constexpr size_t MIN_MULTI_BUFFER_ROWS = 8;
constexpr size_t CONTEXT_POOL_SIZE = 64;
constexpr size_t PREFETCH_DISTANCE = 8;

using HexDigest = std::array<char, StreamingMd5Context::HEX_DIGEST_SIZE>;
using BinaryDigest = std::array<uint8_t, StreamingMd5Context::BINARY_DIGEST_SIZE>;

ALWAYS_INLINE bool IsNullAt(BaseVector *vector, int32_t row)
{
    const int32_t index = vector->GetEncoding() == OMNI_ENCODING_CONST ? 0 : row;
    return vector->IsNull(index);
}

void ComputeScalarMd5(BaseVector *input, StreamingMd5Context &md5, int32_t row, HexDigest &digest)
{
    const std::string_view value = VectorHelper::GetStringValueFromVector(input, row);
    md5.Reset();
    md5.Update(value.data(), value.size());
    md5.FinishHex(digest.data());
}

void ComputeScalarMd5(BaseVector *input, StreamingMd5Context &md5, int32_t row, BinaryDigest &digest)
{
    const std::string_view value = VectorHelper::GetStringValueFromVector(input, row);
    md5.Reset();
    md5.Update(value.data(), value.size());
    md5.Finish(digest.data());
}

void CommitDigest(Vector<LargeStringContainer<std::string_view>> *output, int32_t row, const HexDigest &digest)
{
    output->SetValue(row, std::string_view(digest.data(), digest.size()));
    output->SetNotNull(row);
}

} // namespace

void Md5VectorFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("Md5 function Error", "Md5 requires one argument");
    }

    BaseVector *input = args.top();
    args.pop();
    const int32_t rowSize = context->GetResultRowSize();
    auto *output = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateStringVector(rowSize));

    std::vector<int32_t> scalarRows;
    std::array<std::vector<int32_t>, 4> vectorBuckets;
    scalarRows.reserve(rowSize);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(input, row)) {
            continue;
        }
        const size_t length = VectorHelper::GetStringValueFromVector(input, row).size();
        if (length <= SCALAR_THRESHOLD || length > std::numeric_limits<uint32_t>::max()) {
            scalarRows.push_back(row);
        } else if (length <= 119) {
            vectorBuckets[0].push_back(row);
        } else if (length <= 183) {
            vectorBuckets[1].push_back(row);
        } else if (length <= 247) {
            vectorBuckets[2].push_back(row);
        } else {
            vectorBuckets[3].push_back(row);
        }
    }

    for (auto &bucket : vectorBuckets) {
        if (bucket.size() < MIN_MULTI_BUFFER_ROWS) {
            scalarRows.insert(scalarRows.end(), bucket.begin(), bucket.end());
            bucket.clear();
        }
    }

    bool hasMultiBufferRows = false;
    for (const auto &bucket : vectorBuckets) {
        if (!bucket.empty()) {
            hasMultiBufferRows = true;
            break;
        }
    }

    StreamingMd5Context scalarMd5;
    if (!hasMultiBufferRows) {
        HexDigest digest;
        for (int32_t row = 0; row < rowSize; ++row) {
            if (IsNullAt(input, row)) {
                output->SetNull(row);
                continue;
            }
            ComputeScalarMd5(input, scalarMd5, row, digest);
            CommitDigest(output, row, digest);
        }
        delete input;
        result = output;
        return;
    }

    std::vector<BinaryDigest> rowDigests(static_cast<size_t>(rowSize));
    std::vector<uint8_t> completedRows(static_cast<size_t>(rowSize), 0);
    for (const int32_t row : scalarRows) {
        ComputeScalarMd5(input, scalarMd5, row, rowDigests[static_cast<size_t>(row)]);
        completedRows[static_cast<size_t>(row)] = 1;
    }

    ISAL_MD5_HASH_CTX_MGR manager;
    isal_md5_ctx_mgr_init(&manager);
    std::array<ISAL_MD5_HASH_CTX, CONTEXT_POOL_SIZE> contextStorage;
    std::array<ISAL_MD5_HASH_CTX *, CONTEXT_POOL_SIZE> freeContexts;
    size_t freeContextCount = CONTEXT_POOL_SIZE;
    for (size_t i = 0; i < CONTEXT_POOL_SIZE; ++i) {
        freeContexts[i] = &contextStorage[i];
    }

    auto processCompleted = [&](ISAL_MD5_HASH_CTX *completed) {
        if (completed == nullptr) {
            return;
        }
        const auto row = static_cast<int32_t>(reinterpret_cast<uintptr_t>(completed->user_data));
        const auto *binaryDigest = reinterpret_cast<const uint8_t *>(completed->job.result_digest);
        std::memcpy(rowDigests[static_cast<size_t>(row)].data(), binaryDigest,
            StreamingMd5Context::BINARY_DIGEST_SIZE);
        completedRows[static_cast<size_t>(row)] = 1;
        freeContexts[freeContextCount++] = completed;
    };

    for (const auto &bucket : vectorBuckets) {
        for (size_t i = 0; i < bucket.size(); ++i) {
            const int32_t row = bucket[i];
#if defined(__GNUC__) || defined(__clang__)
            if (i + PREFETCH_DISTANCE < bucket.size()) {
                const int32_t futureRow = bucket[i + PREFETCH_DISTANCE];
                __builtin_prefetch(VectorHelper::GetStringValueFromVector(input, futureRow).data(), 0, 1);
            }
#endif
            if (freeContextCount == 0) {
                ISAL_MD5_HASH_CTX *forced = nullptr;
                isal_md5_ctx_mgr_flush(&manager, &forced);
                processCompleted(forced);
                if (freeContextCount == 0) {
                    delete input;
                    delete output;
                    OMNI_THROW("Md5 function Error", "ISA-L failed to return an MD5 context");
                }
            }

            ISAL_MD5_HASH_CTX *current = freeContexts[--freeContextCount];
            const std::string_view value = VectorHelper::GetStringValueFromVector(input, row);
            isal_hash_ctx_init(current);
            current->user_data = reinterpret_cast<void *>(static_cast<uintptr_t>(row));
            ISAL_MD5_HASH_CTX *completed = nullptr;
            isal_md5_ctx_mgr_submit(&manager, current, &completed, value.data(),
                static_cast<uint32_t>(value.size()), ISAL_HASH_ENTIRE);
            processCompleted(completed);
        }
    }

    ISAL_MD5_HASH_CTX *flushed = nullptr;
    do {
        isal_md5_ctx_mgr_flush(&manager, &flushed);
        processCompleted(flushed);
    } while (flushed != nullptr);

    HexDigest digest;
    for (int32_t row = 0; row < rowSize; ++row) {
        if (IsNullAt(input, row)) {
            output->SetNull(row);
            continue;
        }
        if (completedRows[static_cast<size_t>(row)] == 0) {
            delete input;
            delete output;
            OMNI_THROW("Md5 function Error", "MD5 digest was not completed");
        }
        StreamingMd5Context::DigestToHex(rowDigests[static_cast<size_t>(row)].data(), digest.data());
        CommitDigest(output, row, digest);
    }

    delete input;
    result = output;
}

std::shared_ptr<VectorFunction> MakeMd5VectorFunction(const std::string &,
    const std::vector<DataTypeId> &, const config::QueryConfig &)
{
    return std::make_shared<Md5VectorFunction>();
}

} // namespace omniruntime::vectorization

#endif // OMNI_HAVE_ISAL_CRYPTO_MD5
