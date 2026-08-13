#ifndef OMNI_RUNTIME_OPERATOR_HASHMAP_SVEHT32_AOS_H
#define OMNI_RUNTIME_OPERATOR_HASHMAP_SVEHT32_AOS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <arm_sve.h>

#include "operator/hashmap/base_hash_map.h"

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

namespace omniruntime {
namespace op {
namespace hashmap {

template <typename KeyType = uint32_t, typename ValueType = uint32_t, int HT_GROUP_SIZE = 64>
class SveHashTable32 {
public:
    static constexpr int LANE_COUNT = 8;

    static_assert(sizeof(KeyType) == sizeof(uint32_t), "KeyType must be 32-bit");
    static_assert(sizeof(ValueType) == sizeof(uint32_t), "ValueType must be 32-bit");
    using ResultType = InsertResult<ValueType>;

    struct HashParams32 {
        uint32_t bucketsMask = 0;
        int shiftRight = 0;
    };

    struct alignas(8) Slot {
        uint32_t key;
        uint32_t val;
    };

    static void ValidateLaneCount()
    {
        if (unlikely(svcntw() != static_cast<uint64_t>(LANE_COUNT))) {
            throw std::runtime_error("SveHashTable32 requires fixed 256-bit SVE (8 lanes for u32)");
        }
    }

    explicit SveHashTable32(uint8_t initDegree = defaultDegreeSize)
    {
        ValidateLaneCount();
        InitTableAndHashParam(1ULL << initDegree);
    }

    explicit SveHashTable32(uint64_t initCapacity)
    {
        ValidateLaneCount();
        InitTableAndHashParam(initCapacity);
    }

    inline const HashParams32 &get_hash_param() const
    {
        return hashParam_;
    }

    inline uint32_t *get_slots()
    {
        return reinterpret_cast<uint32_t *>(slots_.data());
    }

    size_t GetElementsSize() const
    {
        return elementsSize_;
    }

    size_t GetCapacity() const
    {
        return capacity_;
    }

    inline ResultType EmplaceNotNullKey(KeyType key)
    {
        if (unlikely(static_cast<uint32_t>(key) == kEmptyKey)) {
            return EmplaceEmptyKey();
        }

        return EmplaceRegularKey(key);
    }

    template <bool WithPayload, bool StoreKey = true, bool NeedEmptyProcessing = true>
    int probe_scalar(
        const uint32_t *query_keys,
        const uint32_t *query_payloads,
        size_t num_keys,
        uint32_t *output_keys,
        uint32_t *output_values,
        uint32_t *output_payloads) const
    {
        int output_count = 0;
        for (size_t i = 0; i < num_keys; ++i) {
            const uint32_t key = query_keys[i];
            const uint32_t payload = WithPayload ? query_payloads[i] : 0;
            const uint32_t hash = get_single_hash_value(key);
            probe_single_key<WithPayload, StoreKey, NeedEmptyProcessing>(
                key, payload, hash, output_keys, output_values, output_payloads, output_count);
        }
        return output_count;
    }

