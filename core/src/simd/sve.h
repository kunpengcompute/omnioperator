#pragma once

#include <arm_sve.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

/**
 * Convert 32 bits to 32 bytes (256 bits, sve bit width of 920B machine), each byte corresponds to the bit of the input.
 *
 * @param input 32 bits. We take 32 bits so that we can use a single sve vector to process the input and store the result
 */
template <bool do_get_null = true>
inline void bits_to_bytes_sve(const uint32_t input, uint8_t* output) {
    svbool_t pg = svptrue_b8();

    // 1. Duplicate the input to a 32-bit vector
    svuint32_t v_in_32 = svdup_n_u32(input);

    // 2. Reinterpret as 8-bit. Memory layout becomes: [B0, B1, B2, B3, B0, B1, B2, B3...], only the first 4 bytes are used.
    svuint8_t v_in_8 = svreinterpret_u8_u32(v_in_32);

    /*
    The following appraoch is slower than the svdup and svreinterpret (1.16G/s vs 1.81G/s)
    uint8_t in_bytes[4] = {
        static_cast<uint8_t>(input),
        static_cast<uint8_t>(input >> 8),
        static_cast<uint8_t>(input >> 16),
        static_cast<uint8_t>(input >> 24),
    };
    svbool_t pg4 = svwhilelt_b8((uint64_t)0, (uint64_t)4);
    svuint8_t v_in_8 = svld1_u8(pg4, in_bytes);
    */

    // 3. Generate the index vector [0, 1, 2, 3, 4, 5, 6, 7..., 31]
    svuint8_t v_idx = svindex_u8(0, 1);

    // 4. Logical Shift Right by 3 (effectively dividing by 8), v_idx becomes: [0x8, 1x8, 2x8, 3x8]
    v_idx = svlsr_n_u8_x(pg, v_idx, 3);

    // 5. Table Lookup: Fetch bytes from v_in_8 using v_idx, index 0 fetches B0 (8 times), index 1 fetches B1 (8 times), etc.
    svuint8_t v_in = svtbl_u8(v_in_8, v_idx);

    // 6. Generate the mask [1, 2, 4, 8, 16, 32, 64, 128] * 4 to match BitUtil::SetBit(idx % 8) bit order, which is in memory layout as [0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000] because BitUtil::SetBit(idx % 8) is using big endian
    svuint64_t v_64 = svdup_n_u64(0x8040201008040201ULL);
    svuint8_t v_mask = svreinterpret_u8_u64(v_64);

    // 7. Isolate the bits, for each 8 bits (each byte), set 1 if > 1, else 0
    svuint8_t v_and = svand_u8_z(pg, v_in, v_mask);
    // v_gt marks lanes where the original bitmap bit is 1.
    svbool_t v_sel;
    if constexpr (do_get_null) {  // By default we output nulls
        v_sel = svcmpgt_n_u8(pg, v_and, 0);
    } else {  // When do_get_null=false we output non-null=1
        v_sel = svcmpeq_n_u8(pg, v_and, 0);
    }
    svuint8_t v_result = svdup_n_u8_z(v_sel, 1);

    // 8. Store exactly 8 bytes to the bool array
    svst1_u8(pg, output, v_result);
}

template <bool do_get_null = true>
void unpack_null_bitmap_sve(const uint8_t* nulls, uint8_t* result, size_t length) {
    size_t i = 0;
    for (; i + 4 <= length; i += 4) {  // process 32 bits (4 bytes) at a time
        uint32_t word = *reinterpret_cast<const uint32_t*>(nulls + i);
        bits_to_bytes_sve<do_get_null>(word, result + 8 * i);  // result is written to the result array in 256 bits (32 bytes) at a time
    }

    const size_t tailBytes = length - i;
    if (tailBytes > 0) {
        uint32_t tailWord = 0;
        std::memcpy(&tailWord, nulls + i, tailBytes);
        alignas(32) uint8_t tailOutput[32];
        bits_to_bytes_sve<do_get_null>(tailWord, tailOutput);
        std::memcpy(result + 8 * i, tailOutput, 8 * tailBytes);
    }
}

inline svbool_t load_4_bytes_to_pg(const uint8_t* nulls) {
    svbool_t pg;
    __asm__ __volatile__("ldr %0, [%1]" : "=Upl"(pg) : "r"(&(nulls[0])) : "memory");
    return pg;
}

