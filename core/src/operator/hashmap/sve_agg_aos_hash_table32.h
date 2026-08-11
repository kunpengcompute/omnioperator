#ifndef OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_H
#define OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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

class SveAggAosHashTable32 {
public:
    static constexpr uint32_t kEmptyKey = static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
    static constexpr int32_t kLaneCount = 8;
    static constexpr int32_t kGroupSize = 64;

    using ResultType = InsertResult<uint32_t>;

    struct LookupResult {
        bool found = false;
        uint32_t handle = 0;
    };

    struct BatchLookupCounts {
        int32_t hitCount = 0;
        int32_t missCount = 0;
    };

    static void ValidateLaneCount()
    {
        if (unlikely(svcntw() != static_cast<uint64_t>(kLaneCount))) {
            throw std::runtime_error("SveAggAosHashTable32 requires fixed 256-bit SVE (8 lanes for u32)");
        }
    }

    explicit SveAggAosHashTable32(uint8_t initDegree = defaultDegreeSize)
    {
        ValidateLaneCount();
        InitTableAndHashParam(1ULL << initDegree);
    }

    explicit SveAggAosHashTable32(uint64_t initCapacity)
    {
        ValidateLaneCount();
        InitTableAndHashParam(initCapacity);
    }

    LookupResult Find(uint32_t key) const
    {
        if (unlikely(key == kEmptyKey)) {
            return {};
        }
        bool inserted = false;
        const uint32_t pos = FindPosition(key, GetHashValue(key), inserted);
        if (inserted) {
            return {};
        }
        return {true, slots_[pos].handle};
    }

    ResultType EmplaceScalar(uint32_t key)
    {
        if (NeedRehash()) {
            RehashLinearProbe(capacity_ << 1);
        }
        if (unlikely(key == kEmptyKey)) {
            throw std::runtime_error("SveAggAosHashTable32: INT32_MAX is reserved as the empty-slot sentinel");
        }

        bool inserted = false;
        const uint32_t pos = FindPosition(key, GetHashValue(key), inserted);
        if (inserted) {
            slots_[pos].key = key;
            slots_[pos].handle = 0;
            ++elementsSize_;
        }
        return ResultType(slots_[pos].handle, inserted);
    }

#ifdef SVEHTMISSES
    bool CanInsertAdditional(uint32_t additional) const
    {
        return elementsSize_ + additional <= rehashThreshold_;
    }

    ResultType EmplaceKnownMiss(uint32_t key, uint32_t slot)
    {
        if (NeedRehash()) {
            return EmplaceScalar(key);
        }
        if (unlikely(key == kEmptyKey)) {
            throw std::runtime_error("SveAggAosHashTable32: INT32_MAX is reserved as the empty-slot sentinel");
        }
        if (likely(slot < capacity_)) {
            Slot &target = slots_[slot];
            if (target.key == kEmptyKey) {
                target.key = key;
                target.handle = 0;
                ++elementsSize_;
                return ResultType(target.handle, true);
            }
            if (target.key == key) {
                return ResultType(target.handle, false);
            }
        }
        return EmplaceScalar(key);
    }

    BatchLookupCounts LookupBatchSVEForInsert(const uint32_t *keys, const uint32_t *rowIds, int32_t count,
        uint32_t *hitRows, uint32_t *hitHandles, uint32_t *missRows, uint32_t *missKeys,
        uint32_t *missSlots) const
    {
        if (count < kGroupSize) {
            return LookupBatchScalar(keys, rowIds, count, hitRows, hitHandles, missRows, missKeys, missSlots);
        }
        return LookupBatchSVEImpl(keys, rowIds, count, hitRows, hitHandles, missRows, missKeys, missSlots);
    }
#endif

    BatchLookupCounts LookupBatchSVE(const uint32_t *keys, const uint32_t *rowIds, int32_t count, uint32_t *hitRows,
        uint32_t *hitHandles, uint32_t *missRows, uint32_t *missKeys) const
    {
        if (count < kGroupSize) {
            return LookupBatchScalar(keys, rowIds, count, hitRows, hitHandles, missRows, missKeys, nullptr);
        }
        return LookupBatchSVEImpl(keys, rowIds, count, hitRows, hitHandles, missRows, missKeys, nullptr);
    }

    size_t GetElementsSize() const
    {
        return elementsSize_;
    }

    uint64_t GetCapacity() const
    {
        return capacity_;
    }

    void Reset()
    {
        slots_.assign(capacity_, Slot {kEmptyKey, 0});
        elementsSize_ = 0;
    }