    template <bool WithPayload, bool StoreKey = true, bool NeedEmptyProcessing = false>
    int probe_sve(
        const uint32_t *query_keys,
        const uint32_t *query_payloads,
        size_t num_keys,
        uint32_t *output_keys,
        uint32_t *output_values,
        uint32_t *output_payloads) const
    {
        int total_output_count = 0;
        if (num_keys < static_cast<size_t>(HT_GROUP_SIZE)) {
            return probe_scalar<WithPayload, StoreKey, NeedEmptyProcessing>(
                query_keys, query_payloads, num_keys, output_keys, output_values, output_payloads);
        }

        svbool_t pg = svptrue_b32();
        const uint32_t *slot_base = get_slots();

        for (size_t group_start = 0; group_start < num_keys; group_start += HT_GROUP_SIZE) {
            const size_t group_size = std::min(static_cast<size_t>(HT_GROUP_SIZE), num_keys - group_start);

            uint32_t hashs[HT_GROUP_SIZE];
            for (size_t i = 0; i < group_size; i += LANE_COUNT) {
                const size_t remaining = std::min(static_cast<size_t>(LANE_COUNT), group_size - i);
                if (likely(remaining == static_cast<size_t>(LANE_COUNT))) {
                    compute_hash_u32(&query_keys[group_start + i], &hashs[i]);
                    prefetch_slots_u32<SV_PLDL1KEEP>(&hashs[i]);
                } else {
                    for (size_t r = 0; r < remaining; ++r) {
                        const uint32_t h = get_single_hash_value(query_keys[group_start + i + r]);
                        hashs[i + r] = h;
                        __builtin_prefetch(&slots_[h], 0, 3);
                    }
                }
            }

            svbool_t inv_mask = svptrue_b32();
            svbool_t active_mask = svpfalse_b();
            svbool_t out_mask = svpfalse_b();

            svuint32_t probe_key = svdup_n_u32(0);
            svuint32_t probe_payload = svdup_n_u32(0);
            svuint32_t tab_val = svdup_n_u32(0);
            svuint32_t h = svdup_n_u32(0);

            int i = 0;
            int o = 0;
            int inc_i = 0;
            int inc_o = 0;
            int iter = 0;

            while (likely(i + LANE_COUNT <= static_cast<int>(group_size))) {
                ++iter;

                // Step 1: Load new keys and payloads
                svuint32_t newk = svld1_u32(inv_mask, &query_keys[group_start + static_cast<size_t>(i)]);
                probe_key = svsel_u32(inv_mask, newk, probe_key);
                if constexpr (WithPayload) {
                    svuint32_t newp = svld1_u32(inv_mask, &query_payloads[group_start + static_cast<size_t>(i)]);
                    probe_payload = svsel_u32(inv_mask, newp, probe_payload);
                }
                // Empty key processing
                svbool_t probe_empty_key_mask;
                if constexpr (NeedEmptyProcessing) {
                    probe_empty_key_mask = svcmpeq_n_u32(pg, probe_key, kEmptyKey);
                }

                // Step 2: Update hash
                h = svadd_n_u32_z(active_mask, h, 1);
                svuint32_t new_h = svld1_u32(inv_mask, &hashs[static_cast<size_t>(i)]);
                h = svsel_u32(inv_mask, new_h, h);
                h = svand_u32_z(pg, h, svdup_n_u32(hashParam_.bucketsMask));

                // Step 3: Gather table key and value
                svuint32_t byte_off = svlsl_n_u32_z(pg, h, 3);
                svuint32_t val_off = svadd_n_u32_z(pg, byte_off, sizeof(uint32_t));

                svuint32_t tab_key = svld1_gather_u32offset_u32(pg, slot_base, byte_off);
                svbool_t empty_mask = svcmpeq_n_u32(pg, tab_key, kEmptyKey);
                svbool_t nonempty_mask = svnot_b_z(pg, empty_mask);
                tab_val = svld1_gather_u32offset_u32(nonempty_mask, slot_base, val_off); // TODO: whether this is worth it? gather table value after getting the nonempty_mask?

                // Step 4: Compare
                out_mask = svcmpeq_u32(nonempty_mask, tab_key, probe_key);
                inv_mask = svorr_b_z(pg, empty_mask, out_mask);
                if constexpr (NeedEmptyProcessing) {
                    inv_mask = svorr_b_z(pg, inv_mask, probe_empty_key_mask);
                }

                // Step 5: Counts
                inc_i = svcntp_b32(pg, inv_mask);
                inc_o = svcntp_b32(pg, out_mask);

                // Empty key processing
                int inc_empty_o = 0;
                if constexpr (NeedEmptyProcessing) {
                    inc_empty_o = hasEmptyKeyValue_ ? svcntp_b32(pg, probe_empty_key_mask) : 0;
                }

                // Step 6: Store
                if (likely(inc_o > 0)) {
                    svuint32_t compacted_vals = svcompact_u32(out_mask, tab_val);
                    svbool_t store_mask = svwhilelt_b32_s32(0, inc_o);
                    svst1_u32(store_mask, &output_values[total_output_count + o], compacted_vals);
                    if constexpr (StoreKey) {
                        svuint32_t compacted_keys = svcompact_u32(out_mask, probe_key);
                        svst1_u32(store_mask, &output_keys[total_output_count + o], compacted_keys);
                    }
                    if constexpr (WithPayload) {
                        svuint32_t compacted_payloads = svcompact_u32(out_mask, probe_payload);
                        svst1_u32(store_mask, &output_payloads[total_output_count + o], compacted_payloads);
                    }
                }

                // Empty key processing
                if constexpr (NeedEmptyProcessing) {
                    if (unlikely(inc_empty_o > 0)) {
                        svbool_t store_mask = svwhilelt_b32_s32(0, inc_empty_o);
                        svuint32_t empty_vals = svdup_n_u32(static_cast<uint32_t>(emptyKeyValue_));
                        svst1_u32(store_mask, &output_values[total_output_count + o + inc_o], empty_vals);
                        if constexpr (StoreKey) {
                            svuint32_t empty_keys = svdup_n_u32(kEmptyKey);
                            svst1_u32(store_mask, &output_keys[total_output_count + o + inc_o], empty_keys);
                        }
                        if constexpr (WithPayload) {
                            svuint32_t compacted_payloads = svcompact_u32(probe_empty_key_mask, probe_payload);
                            svst1_u32(store_mask, &output_payloads[total_output_count + o + inc_o], compacted_payloads);
                        }
                        o += inc_empty_o;
                    }
                }

                // Step 7: Compact
                if (inc_i == LANE_COUNT) {
                    inv_mask = svptrue_b32();
                    active_mask = svpfalse_b();
                } else {
                    active_mask = svnot_b_z(pg, inv_mask);
                    h = svcompact_u32(active_mask, h);
                    probe_key = svcompact_u32(active_mask, probe_key);
                    if constexpr (WithPayload) {
                        probe_payload = svcompact_u32(active_mask, probe_payload);
                    }
                    active_mask = svwhilelt_b32_s32(0, LANE_COUNT - inc_i);
                    inv_mask = svnot_b_z(pg, active_mask);
                }

                i += inc_i;
                o += inc_o;
            }

            const int num_rem_sve_key = iter > 0 ? LANE_COUNT - inc_i : 0;
            if (num_rem_sve_key > 0) {
                uint32_t remaining_keys[LANE_COUNT];
                uint32_t remaining_payloads[LANE_COUNT];
                uint32_t remaining_hashes[LANE_COUNT];

                svst1_u32(pg, remaining_keys, probe_key);
                svst1_u32(pg, remaining_hashes, h);
                if constexpr (WithPayload) {
                    svst1_u32(pg, remaining_payloads, probe_payload);
                }

                for (int k = 0; k < num_rem_sve_key; ++k) {
                    const uint32_t key = remaining_keys[k];
                    const uint32_t hash = remaining_hashes[k];
                    const uint32_t payload = WithPayload ? remaining_payloads[k] : 0;
                    probe_single_key<WithPayload, StoreKey, NeedEmptyProcessing>(key, payload, hash, StoreKey ? output_keys + total_output_count : nullptr, output_values + total_output_count, WithPayload ? output_payloads + total_output_count : nullptr, o);
                }
            }

            for (int j = i + num_rem_sve_key; j < static_cast<int>(group_size); ++j) {
                const uint32_t key = query_keys[group_start + static_cast<size_t>(j)];
                const uint32_t payload = WithPayload ? query_payloads[group_start + static_cast<size_t>(j)] : 0;
                const uint32_t hash = hashs[static_cast<size_t>(j)];
                probe_single_key<WithPayload, StoreKey, NeedEmptyProcessing>(key, payload, hash, StoreKey ? output_keys + total_output_count : nullptr, output_values + total_output_count, WithPayload ? output_payloads + total_output_count : nullptr, o);
            }

            total_output_count += o;
        }

        return total_output_count;
    }

