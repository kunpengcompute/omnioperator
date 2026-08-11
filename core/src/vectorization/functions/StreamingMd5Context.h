/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Reusable streaming MD5 context for vectorized expressions
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace omniruntime::vectorization {

class StreamingMd5Context final {
public:
    static constexpr size_t BINARY_DIGEST_SIZE = 16;
    static constexpr size_t HEX_DIGEST_SIZE = BINARY_DIGEST_SIZE * 2;

    StreamingMd5Context();

    void Reset();

    void Update(const char *data, size_t size);

    void Finish(uint8_t output[BINARY_DIGEST_SIZE]);

    void FinishHex(char output[HEX_DIGEST_SIZE]);

    static void DigestToHex(const uint8_t digest[BINARY_DIGEST_SIZE], char output[HEX_DIGEST_SIZE]);

private:
    void Transform(const uint8_t block[64]);

    uint32_t state_[4] {};
    uint64_t totalBytes_ = 0;
    size_t bufferedBytes_ = 0;
    uint8_t buffer_[64] {};
};

} // namespace omniruntime::vectorization