    int32_t CopyGroups(uint64_t startSlot, int32_t maxRows, uint32_t *keys, uint32_t *handles,
        uint64_t &nextSlot) const
    {
        int32_t copied = 0;
        uint64_t slot = startSlot;
        while (slot < capacity_ && copied < maxRows) {
            if (slots_[slot].key != kEmptyKey) {
                keys[copied] = slots_[slot].key;
                handles[copied] = slots_[slot].handle;
                ++copied;
            }
            ++slot;
        }
        nextSlot = slot;
        return copied;
    }

private:
    static constexpr uint32_t kFib32HashFactor = 0x9e3779b9u;
    static constexpr double maxLoadFactor = 0.75;
    static constexpr uint8_t defaultDegreeSize = 15;

    struct Slot {
        uint32_t key;
        uint32_t handle;
    };

    static_assert(sizeof(Slot) == 8, "AoS slot must stay key+handle packed");

    struct HashParams32 {
        uint32_t bucketsMask = 0;
        int shiftRight = 0;
    };

    std::vector<Slot> slots_;
    HashParams32 hashParam_ {};
    uint64_t rehashThreshold_ = 0;
    uint64_t capacity_ = 0;
    size_t elementsSize_ = 0;

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
            throw std::runtime_error("SveAggAosHashTable32 capacity exceeds uint32_t range");
        }
        slots_.assign(capacity_, Slot {kEmptyKey, 0});
        hashParam_ = BuildHashParamForCapacity(capacity_);
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    bool NeedRehash() const
    {
        return (elementsSize_ + 1) > rehashThreshold_;
    }

    ALWAYS_INLINE static uint32_t GetHashValueForParam(uint32_t key, const HashParams32 &param)
    {
        return (key * kFib32HashFactor) >> param.shiftRight;
    }

    ALWAYS_INLINE uint32_t GetHashValue(uint32_t key) const
    {
        return GetHashValueForParam(key, hashParam_);
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
                inserted = false;
                return current;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == start)) {
                throw std::runtime_error("SveAggAosHashTable32 table full");
            }
        }
    }

    void PlaceEntryLinearProbe(std::vector<Slot> &slots, const HashParams32 &param, uint32_t key, uint32_t handle)
    {
        uint32_t hash = GetHashValueForParam(key, param);
        while (true) {
            if (slots[hash].key == kEmptyKey) {
                slots[hash].key = key;
                slots[hash].handle = handle;
                return;
            }
            hash = (hash + 1) & param.bucketsMask;
        }
    }

    void RehashLinearProbe(uint64_t requestedCapacity)
    {
        const uint64_t newCapacity = NormalizeCapacity(requestedCapacity);
        if (newCapacity > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("SveAggAosHashTable32 rehash capacity exceeds uint32_t range");
        }

        std::vector<Slot> newSlots(newCapacity, Slot {kEmptyKey, 0});
        HashParams32 newParam = BuildHashParamForCapacity(newCapacity);
        for (uint64_t i = 0; i < capacity_; ++i) {
            if (slots_[i].key != kEmptyKey) {
                PlaceEntryLinearProbe(newSlots, newParam, slots_[i].key, slots_[i].handle);
            }
        }
        slots_.swap(newSlots);
        capacity_ = newCapacity;
        hashParam_ = newParam;
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    ALWAYS_INLINE bool LookupOne(uint32_t key, uint32_t &handle) const
    {
        if (unlikely(key == kEmptyKey)) {
            return false;
        }
        uint32_t current = GetHashValue(key);
        const uint32_t start = current;
        while (true) {
            const uint32_t tabKey = slots_[current].key;
            if (tabKey == kEmptyKey) {
                return false;
            }
            if (tabKey == key) {
                handle = slots_[current].handle;
                return true;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == start)) {
                return false;
            }
        }
    }

    BatchLookupCounts LookupBatchScalar(const uint32_t *keys, const uint32_t *rowIds, int32_t count, uint32_t *hitRows,
        uint32_t *hitHandles, uint32_t *missRows, uint32_t *missKeys, uint32_t *missSlots) const
    {
        BatchLookupCounts counts {};
        for (int32_t i = 0; i < count; ++i) {
            if (unlikely(keys[i] == kEmptyKey)) {
                missRows[counts.missCount] = rowIds[i];
                missKeys[counts.missCount] = keys[i];
                if (missSlots != nullptr) {
                    missSlots[counts.missCount] = 0;
                }
                ++counts.missCount;
                continue;
            }
            bool inserted = false;
            const uint32_t pos = FindPosition(keys[i], GetHashValue(keys[i]), inserted);
            if (!inserted) {
                hitRows[counts.hitCount] = rowIds[i];
                hitHandles[counts.hitCount] = slots_[pos].handle;
                ++counts.hitCount;
            } else {
                missRows[counts.missCount] = rowIds[i];
                missKeys[counts.missCount] = keys[i];
                if (missSlots != nullptr) {
                    missSlots[counts.missCount] = pos;
                }
                ++counts.missCount;
            }
        }
        return counts;
    }

    ALWAYS_INLINE void ComputeHashU32(const uint32_t *key, uint32_t *outHash) const
    {
        svbool_t pg32 = svptrue_b32();
        svuint32_t h = svld1_u32(pg32, key);
        h = svmul_n_u32_z(pg32, h, kFib32HashFactor);
        h = svlsr_n_u32_z(pg32, h, hashParam_.shiftRight);
        svst1_u32(pg32, outHash, h);
    }

    template <svprfop PrefetchFlag = SV_PLDL1KEEP>
    ALWAYS_INLINE void PrefetchSlotsU32(const uint32_t *index) const
    {
        svbool_t pg32 = svptrue_b32();
        svuint32_t idx32 = svld1_u32(pg32, index);
        svbool_t pg64 = svptrue_b64();
        svuint64_t idxLo = svunpklo_u64(idx32);
        svuint64_t idxHi = svunpkhi_u64(idx32);
        svuint64_t offLo = svlsl_n_u64_z(pg64, idxLo, 3);
        svuint64_t offHi = svlsl_n_u64_z(pg64, idxHi, 3);
        const uint64_t baseSlots = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(slots_.data()));
        svprfb_gather_u64base_offset(pg64, offLo, baseSlots, PrefetchFlag);
        svprfb_gather_u64base_offset(pg64, offHi, baseSlots, PrefetchFlag);
    }

    int32_t LookupSingleKey(uint32_t key, uint32_t rowId, uint32_t hash, uint32_t *hitRows, uint32_t *hitHandles,
        int32_t &hitCount, uint32_t *missRows, uint32_t *missKeys, uint32_t *missSlots, int32_t &missCount) const
    {
        uint32_t current = hash;
        while (true) {
            const uint32_t tabKey = slots_[current].key;
            if (tabKey == kEmptyKey) {
                missRows[missCount] = rowId;
                missKeys[missCount] = key;
                if (missSlots != nullptr) {
                    missSlots[missCount] = current;
                }
                ++missCount;
                return 1;
            }
            if (tabKey == key) {
                hitRows[hitCount] = rowId;
                hitHandles[hitCount] = slots_[current].handle;
                ++hitCount;
                return 0;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == hash)) {
                missRows[missCount] = rowId;
                missKeys[missCount] = key;
                if (missSlots != nullptr) {
                    missSlots[missCount] = current;
                }
                ++missCount;
                return 2;
            }
        }
    }

    BatchLookupCounts LookupBatchSVEImpl(const uint32_t *keys, const uint32_t *rowIds, int32_t count, uint32_t *hitRows,
        uint32_t *hitHandles, uint32_t *missRows, uint32_t *missKeys, uint32_t *missSlots) const
    {
        BatchLookupCounts total {};
        svbool_t pg = svptrue_b32();
        const uint32_t *slotWordsBase = reinterpret_cast<const uint32_t *>(slots_.data());

        for (int32_t groupStart = 0; groupStart < count; groupStart += kGroupSize) {
            const int32_t groupSize = std::min(kGroupSize, count - groupStart);
            uint32_t hashes[kGroupSize];
            for (int32_t i = 0; i < groupSize; i += kLaneCount) {
                const int32_t remaining = std::min(kLaneCount, groupSize - i);
                if (likely(remaining == kLaneCount)) {
                    ComputeHashU32(&keys[groupStart + i], &hashes[i]);
                    PrefetchSlotsU32<SV_PLDL1KEEP>(&hashes[i]);
                } else {
                    uint32_t tempKeys[kLaneCount] = {0};
                    uint32_t tempHashes[kLaneCount] = {0};
                    std::memcpy(tempKeys, &keys[groupStart + i], remaining * sizeof(uint32_t));
                    ComputeHashU32(tempKeys, tempHashes);
                    std::memcpy(&hashes[i], tempHashes, remaining * sizeof(uint32_t));
                    PrefetchSlotsU32<SV_PLDL1KEEP>(tempHashes);
                }
            }

            svbool_t invMask = svptrue_b32();
            svbool_t activeMask = svpfalse_b();
            svuint32_t probeKey = svdup_n_u32(0);
            svuint32_t probeRow = svdup_n_u32(0);
            svuint32_t h = svdup_n_u32(0);
            svuint32_t tabHandle = svdup_n_u32(0);

            int32_t i = 0;
            int32_t incI = 0;
            int32_t iter = 0;
            while (likely(i + kLaneCount <= groupSize)) {
                ++iter;
                svuint32_t newKey = svld1_u32(invMask, &keys[groupStart + i]);
                svuint32_t newRow = svld1_u32(invMask, &rowIds[groupStart + i]);
                probeKey = svsel_u32(invMask, newKey, probeKey);
                probeRow = svsel_u32(invMask, newRow, probeRow);

                svuint32_t off = svdup_n_u32_z(activeMask, 1);
                h = svadd_u32_z(activeMask, h, off);
                svuint32_t newHash = svld1_u32(invMask, &hashes[i]);
                h = svsel_u32(invMask, newHash, h);
                h = svand_u32_z(pg, h, svdup_n_u32(hashParam_.bucketsMask));

                svuint32_t byteOff = svlsl_n_u32_z(pg, h, 3);
                svuint32_t tabKey = svld1_gather_u32offset_u32(pg, slotWordsBase, byteOff);
                svbool_t emptyMask = svcmpeq_n_u32(pg, tabKey, kEmptyKey);
                svbool_t nonemptyMask = svnot_b_z(pg, emptyMask);
                svuint32_t handleOff = svadd_n_u32_z(pg, byteOff, sizeof(uint32_t));
                tabHandle = svld1_gather_u32offset_u32(nonemptyMask, slotWordsBase, handleOff);
                svbool_t matchMask = svand_b_z(pg, nonemptyMask, svcmpeq_u32(pg, tabKey, probeKey));
                svbool_t doneMask = svorr_b_z(pg, emptyMask, matchMask);

                const int32_t hitInc = svcntp_b32(pg, matchMask);
                if (hitInc > 0) {
                    svuint32_t compactedRows = svcompact_u32(matchMask, probeRow);
                    svuint32_t compactedHandles = svcompact_u32(matchMask, tabHandle);
                    svbool_t storeMask = svwhilelt_b32_s32(0, hitInc);
                    svst1_u32(storeMask, &hitRows[total.hitCount], compactedRows);
                    svst1_u32(storeMask, &hitHandles[total.hitCount], compactedHandles);
                    total.hitCount += hitInc;
                }

                const int32_t missInc = svcntp_b32(pg, emptyMask);
                if (missInc > 0) {
                    svuint32_t compactedRows = svcompact_u32(emptyMask, probeRow);
                    svuint32_t compactedKeys = svcompact_u32(emptyMask, probeKey);
                    svbool_t storeMask = svwhilelt_b32_s32(0, missInc);
                    svst1_u32(storeMask, &missRows[total.missCount], compactedRows);
                    svst1_u32(storeMask, &missKeys[total.missCount], compactedKeys);
                    if (missSlots != nullptr) {
                        svst1_u32(storeMask, &missSlots[total.missCount], svcompact_u32(emptyMask, h));
                    }
                    total.missCount += missInc;
                }

                incI = svcntp_b32(pg, doneMask);
                if (incI == kLaneCount) {
                    invMask = svptrue_b32();
                    activeMask = svpfalse_b();
                } else {
                    activeMask = svnot_b_z(pg, doneMask);
                    h = svcompact_u32(activeMask, h);
                    probeKey = svcompact_u32(activeMask, probeKey);
                    probeRow = svcompact_u32(activeMask, probeRow);
                    activeMask = svwhilelt_b32_s32(0, kLaneCount - incI);
                    invMask = svnot_b_z(pg, activeMask);
                }
                i += incI;
            }

            const int32_t activeTail = iter > 0 ? kLaneCount - incI : 0;
            if (activeTail > 0) {
                uint32_t remainingKeys[kLaneCount];
                uint32_t remainingRows[kLaneCount];
                uint32_t remainingHashes[kLaneCount];
                svst1_u32(pg, remainingKeys, probeKey);
                svst1_u32(pg, remainingRows, probeRow);
                svst1_u32(pg, remainingHashes, h);
                for (int32_t k = 0; k < activeTail; ++k) {
                    LookupSingleKey(remainingKeys[k], remainingRows[k], remainingHashes[k], hitRows, hitHandles,
                        total.hitCount, missRows, missKeys, missSlots, total.missCount);
                }
            }

            for (int32_t j = i + activeTail; j < groupSize; ++j) {
                LookupSingleKey(keys[groupStart + j], rowIds[groupStart + j], hashes[j], hitRows, hitHandles,
                    total.hitCount, missRows, missKeys, missSlots, total.missCount);
            }
        }
        return total;
    }
};

} // namespace hashmap
} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_H