    inline void build_scalar(const uint32_t *build_keys, const uint32_t *build_values, size_t num_keys) {
        // First, check if there is any empty key and assign it to `has_empty_key`, and emplace empty key
        bool has_empty_key = false;
        for (size_t i = 0; i < num_keys; ++i) {
            if (unlikely(build_keys[i] == kEmptyKey)) {
                has_empty_key = true;
                hasEmptyKeyValue_ = true;
                emptyKeyValue_ = static_cast<ValueType>(build_values[i]);
            }
        }

        // If there is no empty key, build the table normally
        if (!has_empty_key) {
            for (size_t i = 0; i < num_keys; ++i) {
                auto ret = EmplaceRegularKey(static_cast<KeyType>(build_keys[i]));
                ret.SetValue(static_cast<ValueType>(build_values[i]));
            }
            return;
        }

        // If there is empty key, build the table with empty key handling
        for (size_t i = 0; i < num_keys; ++i) {
            if (unlikely(build_keys[i] == kEmptyKey)) {
                continue;
            }
            auto ret = EmplaceRegularKey(static_cast<KeyType>(build_keys[i]));
            ret.SetValue(static_cast<ValueType>(build_values[i]));
        }
    }

    inline bool HasEmptyKeyValue() const
    {
        return hasEmptyKeyValue_;
    }

