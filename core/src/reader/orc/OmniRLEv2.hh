/**
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

#ifndef OMNI_RLEV2_HH
#define OMNI_RLEV2_HH

#include "orc/RLEv2.hh"
#include "orc/RLEV2Util.hh"
#include <algorithm>
#include <vector/vector_common.h>
#include "util/bit_util.h"
#include "reader/common/RowSet.h"

namespace omniruntime::reader {

    std::unique_ptr<omniruntime::vec::BaseVector> makeFixedLengthVector(uint64_t numValues,
        omniruntime::type::DataTypeId dataTypeId);

    std::unique_ptr<omniruntime::vec::BaseVector> makeDoubleVector(uint64_t numValues,
        omniruntime::type::DataTypeId dataTypeId);

    std::unique_ptr<omniruntime::vec::BaseVector> makeVarcharVector(uint64_t numValues,
        omniruntime::type::DataTypeId dataTypeId);

    std::unique_ptr<omniruntime::vec::BaseVector> makeDecimalVector(uint64_t numValues,
        omniruntime::type::DataTypeId dataTypeId);

    std::unique_ptr<omniruntime::vec::BaseVector> makeNewVector(uint64_t numValues, const ::orc::Type* baseTp,
          omniruntime::type::DataTypeId dataTypeId);

    class OmniRleDecoderV2 : public ::orc::RleDecoderV2 {
    public:
        OmniRleDecoderV2(std::unique_ptr<::orc::SeekableInputStream> input,
                         bool isSigned, ::orc::MemoryPool& pool) : ::orc::RleDecoderV2(std::move(input), isSigned, pool){
        }
        /**
        * direct read VectorBatch in next
        * @param omnivec the BaseVector to push
        * @param numValues the numValues to push
        * @param nulls the nulls bits to push
        * @param baseTp the orcType to push
        * @param omniTypeId the int* of omniType to push
        */
        void next(omniruntime::vec::BaseVector *omnivec, uint64_t numValues, 
            uint64_t *nulls, int omniTypeId);

        void next(int64_t *data, uint64_t numValues, uint64_t *nulls);

        void next(int32_t *data, uint64_t numValues, uint64_t *nulls);

        void next(int16_t *data, uint64_t numValues, uint64_t *nulls);

        void next(bool *data, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        void next(T *data, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        uint64_t nextShortRepeats(T *data, uint64_t offset, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        uint64_t nextDirect(T *data, uint64_t offset, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        uint64_t nextPatched(T *data, uint64_t offset, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        uint64_t nextDelta(T *data, uint64_t offset, uint64_t numValues, uint64_t *nulls);

        template<typename T>
        uint64_t copyDataFromBuffer(T *data, uint64_t offset, uint64_t numValues, uint64_t *nulls);

        void readLongs(int64_t *data, uint64_t offset, uint64_t len, uint64_t fbs);

        void unrolledUnpack4(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack8(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack16(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack24(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack32(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack40(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack48(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack56(int64_t *data, uint64_t offset, uint64_t len);

        void unrolledUnpack64(int64_t *data, uint64_t offset, uint64_t len);

        void plainUnpackLongs(int64_t *data, uint64_t offset, uint64_t len, uint64_t fbs);

        // Selective API. Shares run state with next(); do not call ::orc::RleDecoderV2::skip().

        // Remaining values of the current run.
        // repeated=false: values -> literals[runRead], count distinct values.
        // repeated=true:  values -> literals[0], that value repeats count times.
        struct RunSlice {
            const int64_t *values;
            uint64_t count;
            bool repeated;
        };

        // Ensure a non-empty remaining slice (advances to next run if needed).
        RunSlice currentRun();

        // n must be <= currentRun().count.
        void consume(uint64_t n)
        {
            runRead += n;
        }

        // Skip numValues values (not rows) across run boundaries.
        void skipValues(uint64_t numValues);

        // Feed run slices to visitor for rows in [0, numRows). nulls: bit set = null.
        // On return DATA has advanced by the non-null count in [0, numRows).
        template <bool hasNulls, typename Visitor>
        void readWithVisitor(const uint64_t *nulls, uint64_t numRows, Visitor &visitor)
        {
            uint64_t pos = 0;
            while (!visitor.atEnd()) {
                const uint64_t row = static_cast<uint64_t>(visitor.currentRow());

                if (row > pos) {
                    const uint64_t values = hasNulls ? CountNonNull(nulls, pos, row) : (row - pos);
                    if (values > 0) {
                        skipValues(values);
                    }
                    pos = row;
                }

                if (hasNulls && BitUtil::IsBitSet(nulls, static_cast<int32_t>(row))) {
                    visitor.processNull();
                    pos = row + 1;
                    continue;
                }

                const RunSlice slice = currentRun();
                const uint64_t k = visitor.template denseRunLength<hasNulls>(nulls, slice.count);

                visitor.processSlice(slice, k, static_cast<common::vector_size_t>(row));
                consume(k);
                pos = row + k;
            }

            if (pos < numRows) {
                const uint64_t values = hasNulls ? CountNonNull(nulls, pos, numRows) : (numRows - pos);
                if (values > 0) {
                    skipValues(values);
                }
            }
        }

        // Non-null rows in [from, to). Bit set = null.
        static uint64_t CountNonNull(const uint64_t *nulls, uint64_t from, uint64_t to)
        {
            const int32_t begin = static_cast<int32_t>(from);
            const int32_t end = static_cast<int32_t>(to);
            return static_cast<uint64_t>((end - begin) - BitUtil::CountBits(nulls, begin, end));
        }

    private:
        void ensureRun();
        void prepareRun();

        // Copied from nextXxx prologues; keep in sync with next().
        void prepareShortRepeatRun();
        void prepareDirectRun();
        void preparePatchedRun();
        void prepareDeltaRun();

        ::orc::EncodingType currentEncoding() const
        {
            return static_cast<::orc::EncodingType>((firstByte >> 6) & 0x03);
        }
    };
}

#endif