/**
 * Use the null bitmap to compact the source vector into a result vector.
 *
 * @param nulls The null bitmap to use for compacting.
 * @param src The source vector to compact.
 * @param result The result vector to store the compacted values.
 * @param length The length of the source vector.
 */
template <bool has_payload = false>
int32_t compact_u64_array_using_null_bitmap_sve(const uint8_t* nulls, uint64_t* src, uint64_t* result, const int32_t length, uint64_t* payload_src = nullptr, uint64_t* payload_res = nullptr) {
    svbool_t pg = svptrue_b64();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) {  // process 32 rows at a time
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(*reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),  // get the 32 bits (4 bytes) from the null bitmap
                                 non_null_bytes);

        for (int32_t j = 0; j < 32; j += 4) {
            svuint64_t src_vec = svld1(pg, src + i + j);
            svbool_t non_null_pg = load_4_bytes_to_pg(non_null_bytes + j);
            const int32_t compacted_count = svcntp_b64(pg, non_null_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint64_t compacted = svcompact_u64(non_null_pg, src_vec);
            svst1(pg, result + o, compacted);
            if constexpr (has_payload) {
                svuint64_t payload_src_vec = svld1(pg, payload_src + i + j);
                svuint64_t payload_compacted = svcompact_u64(non_null_pg, payload_src_vec);
                svst1(pg, payload_res + o, payload_compacted);
            }
            o += compacted_count;
        }
    }

    // Tail path for non-multiple-of-4 lengths.
    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o++] = src[i];
            if constexpr (has_payload) {
                payload_res[o - 1] = payload_src[i];
            }
        }
    }
    return o;
}

template <bool has_payload = false>
int32_t compact_u64_dict_using_null_bitmap_sve(const uint8_t* nulls, const uint64_t* src, const uint32_t* indices, uint64_t* result, const int32_t length, uint64_t* payload_src = nullptr, uint64_t* payload_res = nullptr) {
    svbool_t pg = svptrue_b64();
    svbool_t pg32 = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;

    for (; i + 32 <= length; i += 32) {  // process 32 rows at a time
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(*reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),  // get the 32 bits (4 bytes) from the null bitmap
                                 non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) {  // process 8 rows at a time because indices are 32 bits (sve vector width = 256 = 32 * 8)
            svuint32_t idx_vec = svld1(pg32, indices + i + j);

            svbool_t first_pg = load_4_bytes_to_pg(non_null_bytes + j);
            svuint64_t first_idx = svunpklo_u64(idx_vec);

            svbool_t second_pg = load_4_bytes_to_pg(non_null_bytes + j + 4);
            svuint64_t second_idx = svunpkhi_u64(idx_vec);

            //# First 4 elements
            svuint64_t first_src = svld1_gather_u64index_u64(first_pg, src, first_idx);
            const int32_t first_compacted_count = svcntp_b64(pg, first_pg);
            if (first_compacted_count > 0) {
                svuint64_t first_compacted = svcompact_u64(first_pg, first_src);
                svst1(pg, result + o, first_compacted);
            }
            if constexpr (has_payload) {  // payload is sequential and doesn't need to be gathered
                svuint64_t payload_src_vec = svld1(pg, payload_src + i + j);
                svuint64_t payload_compacted = svcompact_u64(first_pg, payload_src_vec);
                svst1(pg, payload_res + o, payload_compacted);
            }
            o += first_compacted_count;

            //# Second 4 elements
            svuint64_t second_src = svld1_gather_u64index_u64(second_pg, src, second_idx);
            const int32_t second_compacted_count = svcntp_b64(pg, second_pg);
            if (second_compacted_count > 0) {
                svuint64_t second_compacted = svcompact_u64(second_pg, second_src);
                svst1(pg, result + o, second_compacted);
            }
            if constexpr (has_payload) {  // payload is sequential and doesn't need to be gathered
                svuint64_t payload_src_vec = svld1(pg, payload_src + i + j + 4);
                svuint64_t payload_compacted = svcompact_u64(second_pg, payload_src_vec);
                svst1(pg, payload_res + o, payload_compacted);
            }
            o += second_compacted_count;
        }
    }

    // Tail path for non-multiple-of-4 lengths.
    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o++] = src[indices[i]];
            if constexpr (has_payload) {
                payload_res[o - 1] = payload_src[i];
            }
        }
    }
    return o;
}