    ResultType FindValueFromHashmap(KeyType key) const
    {
        return FindMatchPosition(key);
    }

    ResultType FindMatchPosition(KeyType key) const
    {
        if (unlikely(static_cast<uint32_t>(key) == kEmptyKey)) {
            if (hasEmptyKeyValue_) {
                return ResultType(const_cast<ValueType &>(emptyKeyValue_), false);
            }
            return ResultType(const_cast<ValueType &>(nullValue_), false);
        }

        const uint32_t hash = get_single_hash_value(static_cast<uint32_t>(key));
        bool inserted = false;
        const uint32_t pos = FindPosition(static_cast<uint32_t>(key), hash, inserted);
        if (inserted) {
            return ResultType(const_cast<ValueType &>(nullValue_), false);
        }
        return ResultType(const_cast<ValueType &>(slots_[pos].val), false);
    }

private:
    static constexpr uint32_t kFib32HashFactor = 0x9e3779b9u;

    static constexpr uint32_t kEmptyKey = std::numeric_limits<uint32_t>::max();
    static constexpr double maxLoadFactor = 0.75;
    static constexpr uint8_t defaultDegreeSize = 15;

    HashParams32 hashParam_ {};
    ValueType nullValue_ = static_cast<ValueType>(0);
    uint64_t rehashThreshold_ = 0;
    size_t elementsSize_ = 0;
    uint64_t capacity_ = 0;
    bool hasEmptyKeyValue_ = false;
    ValueType emptyKeyValue_ {};

    std::vector<Slot> slots_;

    static uint64_t NormalizeCapacity(uint64_t requestedCapacity)
    {
        uint64_t normalized = std::max<uint64_t>(2, requestedCapacity);
        if ((normalized & (normalized - 1)) == 0) {
            return normalized;
        }
        int msbIndex = 63 - __builtin_clzll(normalized);
        return 1ULL << (msbIndex + 1);
    }

    static HashParams32 BuildHashParamForCapacity(uint64_t tableCapacity)
    {
        HashParams32 param {};
        param.bucketsMask = static_cast<uint32_t>(tableCapacity - 1);
        int log2Capacity = 63 - __builtin_clzll(tableCapacity);
        param.shiftRight = std::max(0, 32 - log2Capacity);
        return param;
    }

    static uint64_t ComputeRehashThreshold(uint64_t tableCapacity)
    {
        return static_cast<uint64_t>(std::ceil(static_cast<double>(tableCapacity) * maxLoadFactor));
    }

    void InitTableAndHashParam(uint64_t requestedCapacity)
    {
        capacity_ = NormalizeCapacity(requestedCapacity);
        if (capacity_ > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("SveHashTable32 capacity exceeds uint32_t range");
        }

        slots_.assign(capacity_, Slot {kEmptyKey, 0});
        hashParam_ = BuildHashParamForCapacity(capacity_);
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    bool NeedRehash() const
    {
        return (elementsSize_ + 1) > rehashThreshold_;
    }

    inline static uint32_t GetHashValueForParam(uint32_t key, const HashParams32 &param)
    {
        return (key * kFib32HashFactor) >> param.shiftRight;
    }

    inline uint32_t get_single_hash_value(uint32_t key) const
    {
        return GetHashValueForParam(key, hashParam_);
    }

    inline ResultType EmplaceEmptyKey()
    {
        const bool inserted = !hasEmptyKeyValue_;
        hasEmptyKeyValue_ = true;
        return ResultType(emptyKeyValue_, inserted);
    }

    inline ResultType EmplaceRegularKey(KeyType key)
    {
        if (unlikely(static_cast<uint32_t>(key) == kEmptyKey)) {
            throw std::runtime_error("SveHashTable32: INT32_MAX is reserved as the empty-slot sentinel");
        }
        if (NeedRehash()) {
            RehashLinearProbe(capacity_ << 1);
        }

        const uint32_t hashValue = get_single_hash_value(static_cast<uint32_t>(key));
        bool inserted = false;
        const uint32_t pos = FindPosition(static_cast<uint32_t>(key), hashValue, inserted);
        if (inserted) {
            slots_[pos].key = static_cast<uint32_t>(key);
            slots_[pos].val = static_cast<uint32_t>(0);
            ++elementsSize_;
        }
        return ResultType(slots_[pos].val, inserted);
    }

    uint32_t FindPosition(uint32_t key, uint32_t hashValue, bool &inserted) const
    {
        uint32_t current = hashValue;
        const uint32_t start = current;

        while (true) {
            const uint32_t tabKey = slots_[current].key;
            if (tabKey == kEmptyKey) {
                inserted = true;
                return current;
            }
            if (tabKey == key) {
                return current;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == start)) {
                throw std::runtime_error("SveHashTable32 table full");
            }
        }
    }

