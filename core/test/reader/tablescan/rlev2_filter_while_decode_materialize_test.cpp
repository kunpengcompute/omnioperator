/**
 * Decoder-level regression for B2b filter-while-decode (integer RLEv2).
 *
 * Does NOT depend on OMNI_SCAN_DECODE_STATS. Proves:
 *   1) fixture bytes are the intended RLEv2 encoding (first-byte high 2 bits);
 *   2) readWithVisitor + IntColumnVisitor<kExtract=false> keeps survivors in
 *      outputRows only — numValues() stays 0 (no value materialization).
 *
 * Byte streams are the unsigned examples from the ORC RLEv2 specification.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "orc/OrcFile.hh"
#include "orc/io/InputStream.hh"
#include "reader/common/Filter.h"
#include "reader/orc/ColumnVisitor.h"
#include "reader/orc/OmniRLEv2.hh"

namespace {

using omniruntime::reader::IntColumnVisitor;
using omniruntime::reader::OmniRleDecoderV2;

// ORC RLEv2 encoding type in the high 2 bits of the first byte.
constexpr uint8_t kEncShortRepeat = 0;
constexpr uint8_t kEncDirect = 1;
constexpr uint8_t kEncPatchedBase = 2;
constexpr uint8_t kEncDelta = 3;

struct EncodingCase {
    const char *name;
    uint8_t expectedEncoding;
    std::vector<unsigned char> bytes;
    uint64_t numRows;
    int64_t filterLower;
    int64_t filterUpper;
    size_t expectedHits;
};

// Spec fixtures (unsigned). See ORC RLEv2 Short Repeat / Direct / Patched Base / Delta.
const EncodingCase kCases[] = {
    {"SHORT_REPEAT",
     kEncShortRepeat,
     {0x0a, 0x27, 0x10},
     5,
     /*filter*/ 10000,
     10000,
     /*hits: all equal, accept all — still must not materialize values*/ 5},
    {"DIRECT",
     kEncDirect,
     {0x5e, 0x03, 0x5c, 0xa1, 0xab, 0x1e, 0xde, 0xad, 0xbe, 0xef},
     4,
     /*keep only 23713*/ 23713,
     23713,
     1},
    {"PATCHED_BASE",
     kEncPatchedBase,
     {0x8e, 0x13, 0x2b, 0x21, 0x07, 0xd0, 0x1e, 0x00, 0x14, 0x70, 0x28, 0x32, 0x3c, 0x46, 0x50, 0x5a,
      0x64, 0x6e, 0x78, 0x82, 0x8c, 0x96, 0xa0, 0xaa, 0xb4, 0xbe, 0xfc, 0xe8},
     20,
     /*keep outlier 1000000*/ 1000000,
     1000000,
     1},
    {"DELTA",
     kEncDelta,
     {0xc6, 0x09, 0x02, 0x02, 0x22, 0x42, 0x42, 0x46},
     10,
     /*keep only 29*/ 29,
     29,
     1},
};

class RleV2FilterWhileDecodeMaterializeTest : public ::testing::TestWithParam<EncodingCase> {};

TEST_P(RleV2FilterWhileDecodeMaterializeTest, KExtractFalseDoesNotMaterialize)
{
    const EncodingCase &tc = GetParam();
    ASSERT_FALSE(tc.bytes.empty());
    ASSERT_EQ(static_cast<uint8_t>((tc.bytes[0] >> 6) & 0x03), tc.expectedEncoding)
        << "fixture first byte is not " << tc.name;

    auto input = std::make_unique<::orc::SeekableArrayInputStream>(
        reinterpret_cast<const char *>(tc.bytes.data()), static_cast<uint64_t>(tc.bytes.size()));
    OmniRleDecoderV2 decoder(std::move(input), /*isSigned=*/false, *::orc::getDefaultPool());

    std::vector<common::vector_size_t> rowIds(static_cast<size_t>(tc.numRows));
    std::iota(rowIds.begin(), rowIds.end(), 0);
    common::RowSet activeRows(rowIds.data(), rowIds.size());

    ::common::BigintRange filter(tc.filterLower, tc.filterUpper, /*nullAllowed=*/false);
    std::vector<common::vector_size_t> outputRows;
    // kExtract=false: out / outNulls unused; pass nullptr so any write would crash.
    IntColumnVisitor<int64_t, ::common::BigintRange, /*kExtract=*/false> visitor(
        &filter, activeRows, &outputRows, /*out=*/nullptr, /*outNulls=*/nullptr, /*stats=*/nullptr);

    decoder.readWithVisitor</*hasNulls=*/false>(/*nulls=*/nullptr, tc.numRows, visitor);

    EXPECT_EQ(visitor.numValues(), 0u) << tc.name << ": values must not be materialized";
    EXPECT_EQ(outputRows.size(), tc.expectedHits) << tc.name << ": survivor rows only";
}

INSTANTIATE_TEST_CASE_P(FourRleV2Encodings, RleV2FilterWhileDecodeMaterializeTest,
                        ::testing::ValuesIn(kCases),
                        [](const ::testing::TestParamInfo<EncodingCase> &info) {
                            return std::string(info.param.name);
                        });

} // namespace