// Generate a sequential u64 payload array: out[i] = base_row + i.
inline void fill_u64_sequential_sve(uint64_t base_row, uint64_t* out, int32_t length) {
    const svbool_t pg_all = svptrue_b64();
    int32_t i = 0;
    while (i < length) {
        const svbool_t pg = svwhilelt_b64_s32(i, length);
        const svuint64_t idx = svindex_u64(base_row + static_cast<uint64_t>(i), 1);
        svst1(pg, out + i, idx);
        i += static_cast<int32_t>(svcntp_b64(pg_all, pg));
    }
}

template <typename KeyType, typename KeyAccessor>
inline int32_t compact_probe_keys_scalar_with_nulls(const uint8_t* nulls, const int32_t start_row, const int32_t num_rows, KeyType* key_res, uint64_t* payload_res, KeyAccessor&& key_accessor) {
    int32_t o = 0;
    for (int32_t i = 0; i < num_rows; ++i) {
        const int32_t row = start_row + i;
        const bool isNull = ((nulls[static_cast<size_t>(row) >> 3] >> (row & 7)) & 0x1u) != 0u;
        if (isNull) {
            continue;
        }
        key_res[static_cast<size_t>(o)] = static_cast<KeyType>(key_accessor(row));
        payload_res[static_cast<size_t>(o)] = static_cast<uint64_t>(row);
        ++o;
    }
    return o;
}

template <typename KeyType, typename KeyAccessor>
inline int32_t compact_probe_keys_scalar_with_nulls_u32_payload(
    const uint8_t* nulls,
    const int32_t start_row,
    const int32_t num_rows,
    KeyType* key_res,
    uint32_t* payload_res,
    KeyAccessor&& key_accessor)
{
    int32_t o = 0;
    for (int32_t i = 0; i < num_rows; ++i) {
        const int32_t row = start_row + i;
        const bool isNull = ((nulls[static_cast<size_t>(row) >> 3] >> (row & 7)) & 0x1u) != 0u;
        if (isNull) {
            continue;
        }
        key_res[static_cast<size_t>(o)] = static_cast<KeyType>(key_accessor(row));
        payload_res[static_cast<size_t>(o)] = static_cast<uint32_t>(row);
        ++o;
    }
    return o;
}

// Compaction helpers for join probe: generate sequential payload row indices (base_row + i)
// instead of reading payload_src/payload_res arrays.
inline int32_t compact_u64_array_using_null_bitmap_sve_gen_payload(const uint8_t* nulls, const uint64_t* src, uint64_t* result, const int32_t length, const uint64_t base_row, uint64_t* payload_res) {
    svbool_t pg = svptrue_b64();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) {
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(*reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)), non_null_bytes);

        for (int32_t j = 0; j < 32; j += 4) {
            svuint64_t src_vec = svld1(pg, src + i + j);
            svbool_t non_null_pg = load_4_bytes_to_pg(non_null_bytes + j);
            const int32_t compacted_count = svcntp_b64(pg, non_null_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint64_t compacted = svcompact_u64(non_null_pg, src_vec);
            svuint64_t row_idx = svindex_u64(base_row + static_cast<uint64_t>(i + j), 1);
            svuint64_t row_idx_compacted = svcompact_u64(non_null_pg, row_idx);

            svbool_t store_mask = svwhilelt_b64_s64(0, compacted_count);
            svst1(store_mask, result + o, compacted);
            svst1(store_mask, payload_res + o, row_idx_compacted);
            o += compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o] = src[i];
            payload_res[o] = base_row + static_cast<uint64_t>(i);
            ++o;
        }
    }
    return o;
}