    void PlaceEntryLinearProbe(
        std::vector<Slot> &slots,
        const HashParams32 &param,
        uint32_t key,
        uint32_t value)
    {
        uint32_t hash = GetHashValueForParam(key, param);
        while (true) {
            if (slots[hash].key == kEmptyKey) {
                slots[hash].key = key;
                slots[hash].val = value;
                return;
            }
            hash = (hash + 1) & param.bucketsMask;
        }
    }

    void RehashLinearProbe(uint64_t requestedCapacity)
    {
        const uint64_t newCapacity = NormalizeCapacity(requestedCapacity);
        if (newCapacity > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("SveHashTable32 rehash capacity exceeds uint32_t range");
        }

        std::vector<Slot> newSlots(newCapacity, Slot {kEmptyKey, 0});
        HashParams32 newParam = BuildHashParamForCapacity(newCapacity);
        for (uint64_t i = 0; i < capacity_; ++i) {
            if (slots_[i].key == kEmptyKey) {
                continue;
            }
            PlaceEntryLinearProbe(newSlots, newParam, slots_[i].key, slots_[i].val);
        }

        slots_.swap(newSlots);
        capacity_ = newCapacity;
        hashParam_ = newParam;
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    template <bool WithPayload, bool StoreKey, bool NeedEmptyProcessing = true>
    inline int probe_single_key(
        const uint32_t key,
        const uint32_t payload,
        const uint32_t hash,
        uint32_t *output_keys,
        uint32_t *output_values,
        uint32_t *output_payloads,
        int &output_index) const
    {
        if constexpr (NeedEmptyProcessing) {
            if (unlikely(key == kEmptyKey)) {
                if (!hasEmptyKeyValue_) {
                    return 1;
                }
                if constexpr (StoreKey) {
                    output_keys[output_index] = key;
                }
                output_values[output_index] = static_cast<uint32_t>(emptyKeyValue_);
                if constexpr (WithPayload) {
                    output_payloads[output_index] = payload;
                }
                ++output_index;
                return 0;
            }
        }

        uint32_t current = hash;
        while (true) {
            const uint32_t tabKey = slots_[current].key;
            if (tabKey == kEmptyKey) {
                return 1;
            }
            if (tabKey == key) {
                if constexpr (StoreKey) {
                    output_keys[output_index] = key;
                }
                output_values[output_index] = slots_[current].val;
                if constexpr (WithPayload) {
                    output_payloads[output_index] = payload;
                }
                ++output_index;
                return 0;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == hash)) {
                return 2;
            }
        }
    }

    inline void compute_hash_u32(const uint32_t *key, uint32_t *out_hash) const
    {
        svbool_t pg32 = svptrue_b32();
        svuint32_t h = svld1_u32(pg32, key);
        h = svmul_n_u32_z(pg32, h, kFib32HashFactor);
        h = svlsr_n_u32_z(pg32, h, hashParam_.shiftRight);
        svst1_u32(pg32, out_hash, h);
    }

    template <svprfop PrefetchFlag = SV_PLDL1KEEP>
    inline void prefetch_slots_u32(const uint32_t *index) const
    {
        svbool_t pg32 = svptrue_b32();
        svuint32_t idx32 = svld1_u32(pg32, index);
        svbool_t pg64 = svptrue_b64();
        svuint64_t idx_lo = svunpklo_u64(idx32);
        svuint64_t idx_hi = svunpkhi_u64(idx32);
        svuint64_t off_lo = svlsl_n_u64_z(pg64, idx_lo, 3);
        svuint64_t off_hi = svlsl_n_u64_z(pg64, idx_hi, 3);
        const uint64_t base_slots = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(slots_.data()));
        svprfb_gather_u64base_offset(pg64, off_lo, base_slots, PrefetchFlag);
        svprfb_gather_u64base_offset(pg64, off_hi, base_slots, PrefetchFlag);
    }

};

} // namespace hashmap
} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_OPERATOR_HASHMAP_SVEHT32_AOS_H
