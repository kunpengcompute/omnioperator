/**
* Copyright (C) 2025-2025. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PREDICATECONDITION_H
#define PREDICATECONDITION_H

#include <cstdint>
#include <memory>
#include <utility>
#include <set>
#include <arm_neon.h>
#include "vector/vector_common.h"
#include "util/bit_util.h"
#include "PredicateOperatorType.h"
#include "TimeRebaseInfo.h"

using omniruntime::BitUtil;
using omniruntime::mem::AlignedBuffer;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::unsafe::UnsafeVector;
using omniruntime::vec::unsafe::UnsafeBaseVector;
using omniruntime::exception::OmniException;

namespace common {
    constexpr int32_t NEON_BYTE_SIZE = 16;

    // SQL predicate state for one batch. FALSE is represented by a row being
    // absent from both bitmaps: validBits & ~(trueBits | unknownBits).
    struct PredicateResult {
        uint8_t *trueBits;
        uint8_t *unknownBits;
    };

    inline uint8_t validBitsMask(int32_t vectorSize, int32_t byteIndex) {
        int32_t remainingBits = vectorSize - byteIndex * 8;
        if (remainingBits >= 8) {
            return 0xff;
        }
        if (remainingBits <= 0) {
            return 0;
        }
        return static_cast<uint8_t>((1U << remainingBits) - 1);
    }

    inline void clearUnusedTailBits(uint8_t *bits, int32_t vectorSize) {
        int32_t remainingBits = vectorSize & 7;
        if (remainingBits == 0 || vectorSize == 0) {
            return;
        }
        bits[BitUtil::Nbytes(vectorSize) - 1] &= static_cast<uint8_t>((1U << remainingBits) - 1);
    }

    namespace vector_type {
        template<size_t S>
        struct neon_vector_type_impl;

        template<>
        struct neon_vector_type_impl<8> {
            using signed_type = int8x16_t;
            using unsigned_type = uint8x16_t;
        };

        template<>
        struct neon_vector_type_impl<16> {
            using signed_type = int16x8_t;
            using unsigned_type = uint16x8_t;
        };

        template<>
        struct neon_vector_type_impl<32> {
            using signed_type = int32x4_t;
            using unsigned_type = uint32x4_t;
        };

        template<>
        struct neon_vector_type_impl<64> {
            using signed_type = int64x2_t;
            using unsigned_type = uint64x2_t;
        };

        template<class T>
        using neon_vector_type = typename std::conditional_t<std::is_same<T, double>::value, float64x2_t,
            typename neon_vector_type_impl<8 * sizeof(T)>::signed_type>;
    }

    namespace data_transfer {
        // broadcast操作
        template <typename T, typename std::enable_if<std::is_same<T, int8_t>::value, int>::type = 0>
        inline int8x16_t broadcast(T val) {
            return vdupq_n_s8(int8_t(val));
        }

        template <typename T, typename std::enable_if<std::is_same<T, int16_t>::value, int>::type = 0>
        inline int16x8_t broadcast(T val) {
            return vdupq_n_s16(int16_t(val));
        }

        template <typename T, typename std::enable_if<std::is_same<T, int32_t>::value, int>::type = 0>
        inline int32x4_t broadcast(T val) {
            return vdupq_n_s32(int32_t(val));
        }

        template <typename T, typename std::enable_if<std::is_same<T, int64_t>::value, int>::type = 0>
        inline int64x2_t broadcast(T val) {
            return vdupq_n_s64(int64_t(val));
        }

        template <typename T, typename std::enable_if<std::is_same<T, double>::value, int>::type = 0>
        inline float64x2_t broadcast(T value) {
            return vdupq_n_f64(value);
        }

        // load操作
        template <typename T, typename std::enable_if<std::is_same<T, int8_t>::value, int>::type = 0>
        inline int8x16_t load_data(T const* src) {
            return vld1q_s8((int8_t*)src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int16_t>::value, int>::type = 0>
        inline int16x8_t load_data(T const* src) {
            return vld1q_s16((int16_t*)src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int32_t>::value, int>::type = 0>
        inline int32x4_t load_data(T const* src) {
            return vld1q_s32((int32_t*)src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int64_t>::value, int>::type = 0>
        inline int64x2_t load_data(T const* src) {
            return vld1q_s64((int64_t*)src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, double>::value, int>::type = 0>
        inline float64x2_t load_data(T const* src) {
            return vld1q_f64(src);
        }

        // store操作
        template <typename T, typename std::enable_if<std::is_same<T, int8_t>::value, int>::type = 0>
        inline void store_data(T* dst, const int8x16_t &src) {
            vst1q_s8((int8_t*)dst, src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int16_t>::value, int>::type = 0>
        inline void store_data(T* dst, const int16x8_t &src) {
            vst1q_s16((int16_t*)dst, src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int32_t>::value, int>::type = 0>
        inline void store_data(T* dst, const int32x4_t &src) {
            vst1q_s32((int32_t*)dst, src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, int64_t>::value, int>::type = 0>
        inline void store_data(T* dst, const int64x2_t &src) {
            vst1q_s64((int64_t*)dst, src);
        }

        template <typename T, typename std::enable_if<std::is_same<T, double>::value, int>::type = 0>
        inline void store_data(T* dst, const float64x2_t &src) {
            vst1q_f64(dst, src);
        }
    }

    namespace data_operator {
        template <typename V>
        inline bool equalTo(const V &left, const V &right) {
            return left == right;
        }

        template <typename V>
        inline bool greater(const V &left, const V &right) {
            return left > right;
        }

        template <typename V>
        inline bool greaterEqual(const V &left, const V &right) {
            return left >= right;
        }

        template <typename V>
        inline bool less(const V &left, const V &right) {
            return left < right;
        }

        template <typename V>
        inline bool lessEqual(const V &left, const V &right) {
            return left <= right;
        }

        template <typename BatchType>
        inline BatchType batchEqualTo(const BatchType &left, const BatchType &right) {
            return left == right;
        }

        template <typename BatchType>
        inline BatchType batchGreater(const BatchType &left, const BatchType &right) {
            return left > right;
        }

        template <typename BatchType>
        inline BatchType batchGreaterEqual(const BatchType &left, const BatchType &right) {
            return left >= right;
        }

        template <typename BatchType>
        inline BatchType batchLess(const BatchType &left, const BatchType &right) {
            return left < right;
        }

        template <typename BatchType>
        inline BatchType batchLessEqual(const BatchType &left, const BatchType &right) {
            return left <= right;
        }

        template <typename T>
        inline uint64_t mask(T *buffer, int32_t size) {
            uint64_t res = 0;
            for (int32_t i = 0; i < size; i++) {
                if (buffer[i]) {
                    res |= 1uL << i;
                }
            }
            return res;
        }
    }

    class PredicateCondition {
    public:
        explicit PredicateCondition(const PredicateOperatorType &opType) {
            this->op = opType;
        }

        virtual ~PredicateCondition() = default;

        virtual PredicateResult compute(std::vector<BaseVector *> &vecBatch) = 0;

        virtual void init(const int32_t size) {
            bitSize = size;
            bitMarkBuf = std::make_unique<AlignedBuffer<uint8_t>>(BitUtil::Nbytes(size) + 8, true);
            bitMark = bitMarkBuf->GetBuffer();
            unknownMarkBuf = std::make_unique<AlignedBuffer<uint8_t>>(BitUtil::Nbytes(size) + 8, true);
            unknownMark = unknownMarkBuf->GetBuffer();
        }

        virtual bool isAllNull(int32_t columnIndex) {
            return false;
        }

        virtual bool isAllNotNull(int32_t columnIndex) {
            return false;
        }

        void buildNullColumns(int32_t columnCount) {
            for (int32_t i = 0; i < columnCount; i++) {
                if (isAllNull(i)) {
                    isAllNullColumns.insert(i);
                }
                if (isAllNotNull(i)) {
                    isAllNotNullColumns.insert(i);
                }
            }
        }

        std::set<int32_t> &getIsAllNullColumns() {
            return isAllNullColumns;
        }

        std::set<int32_t> &getIsAllNotNullColumns() {
            return isAllNotNullColumns;
        }

        std::unique_ptr<common::TimeRebaseInfo> timeRebaseInfo;

    protected:
        PredicateOperatorType op;
        int32_t bitSize = 0;
        std::unique_ptr<AlignedBuffer<uint8_t>> bitMarkBuf;
        uint8_t *bitMark = nullptr;
        std::unique_ptr<AlignedBuffer<uint8_t>> unknownMarkBuf;
        uint8_t *unknownMark = nullptr;
        std::set<int32_t> isAllNullColumns;
        std::set<int32_t> isAllNotNullColumns;
    };

    template<typename T>
    class LeafPredicateCondition final : public PredicateCondition {
    public:
        using BatchType = vector_type::neon_vector_type<T>;

        LeafPredicateCondition(const PredicateOperatorType &opType, int32_t index, T value)
            : PredicateCondition(opType), index(index), value(value) {
        }

        template<BatchType (*BATCH_OP)(const BatchType &, const BatchType &), bool (*OP)(const T &, const T &)>
        void computeCompare(BaseVector *vector, int32_t vectorSize) {
            BatchType broadcast = data_transfer::broadcast(value);
            int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(T));
            Vector<T> *realVector = reinterpret_cast<Vector<T> *>(vector);
            T *values = UnsafeVector::GetRawValues(realVector);
            T buffer[step];
            int32_t idx = 0;
            for (; idx + step <= vectorSize; idx += step) {
                BatchType valuesBatch = data_transfer::load_data(values + idx);
                BatchType result = BATCH_OP(valuesBatch, broadcast);
                data_transfer::store_data(buffer, result);
                uint64_t mask = data_operator::mask(buffer, step);
                BitUtil::StoreBits<uint64_t>(reinterpret_cast<uint64_t *>(bitMark), idx, mask, 64);
            }
            for (; idx < vectorSize; idx++) {
                bool result = OP(values[idx], value);
                BitUtil::SetBit(bitMark, idx, result);
            }
        }

        PredicateResult compute(std::vector<BaseVector *> &vecBatch) override {
            auto vector = vecBatch[index];
            auto vectorSize = vector->GetSize();
            int32_t byteLen = BitUtil::Nbytes(vectorSize);
            switch (op) {
                case TRUE: {
                    memset(bitMark, -1, byteLen);
                    memset(unknownMark, 0, byteLen);
                    break;
                }
                case FALSE: {
                    memset(bitMark, 0, byteLen);
                    memset(unknownMark, 0, byteLen);
                    break;
                }
                case EQUAL_TO: {
                    computeCompare<data_operator::batchEqualTo, data_operator::equalTo>(vector, vectorSize);
                    break;
                }
                case GREATER_THAN: {
                    computeCompare<data_operator::batchGreater, data_operator::greater>(vector, vectorSize);
                    break;
                }
                case GREATER_THAN_OR_EQUAL: {
                    computeCompare<data_operator::batchGreaterEqual, data_operator::greaterEqual>(vector, vectorSize);
                    break;
                }
                case LESS_THAN: {
                    computeCompare<data_operator::batchLess, data_operator::less>(vector, vectorSize);
                    break;
                }
                case LESS_THAN_OR_EQUAL: {
                    computeCompare<data_operator::batchLessEqual, data_operator::lessEqual>(vector, vectorSize);
                    break;
                }
                case IS_NOT_NULL: {
                    uint8_t *nulls = reinterpret_cast<uint8_t *>(UnsafeBaseVector::GetNulls(vector));
                    int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
                    int32_t idx = 0;
                    for (; idx + step <= byteLen; idx += step) {
                        uint8x16_t valuesBatch = vld1q_u8(nulls + idx);
                        uint8x16_t result = ~valuesBatch;
                        vst1q_u8(bitMark + idx, result);
                    }
                    for (; idx < byteLen; idx++) {
                        bitMark[idx] = ~nulls[idx];
                    }
                    memset(unknownMark, 0, byteLen);
                    break;
                }
                case IS_NULL: {
                    memcpy(bitMark, UnsafeBaseVector::GetNulls(vector), byteLen);
                    memset(unknownMark, 0, byteLen);
                    break;
                }
                default:
                    throw OmniException("OPERATOR_RUNTIME_ERROR", 
                        "LeafPredicateCondition UnSupport OperatorType: " + std::to_string(op));
            }

            if (op == EQUAL_TO || op == GREATER_THAN || op == GREATER_THAN_OR_EQUAL ||
                op == LESS_THAN || op == LESS_THAN_OR_EQUAL) {
                // ORC does not write values for NULL rows. Move null-bitmap handling into
                // predicate evaluation so those indeterminate value slots are never treated
                // as ordinary comparison results.
                if (vector->HasNull()) {
                    auto *nulls = reinterpret_cast<uint8_t *>(UnsafeBaseVector::GetNulls(vector));
                    for (int32_t i = 0; i < byteLen; ++i) {
                        unknownMark[i] = nulls[i] & validBitsMask(vectorSize, i);
                        bitMark[i] &= static_cast<uint8_t>(~nulls[i]);
                    }
                } else {
                    memset(unknownMark, 0, byteLen);
                }
            }

            clearUnusedTailBits(bitMark, vectorSize);
            clearUnusedTailBits(unknownMark, vectorSize);
            return {bitMark, unknownMark};
        }

        bool isAllNull(int32_t columnIndex) override {
            return columnIndex == index && op == IS_NULL;
        }

        bool isAllNotNull(int32_t columnIndex) override {
            return columnIndex == index && op == IS_NOT_NULL;
        }

    private:
        int32_t index;
        T value;
    };

    class NotPredicateCondition : public PredicateCondition {
    public:
        explicit NotPredicateCondition(std::unique_ptr<PredicateCondition> child) : PredicateCondition(NOT),
            child(std::move(child)) {
        }

        void init(const int32_t size) override {
            PredicateCondition::init(size);
            child->init(size);
        }

        PredicateResult compute(std::vector<BaseVector *> &vecBatch) override {
            auto vectorSize = vecBatch[0]->GetSize();
            PredicateResult childResult = child->compute(vecBatch);
            int32_t byteLen = BitUtil::Nbytes(vectorSize);
            int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
            int32_t index = 0;
            for (; index + step <= byteLen; index += step) {
                uint8x16_t childTrue = vld1q_u8(childResult.trueBits + index);
                uint8x16_t childUnknown = vld1q_u8(childResult.unknownBits + index);
                vst1q_u8(bitMark + index, ~(childTrue | childUnknown));
                vst1q_u8(unknownMark + index, childUnknown);
            }
            for (; index < byteLen; ++index) {
                uint8_t validBits = validBitsMask(vectorSize, index);
                bitMark[index] = validBits & static_cast<uint8_t>(
                    ~(childResult.trueBits[index] | childResult.unknownBits[index]));
                unknownMark[index] = validBits & childResult.unknownBits[index];
            }
            clearUnusedTailBits(bitMark, vectorSize);
            clearUnusedTailBits(unknownMark, vectorSize);
            return {bitMark, unknownMark};
        }

    private:
        std::unique_ptr<PredicateCondition> child;
    };

    class AndPredicateCondition : public PredicateCondition {
    public:
        AndPredicateCondition(std::unique_ptr<PredicateCondition> left, std::unique_ptr<PredicateCondition> right)
            : PredicateCondition(AND), left(std::move(left)), right(std::move(right)) {
        }

        void init(const int32_t size) override {
            PredicateCondition::init(size);
            left->init(size);
            right->init(size);
        }

        PredicateResult compute(std::vector<BaseVector *> &vecBatch) override {
            auto vectorSize = vecBatch[0]->GetSize();
            PredicateResult leftResult = left->compute(vecBatch);
            PredicateResult rightResult = right->compute(vecBatch);
            int32_t byteLen = BitUtil::Nbytes(vectorSize);
            int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
            int32_t index = 0;
            for (; index + step <= byteLen; index += step) {
                uint8x16_t leftTrue = vld1q_u8(leftResult.trueBits + index);
                uint8x16_t leftUnknown = vld1q_u8(leftResult.unknownBits + index);
                uint8x16_t rightTrue = vld1q_u8(rightResult.trueBits + index);
                uint8x16_t rightUnknown = vld1q_u8(rightResult.unknownBits + index);
                uint8x16_t trueBits = leftTrue & rightTrue;
                uint8x16_t falseBits = (~(leftTrue | leftUnknown)) | (~(rightTrue | rightUnknown));
                vst1q_u8(bitMark + index, trueBits);
                vst1q_u8(unknownMark + index, ~(trueBits | falseBits));
            }
            for (; index < byteLen; ++index) {
                uint8_t validBits = validBitsMask(vectorSize, index);
                uint8_t leftFalse = validBits & static_cast<uint8_t>(
                    ~(leftResult.trueBits[index] | leftResult.unknownBits[index]));
                uint8_t rightFalse = validBits & static_cast<uint8_t>(
                    ~(rightResult.trueBits[index] | rightResult.unknownBits[index]));
                bitMark[index] = leftResult.trueBits[index] & rightResult.trueBits[index];
                uint8_t falseBits = leftFalse | rightFalse;
                unknownMark[index] = validBits & static_cast<uint8_t>(~(bitMark[index] | falseBits));
            }
            clearUnusedTailBits(bitMark, vectorSize);
            clearUnusedTailBits(unknownMark, vectorSize);
            return {bitMark, unknownMark};
        }

        bool isAllNull(int32_t columnIndex) override {
            return left->isAllNull(columnIndex) || right->isAllNull(columnIndex);
        }

        bool isAllNotNull(int32_t columnIndex) override {
            return left->isAllNotNull(columnIndex) || right->isAllNotNull(columnIndex);
        }

    private:
        std::unique_ptr<PredicateCondition> left;
        std::unique_ptr<PredicateCondition> right;
    };

    class OrPredicateCondition : public PredicateCondition {
    public:
        OrPredicateCondition(std::unique_ptr<PredicateCondition> left, std::unique_ptr<PredicateCondition> right)
            : PredicateCondition(OR), left(std::move(left)), right(std::move(right)) {
        }

        void init(const int32_t size) override {
            PredicateCondition::init(size);
            left->init(size);
            right->init(size);
        }

        PredicateResult compute(std::vector<BaseVector *> &vecBatch) override {
            auto vectorSize = vecBatch[0]->GetSize();
            PredicateResult leftResult = left->compute(vecBatch);
            PredicateResult rightResult = right->compute(vecBatch);
            int32_t byteLen = BitUtil::Nbytes(vectorSize);
            int32_t step = static_cast<int32_t>(NEON_BYTE_SIZE / sizeof(uint8_t));
            int32_t index = 0;
            for (; index + step <= byteLen; index += step) {
                uint8x16_t leftTrue = vld1q_u8(leftResult.trueBits + index);
                uint8x16_t leftUnknown = vld1q_u8(leftResult.unknownBits + index);
                uint8x16_t rightTrue = vld1q_u8(rightResult.trueBits + index);
                uint8x16_t rightUnknown = vld1q_u8(rightResult.unknownBits + index);
                uint8x16_t trueBits = leftTrue | rightTrue;
                uint8x16_t falseBits = (~(leftTrue | leftUnknown)) & (~(rightTrue | rightUnknown));
                vst1q_u8(bitMark + index, trueBits);
                vst1q_u8(unknownMark + index, ~(trueBits | falseBits));
            }
            for (; index < byteLen; ++index) {
                uint8_t validBits = validBitsMask(vectorSize, index);
                uint8_t leftFalse = validBits & static_cast<uint8_t>(
                    ~(leftResult.trueBits[index] | leftResult.unknownBits[index]));
                uint8_t rightFalse = validBits & static_cast<uint8_t>(
                    ~(rightResult.trueBits[index] | rightResult.unknownBits[index]));
                bitMark[index] = leftResult.trueBits[index] | rightResult.trueBits[index];
                uint8_t falseBits = leftFalse & rightFalse;
                unknownMark[index] = validBits & static_cast<uint8_t>(~(bitMark[index] | falseBits));
            }
            clearUnusedTailBits(bitMark, vectorSize);
            clearUnusedTailBits(unknownMark, vectorSize);
            return {bitMark, unknownMark};
        }

    private:
        std::unique_ptr<PredicateCondition> left;
        std::unique_ptr<PredicateCondition> right;
    };
}

#endif //PREDICATECONDITION_H