inline int32_t compact_u32_array_using_null_bitmap_sve_gen_payload(
    const uint8_t* nulls,
    const uint32_t* src,
    uint32_t* result,
    const int32_t length,
    const uint32_t base_row,
    uint32_t* payload_res)
{
    svbool_t pg = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) {
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(
            *reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),
            non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) {
            alignas(32) uint32_t flags32[8];
            for (int32_t k = 0; k < 8; ++k) {
                flags32[k] = static_cast<uint32_t>(non_null_bytes[j + k]);
            }
            svuint32_t flag_vec = svld1(pg, flags32);
            svbool_t keep_pg = svcmpne_n_u32(pg, flag_vec, 0);
            const int32_t compacted_count = svcntp_b32(pg, keep_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint32_t src_vec = svld1(pg, src + i + j);
            svuint32_t compacted = svcompact_u32(keep_pg, src_vec);
            svuint32_t row_idx = svindex_u32(base_row + static_cast<uint32_t>(i + j), 1);
            svuint32_t row_idx_compacted = svcompact_u32(keep_pg, row_idx);

            svbool_t store_mask = svwhilelt_b32_s32(0, compacted_count);
            svst1_u32(store_mask, result + o, compacted);
            svst1_u32(store_mask, payload_res + o, row_idx_compacted);
            o += compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o] = src[i];
            payload_res[o] = base_row + static_cast<uint32_t>(i);
            ++o;
        }
    }
    return o;
}

inline int32_t compact_u32_dict_using_null_bitmap_sve_gen_payload(
    const uint8_t* nulls,
    const uint32_t* src,
    const uint32_t* indices,
    uint32_t* result,
    const int32_t length,
    const uint32_t base_row,
    uint32_t* payload_res)
{
    svbool_t pg = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) {
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(
            *reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),
            non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) {
            alignas(32) uint32_t flags32[8];
            for (int32_t k = 0; k < 8; ++k) {
                flags32[k] = static_cast<uint32_t>(non_null_bytes[j + k]);
            }
            svuint32_t flag_vec = svld1(pg, flags32);
            svbool_t keep_pg = svcmpne_n_u32(pg, flag_vec, 0);
            const int32_t compacted_count = svcntp_b32(pg, keep_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint32_t idx_vec = svld1(pg, indices + i + j);
            svuint32_t gathered = svld1_gather_u32index_u32(keep_pg, src, idx_vec);
            svuint32_t compacted = svcompact_u32(keep_pg, gathered);
            svuint32_t row_idx = svindex_u32(base_row + static_cast<uint32_t>(i + j), 1);
            svuint32_t row_idx_compacted = svcompact_u32(keep_pg, row_idx);

            svbool_t store_mask = svwhilelt_b32_s32(0, compacted_count);
            svst1_u32(store_mask, result + o, compacted);
            svst1_u32(store_mask, payload_res + o, row_idx_compacted);
            o += compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o] = src[indices[i]];
            payload_res[o] = base_row + static_cast<uint32_t>(i);
            ++o;
        }
    }
    return o;
}

inline int32_t compact_u64_dict_using_null_bitmap_sve_gen_payload(const uint8_t* nulls, const uint64_t* src, const uint32_t* indices, uint64_t* result, const int32_t length, const uint64_t base_row, uint64_t* payload_res) {
    svbool_t pg = svptrue_b64();
    svbool_t pg32 = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;

    for (; i + 32 <= length; i += 32) {
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(*reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)), non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) {
            svuint32_t idx_vec = svld1(pg32, indices + i + j);

            svbool_t first_pg = load_4_bytes_to_pg(non_null_bytes + j);
            svuint64_t first_idx = svunpklo_u64(idx_vec);
            svuint64_t first_src = svld1_gather_u64index_u64(first_pg, src, first_idx);
            const int32_t first_compacted_count = svcntp_b64(pg, first_pg);
            if (first_compacted_count > 0) {
                svuint64_t first_compacted = svcompact_u64(first_pg, first_src);
                svuint64_t first_row_idx = svindex_u64(base_row + static_cast<uint64_t>(i + j), 1);
                svuint64_t first_row_idx_compacted = svcompact_u64(first_pg, first_row_idx);
                svbool_t store_mask = svwhilelt_b64_s64(0, first_compacted_count);
                svst1(store_mask, result + o, first_compacted);
                svst1(store_mask, payload_res + o, first_row_idx_compacted);
            }
            o += first_compacted_count;

            svbool_t second_pg = load_4_bytes_to_pg(non_null_bytes + j + 4);
            svuint64_t second_idx = svunpkhi_u64(idx_vec);
            svuint64_t second_src = svld1_gather_u64index_u64(second_pg, src, second_idx);
            const int32_t second_compacted_count = svcntp_b64(pg, second_pg);
            if (second_compacted_count > 0) {
                svuint64_t second_compacted = svcompact_u64(second_pg, second_src);
                svuint64_t second_row_idx = svindex_u64(base_row + static_cast<uint64_t>(i + j + 4), 1);
                svuint64_t second_row_idx_compacted = svcompact_u64(second_pg, second_row_idx);
                svbool_t store_mask = svwhilelt_b64_s64(0, second_compacted_count);
                svst1(store_mask, result + o, second_compacted);
                svst1(store_mask, payload_res + o, second_row_idx_compacted);
            }
            o += second_compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o] = src[indices[i]];
            payload_res[o] = base_row + static_cast<uint64_t>(i);
            ++o;
        }
    }
    return o;
}

/**
 * Returns true if every bit in [0, length_bits) of the null bitmap is 0, else false.
 *
 * This variant is specialized for 256-bit SVE hardware: process exactly 32-byte chunks
 * without per-iteration remainder calculations. Leftover bytes and leftover bits are
 * checked one by one.
 */
inline bool is_null_bitmap_all_zero_chunk256(const uint8_t* nulls, size_t length_bits) {
    if (length_bits == 0) {
        return true;
    }

    // SVE-256b only: require 256-bit SVE (svcntb() == 32)
    if (svcntb() != 32) {
        throw std::runtime_error("is_null_bitmap_all_zero_chunk256 only supports SVE 256b environments");
    }

    const size_t full_bytes = length_bits >> 3;
    const unsigned tail_bits = static_cast<unsigned>(length_bits & 7u);

    constexpr size_t kChunkBytes = 32;
    const size_t chunk_count = full_bytes / kChunkBytes;
    const size_t chunked_bytes = chunk_count * kChunkBytes;

    const svbool_t pg = svptrue_b8();
    for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
        const svuint8_t v = svld1_u8(pg, nulls + (chunk * kChunkBytes));
        if (svmaxv_u8(pg, v) != 0) {
            return false;
        }
    }

    for (size_t i = chunked_bytes; i < full_bytes; ++i) {
        if (nulls[i] != 0) {
            return false;
        }
    }

    if (tail_bits != 0) {
        const uint8_t tail_byte = nulls[full_bytes];
        for (unsigned bit = 0; bit < tail_bits; ++bit) {
            if (((tail_byte >> bit) & 0x1u) != 0u) {
                return false;
            }
        }
    }

    return true;
}


/**
 * 32-bit variant of compact_u64_array_using_null_bitmap_sve.
 *
 * Compacts non-null rows from a u32 array into a dense output array.
 * When has_payload=true, compacts a parallel u32 payload array in lock-step.
 */
template <bool has_payload = false>
int32_t compact_u32_array_using_null_bitmap_sve(
    const uint8_t* nulls,
    uint32_t* src,
    uint32_t* result,
    const int32_t length,
    uint32_t* payload_src = nullptr,
    uint32_t* payload_res = nullptr
) {
    svbool_t pg = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) { // process 32 rows at a time
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(
            *reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),
            non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) { // 8 rows = 256 bits of u32
            alignas(32) uint32_t flags32[8];
            for (int32_t k = 0; k < 8; ++k) {
                flags32[k] = static_cast<uint32_t>(non_null_bytes[j + k]);
            }
            svuint32_t flag_vec = svld1(pg, flags32);
            svbool_t keep_pg = svcmpne_n_u32(pg, flag_vec, 0);
            const int32_t compacted_count = svcntp_b32(pg, keep_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint32_t src_vec = svld1(pg, src + i + j);
            svuint32_t compacted = svcompact_u32(keep_pg, src_vec);
            svbool_t store_mask = svwhilelt_b32_s32(0, compacted_count);
            svst1_u32(store_mask, result + o, compacted);

            if constexpr (has_payload) {
                svuint32_t payload_vec = svld1(pg, payload_src + i + j);
                svuint32_t payload_compacted = svcompact_u32(keep_pg, payload_vec);
                svst1_u32(store_mask, payload_res + o, payload_compacted);
            }
            o += compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o++] = src[i];
            if constexpr (has_payload) {
                payload_res[o - 1] = payload_src[i];
            }
        }
    }
    return o;
}

/**
 * 32-bit variant of compact_u64_dict_using_null_bitmap_sve.
 *
 * Compacts non-null rows from a dictionary-encoded u32 vector into a dense output array.
 * When has_payload=true, compacts a parallel u32 payload array in lock-step.
 */
template <bool has_payload = false>
int32_t compact_u32_dict_using_null_bitmap_sve(
    const uint8_t* nulls,
    const uint32_t* src,
    const uint32_t* indices,
    uint32_t* result,
    const int32_t length,
    uint32_t* payload_src = nullptr,
    uint32_t* payload_res = nullptr
) {
    svbool_t pg = svptrue_b32();

    int32_t o = 0;
    int32_t i = 0;
    for (; i + 32 <= length; i += 32) { // process 32 rows at a time
        alignas(32) uint8_t non_null_bytes[32];
        bits_to_bytes_sve<false>(
            *reinterpret_cast<const uint32_t*>(nulls + (static_cast<size_t>(i) >> 3)),
            non_null_bytes);

        for (int32_t j = 0; j < 32; j += 8) {
            alignas(32) uint32_t flags32[8];
            for (int32_t k = 0; k < 8; ++k) {
                flags32[k] = static_cast<uint32_t>(non_null_bytes[j + k]);
            }
            svuint32_t flag_vec = svld1(pg, flags32);
            svbool_t keep_pg = svcmpne_n_u32(pg, flag_vec, 0);
            const int32_t compacted_count = svcntp_b32(pg, keep_pg);
            if (compacted_count == 0) {
                continue;
            }

            svuint32_t idx_vec = svld1(pg, indices + i + j);
            svuint32_t gathered = svld1_gather_u32index_u32(keep_pg, src, idx_vec);
            svuint32_t compacted = svcompact_u32(keep_pg, gathered);
            svbool_t store_mask = svwhilelt_b32_s32(0, compacted_count);
            svst1_u32(store_mask, result + o, compacted);

            if constexpr (has_payload) {
                svuint32_t payload_vec = svld1(pg, payload_src + i + j);
                svuint32_t payload_compacted = svcompact_u32(keep_pg, payload_vec);
                svst1_u32(store_mask, payload_res + o, payload_compacted);
            }
            o += compacted_count;
        }
    }

    for (; i < length; ++i) {
        const bool isNull = ((nulls[static_cast<size_t>(i) >> 3] >> (i & 7)) & 0x1u) != 0u;
        if (!isNull) {
            result[o++] = src[indices[i]];
            if constexpr (has_payload) {
                payload_res[o - 1] = payload_src[i];
            }
        }
    }
    return o;
}

inline void gather_u64_dict_values_sve(
    const uint64_t* src,
    const uint32_t* indices,
    uint64_t* result,
    const int32_t length
) {
    const svbool_t pg64 = svptrue_b64();
    const svbool_t pg32 = svptrue_b32();
    int32_t i = 0;
    for (; i + 8 <= length; i += 8) {
        const svuint32_t idx_vec = svld1(pg32, indices + i);
        const svuint64_t first_idx = svunpklo_u64(idx_vec);
        const svuint64_t second_idx = svunpkhi_u64(idx_vec);
        const svuint64_t first_gathered = svld1_gather_u64index_u64(pg64, src, first_idx);
        const svuint64_t second_gathered = svld1_gather_u64index_u64(pg64, src, second_idx);
        svst1(pg64, result + i, first_gathered);
        svst1(pg64, result + i + 4, second_gathered);
    }
    for (; i < length; ++i) {
        result[i] = src[indices[i]];
    }
}

inline void gather_u32_dict_values_sve(
    const uint32_t* src,
    const uint32_t* indices,
    uint32_t* result,
    const int32_t length
) {
    const svbool_t pg32 = svptrue_b32();
    int32_t i = 0;
    for (; i + 8 <= length; i += 8) {
        const svuint32_t idx_vec = svld1(pg32, indices + i);
        const svuint32_t gathered = svld1_gather_u32index_u32(pg32, src, idx_vec);
        svst1_u32(pg32, result + i, gathered);
    }
    for (; i < length; ++i) {
        result[i] = src[indices[i]];
    }
}
