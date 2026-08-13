#ifndef OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_PAIR_H
#define OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_PAIR_H

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

class SveAggAosHashTable32Pair {
public:
    static constexpr uint32_t kOccupied = 1u;
    static constexpr uint32_t kKey0Null = 1u << 1;
    static constexpr uint32_t kKey1Null = 1u << 2;
    static constexpr uint32_t kNullMask = kKey0Null | kKey1Null;
    static constexpr int32_t kLaneCount = 8;
    static constexpr int32_t kGroupSize = 64;

    using ResultType = InsertResult<uint32_t>;

    struct Key {
        uint32_t key0 = 0;
        uint32_t key1 = 0;
        uint32_t nullMask = 0;
    };

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
            throw std::runtime_error("SveAggAosHashTable32Pair requires fixed 256-bit SVE (8 lanes for u32)");
        }
    }

    explicit SveAggAosHashTable32Pair(uint8_t initDegree = defaultDegreeSize)
    {
        ValidateLaneCount();
        InitTableAndHashParam(1ULL << initDegree);
    }

    explicit SveAggAosHashTable32Pair(uint64_t initCapacity)
    {
        ValidateLaneCount();
        InitTableAndHashParam(initCapacity);
    }

    LookupResult Find(Key key) const
    {
        bool inserted = false;
        const uint32_t pos = FindPosition(key, GetHashValue(key), inserted);
        if (inserted) {
            return {};
        }
        return {true, slots_[pos].handle};
    }

    ResultType EmplaceScalar(Key key)
    {
        if (NeedRehash()) {
            RehashLinearProbe(capacity_ << 1);
        }

        bool inserted = false;
        const uint32_t pos = FindPosition(key, GetHashValue(key), inserted);
        if (inserted) {
            slots_[pos].key0 = key.key0;
            slots_[pos].key1 = key.key1;
            slots_[pos].handle = 0;
            slots_[pos].meta = kOccupied | (key.nullMask & kNullMask);
            ++elementsSize_;
        }
        return ResultType(slots_[pos].handle, inserted);
    }

#ifdef SVEHTMISSES
    bool CanInsertAdditional(uint32_t additional) const
    {
        return elementsSize_ + additional <= rehashThreshold_;
    }

    ResultType EmplaceKnownMiss(Key key, uint32_t slot)
    {
        if (NeedRehash()) {
            return EmplaceScalar(key);
        }
        if (likely(slot < capacity_)) {
            Slot &target = slots_[slot];
            if ((target.meta & kOccupied) == 0) {
                target.key0 = key.key0;
                target.key1 = key.key1;
                target.handle = 0;
                target.meta = kOccupied | (key.nullMask & kNullMask);
                ++elementsSize_;
                return ResultType(target.handle, true);
            }
            if (KeyEquals(target, key)) {
                return ResultType(target.handle, false);
            }
        }
        return EmplaceScalar(key);
    }

    BatchLookupCounts LookupBatchSVEForInsert(const uint32_t *key0, const uint32_t *key1,
        const uint32_t *nullMasks, const uint32_t *rowIds, int32_t count, uint32_t *hitRows,
        uint32_t *hitHandles, uint32_t *missRows, Key *missKeys, uint32_t *missSlots) const
    {
        if (count < kGroupSize) {
            return LookupBatchScalar(
                key0, key1, nullMasks, rowIds, count, hitRows, hitHandles, missRows, missKeys, missSlots);
        }
        return LookupBatchSVEImpl(
            key0, key1, nullMasks, rowIds, count, hitRows, hitHandles, missRows, missKeys, missSlots);
    }
#endif

    BatchLookupCounts LookupBatchSVE(const uint32_t *key0, const uint32_t *key1, const uint32_t *nullMasks,
        const uint32_t *rowIds, int32_t count, uint32_t *hitRows, uint32_t *hitHandles, uint32_t *missRows,
        Key *missKeys) const
    {
        if (count < kGroupSize) {
            return LookupBatchScalar(
                key0, key1, nullMasks, rowIds, count, hitRows, hitHandles, missRows, missKeys, nullptr);
        }
        return LookupBatchSVEImpl(
            key0, key1, nullMasks, rowIds, count, hitRows, hitHandles, missRows, missKeys, nullptr);
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
        slots_.assign(capacity_, Slot {});
        elementsSize_ = 0;
    }

    int32_t CopyGroups(uint64_t startSlot, int32_t maxRows, uint32_t *key0, uint32_t *key1, uint32_t *nullMasks,
        uint32_t *handles, uint64_t &nextSlot) const
    {
        int32_t copied = 0;
        uint64_t slot = startSlot;
        while (slot < capacity_ && copied < maxRows) {
            if ((slots_[slot].meta & kOccupied) != 0) {
                key0[copied] = slots_[slot].key0;
                key1[copied] = slots_[slot].key1;
                nullMasks[copied] = slots_[slot].meta & kNullMask;
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
    static constexpr uint32_t kMixFactor0 = 0x85ebca6bu;
    static constexpr uint32_t kMixFactor1 = 0xc2b2ae35u;
    static constexpr double maxLoadFactor = 0.75;
    static constexpr uint8_t defaultDegreeSize = 15;

    struct Slot {
        uint32_t key0 = 0;
        uint32_t key1 = 0;
        uint32_t handle = 0;
        uint32_t meta = 0;
    };

    static_assert(sizeof(Slot) == 16, "Pair AoS slot must stay key0+key1+handle+meta packed");

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
            throw std::runtime_error("SveAggAosHashTable32Pair capacity exceeds uint32_t range");
        }
        slots_.assign(capacity_, Slot {});
        hashParam_ = BuildHashParamForCapacity(capacity_);
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    bool NeedRehash() const
    {
        return (elementsSize_ + 1) > rehashThreshold_;
    }

    static ALWAYS_INLINE uint32_t Rotl32(uint32_t value, int shift)
    {
        return (value << shift) | (value >> (32 - shift));
    }

    static ALWAYS_INLINE uint32_t Mix32(uint32_t value)
    {
        value ^= value >> 16;
        value *= kMixFactor0;
        value ^= value >> 13;
        value *= kMixFactor1;
        value ^= value >> 16;
        return value;
    }

    static ALWAYS_INLINE uint32_t GetHashValueForParam(Key key, const HashParams32 &param)
    {
        const uint32_t normalized0 = (key.nullMask & kKey0Null) != 0 ? 0 : key.key0;
        const uint32_t normalized1 = (key.nullMask & kKey1Null) != 0 ? 0 : key.key1;
        uint32_t hash = Mix32(normalized0);
        hash ^= Rotl32(Mix32(normalized1), 16);
        hash ^= Mix32(key.nullMask);
        hash *= kFib32HashFactor;
        return hash >> param.shiftRight;
    }

    ALWAYS_INLINE uint32_t GetHashValue(Key key) const
    {
        return GetHashValueForParam(key, hashParam_);
    }

    static ALWAYS_INLINE bool KeyEquals(const Slot &slot, Key key)
    {
        if ((slot.meta & kNullMask) != (key.nullMask & kNullMask)) {
            return false;
        }
        if ((key.nullMask & kKey0Null) == 0 && slot.key0 != key.key0) {
            return false;
        }
        if ((key.nullMask & kKey1Null) == 0 && slot.key1 != key.key1) {
            return false;
        }
        return true;
    }

    uint32_t FindPosition(Key key, uint32_t hashValue, bool &inserted) const
    {
        uint32_t current = hashValue;
        const uint32_t start = current;
        while (true) {
            const uint32_t meta = slots_[current].meta;
            if ((meta & kOccupied) == 0) {
                inserted = true;
                return current;
            }
            if (KeyEquals(slots_[current], key)) {
                inserted = false;
                return current;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == start)) {
                throw std::runtime_error("SveAggAosHashTable32Pair table full");
            }
        }
    }

    void PlaceEntryLinearProbe(std::vector<Slot> &slots, const HashParams32 &param, const Slot &entry)
    {
        Key key {entry.key0, entry.key1, entry.meta & kNullMask};
        uint32_t hash = GetHashValueForParam(key, param);
        while (true) {
            if ((slots[hash].meta & kOccupied) == 0) {
                slots[hash] = entry;
                return;
            }
            hash = (hash + 1) & param.bucketsMask;
        }
    }

    void RehashLinearProbe(uint64_t requestedCapacity)
    {
        const uint64_t newCapacity = NormalizeCapacity(requestedCapacity);
        if (newCapacity > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("SveAggAosHashTable32Pair rehash capacity exceeds uint32_t range");
        }

        std::vector<Slot> newSlots(newCapacity, Slot {});
        HashParams32 newParam = BuildHashParamForCapacity(newCapacity);
        for (uint64_t i = 0; i < capacity_; ++i) {
            if ((slots_[i].meta & kOccupied) != 0) {
                PlaceEntryLinearProbe(newSlots, newParam, slots_[i]);
            }
        }
        slots_.swap(newSlots);
        capacity_ = newCapacity;
        hashParam_ = newParam;
        rehashThreshold_ = ComputeRehashThreshold(capacity_);
    }

    ALWAYS_INLINE bool LookupOne(Key key, uint32_t &handle) const
    {
        uint32_t current = GetHashValue(key);
        const uint32_t start = current;
        while (true) {
            const uint32_t meta = slots_[current].meta;
            if ((meta & kOccupied) == 0) {
                return false;
            }
            if (KeyEquals(slots_[current], key)) {
                handle = slots_[current].handle;
                return true;
            }
            current = (current + 1) & hashParam_.bucketsMask;
            if (unlikely(current == start)) {
                return false;
            }
        }
    }

    BatchLookupCounts LookupBatchScalar(const uint32_t *key0, const uint32_t *key1, const uint32_t *nullMasks,
        const uint32_t *rowIds, int32_t count, uint32_t *hitRows, uint32_t *hitHandles, uint32_t *missRows,
        Key *missKeys, uint32_t *missSlots) const
    {
        BatchLookupCounts counts {};
        for (int32_t i = 0; i < count; ++i) {
            Key key {key0[i], key1[i], nullMasks[i]};
            bool inserted = false;
            const uint32_t pos = FindPosition(key, GetHashValue(key), inserted);
            if (!inserted) {
                hitRows[counts.hitCount] = rowIds[i];
                hitHandles[counts.hitCount] = slots_[pos].handle;
                ++counts.hitCount;
            } else {
                missRows[counts.missCount] = rowIds[i];
                missKeys[counts.missCount] = key;
                if (missSlots != nullptr) {
                    missSlots[counts.missCount] = pos;
                }
                ++counts.missCount;
            }
        }
        return counts;
    }

    ALWAYS_INLINE static svuint32_t Rotl32SVE(svbool_t pg, svuint32_t value, int shift)
    {
        return svorr_u32_z(pg, svlsl_n_u32_z(pg, value, shift), svlsr_n_u32_z(pg, value, 32 - shift));
    }

    ALWAYS_INLINE static svuint32_t Mix32SVE(svbool_t pg, svuint32_t value)
    {
        value = sveor_u32_z(pg, value, svlsr_n_u32_z(pg, value, 16));
        value = svmul_n_u32_z(pg, value, kMixFactor0);
        value = sveor_u32_z(pg, value, svlsr_n_u32_z(pg, value, 13));
        value = svmul_n_u32_z(pg, value, kMixFactor1);
        value = sveor_u32_z(pg, value, svlsr_n_u32_z(pg, value, 16));
        return value;
    }

    ALWAYS_INLINE void ComputeHashU32(
        const uint32_t *key0, const uint32_t *key1, const uint32_t *nullMasks, uint32_t *outHash) const
    {
        svbool_t pg = svptrue_b32();
        svuint32_t k0 = svld1_u32(pg, key0);
        svuint32_t k1 = svld1_u32(pg, key1);
        svuint32_t nm = svld1_u32(pg, nullMasks);
        svbool_t key0Null = svcmpne_n_u32(pg, svand_n_u32_z(pg, nm, kKey0Null), 0);
        svbool_t key1Null = svcmpne_n_u32(pg, svand_n_u32_z(pg, nm, kKey1Null), 0);
        k0 = svsel_u32(key0Null, svdup_n_u32(0), k0);
        k1 = svsel_u32(key1Null, svdup_n_u32(0), k1);

        svuint32_t hash = Mix32SVE(pg, k0);
        hash = sveor_u32_z(pg, hash, Rotl32SVE(pg, Mix32SVE(pg, k1), 16));
        hash = sveor_u32_z(pg, hash, Mix32SVE(pg, nm));
        hash = svmul_n_u32_z(pg, hash, kFib32HashFactor);
        hash = svlsr_n_u32_z(pg, hash, hashParam_.shiftRight);
        svst1_u32(pg, outHash, hash);
    }

    template <svprfop PrefetchFlag = SV_PLDL1KEEP>
    ALWAYS_INLINE void PrefetchSlotsU32(const uint32_t *index) const
    {
        svbool_t pg32 = svptrue_b32();
        svuint32_t idx32 = svld1_u32(pg32, index);
        svbool_t pg64 = svptrue_b64();
        svuint64_t idxLo = svunpklo_u64(idx32);
        svuint64_t idxHi = svunpkhi_u64(idx32);
        svuint64_t offLo = svlsl_n_u64_z(pg64, idxLo, 4);
        svuint64_t offHi = svlsl_n_u64_z(pg64, idxHi, 4);
        const uint64_t baseSlots = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(slots_.data()));
        svprfb_gather_u64base_offset(pg64, offLo, baseSlots, PrefetchFlag);
        svprfb_gather_u64base_offset(pg64, offHi, baseSlots, PrefetchFlag);
    }

    int32_t LookupSingleKey(Key key, uint32_t rowId, uint32_t hash, uint32_t *hitRows, uint32_t *hitHandles,
        int32_t &hitCount, uint32_t *missRows, Key *missKeys, uint32_t *missSlots, int32_t &missCount) const
    {
        uint32_t current = hash;
        while (true) {
            const uint32_t meta = slots_[current].meta;
            if ((meta & kOccupied) == 0) {
                missRows[missCount] = rowId;
                missKeys[missCount] = key;
                if (missSlots != nullptr) {
                    missSlots[missCount] = current;
                }
                ++missCount;
                return 1;
            }
            if (KeyEquals(slots_[current], key)) {
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

    BatchLookupCounts LookupBatchSVEImpl(const uint32_t *key0, const uint32_t *key1, const uint32_t *nullMasks,
        const uint32_t *rowIds, int32_t count, uint32_t *hitRows, uint32_t *hitHandles, uint32_t *missRows,
        Key *missKeys, uint32_t *missSlots) const
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
                    ComputeHashU32(&key0[groupStart + i], &key1[groupStart + i], &nullMasks[groupStart + i],
                        &hashes[i]);
                    PrefetchSlotsU32<SV_PLDL1KEEP>(&hashes[i]);
                } else {
                    uint32_t tempKey0[kLaneCount] = {0};
                    uint32_t tempKey1[kLaneCount] = {0};
                    uint32_t tempNullMasks[kLaneCount] = {0};
                    uint32_t tempHashes[kLaneCount] = {0};
                    std::memcpy(tempKey0, &key0[groupStart + i], remaining * sizeof(uint32_t));
                    std::memcpy(tempKey1, &key1[groupStart + i], remaining * sizeof(uint32_t));
                    std::memcpy(tempNullMasks, &nullMasks[groupStart + i], remaining * sizeof(uint32_t));
                    ComputeHashU32(tempKey0, tempKey1, tempNullMasks, tempHashes);
                    std::memcpy(&hashes[i], tempHashes, remaining * sizeof(uint32_t));
                    PrefetchSlotsU32<SV_PLDL1KEEP>(tempHashes);
                }
            }

            svbool_t invMask = svptrue_b32();
            svbool_t activeMask = svpfalse_b();
            svuint32_t probeKey0 = svdup_n_u32(0);
            svuint32_t probeKey1 = svdup_n_u32(0);
            svuint32_t probeNullMask = svdup_n_u32(0);
            svuint32_t probeRow = svdup_n_u32(0);
            svuint32_t h = svdup_n_u32(0);
            svuint32_t tabHandle = svdup_n_u32(0);

            int32_t i = 0;
            int32_t incI = 0;
            int32_t iter = 0;
            while (likely(i + kLaneCount <= groupSize)) {
                ++iter;
                svuint32_t newKey0 = svld1_u32(invMask, &key0[groupStart + i]);
                svuint32_t newKey1 = svld1_u32(invMask, &key1[groupStart + i]);
                svuint32_t newNullMask = svld1_u32(invMask, &nullMasks[groupStart + i]);
                svuint32_t newRow = svld1_u32(invMask, &rowIds[groupStart + i]);
                probeKey0 = svsel_u32(invMask, newKey0, probeKey0);
                probeKey1 = svsel_u32(invMask, newKey1, probeKey1);
                probeNullMask = svsel_u32(invMask, newNullMask, probeNullMask);
                probeRow = svsel_u32(invMask, newRow, probeRow);

                svuint32_t off = svdup_n_u32_z(activeMask, 1);
                h = svadd_u32_z(activeMask, h, off);
                svuint32_t newHash = svld1_u32(invMask, &hashes[i]);
                h = svsel_u32(invMask, newHash, h);
                h = svand_u32_z(pg, h, svdup_n_u32(hashParam_.bucketsMask));

                svuint32_t byteOff = svlsl_n_u32_z(pg, h, 4);
                svuint32_t key0Off = byteOff;
                svuint32_t key1Off = svadd_n_u32_z(pg, byteOff, sizeof(uint32_t));
                svuint32_t handleOff = svadd_n_u32_z(pg, byteOff, 2 * sizeof(uint32_t));
                svuint32_t metaOff = svadd_n_u32_z(pg, byteOff, 3 * sizeof(uint32_t));

                svuint32_t tabMeta = svld1_gather_u32offset_u32(pg, slotWordsBase, metaOff);
                svbool_t occupiedMask = svcmpne_n_u32(pg, svand_n_u32_z(pg, tabMeta, kOccupied), 0);
                svbool_t emptyMask = svnot_b_z(pg, occupiedMask);
                svbool_t nullMatchMask =
                    svcmpeq_u32(pg, svand_n_u32_z(pg, tabMeta, kNullMask), probeNullMask);

                svuint32_t tabKey0 = svld1_gather_u32offset_u32(occupiedMask, slotWordsBase, key0Off);
                svuint32_t tabKey1 = svld1_gather_u32offset_u32(occupiedMask, slotWordsBase, key1Off);
                tabHandle = svld1_gather_u32offset_u32(occupiedMask, slotWordsBase, handleOff);

                svbool_t key0Null = svcmpne_n_u32(pg, svand_n_u32_z(pg, probeNullMask, kKey0Null), 0);
                svbool_t key1Null = svcmpne_n_u32(pg, svand_n_u32_z(pg, probeNullMask, kKey1Null), 0);
                svbool_t key0Match = svorr_b_z(pg, key0Null, svcmpeq_u32(pg, tabKey0, probeKey0));
                svbool_t key1Match = svorr_b_z(pg, key1Null, svcmpeq_u32(pg, tabKey1, probeKey1));
                svbool_t matchMask = svand_b_z(pg, occupiedMask,
                    svand_b_z(pg, nullMatchMask, svand_b_z(pg, key0Match, key1Match)));
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
                    uint32_t compactedRows[kLaneCount];
                    uint32_t compactedKey0[kLaneCount];
                    uint32_t compactedKey1[kLaneCount];
                    uint32_t compactedNullMasks[kLaneCount];
                    uint32_t compactedSlots[kLaneCount];
                    svst1_u32(svptrue_b32(), compactedRows, svcompact_u32(emptyMask, probeRow));
                    svst1_u32(svptrue_b32(), compactedKey0, svcompact_u32(emptyMask, probeKey0));
                    svst1_u32(svptrue_b32(), compactedKey1, svcompact_u32(emptyMask, probeKey1));
                    svst1_u32(svptrue_b32(), compactedNullMasks, svcompact_u32(emptyMask, probeNullMask));
                    if (missSlots != nullptr) {
                        svst1_u32(svptrue_b32(), compactedSlots, svcompact_u32(emptyMask, h));
                    }
                    for (int32_t m = 0; m < missInc; ++m) {
                        missRows[total.missCount] = compactedRows[m];
                        missKeys[total.missCount] =
                            Key {compactedKey0[m], compactedKey1[m], compactedNullMasks[m]};
                        if (missSlots != nullptr) {
                            missSlots[total.missCount] = compactedSlots[m];
                        }
                        ++total.missCount;
                    }
                }

                incI = svcntp_b32(pg, doneMask);
                if (incI == kLaneCount) {
                    invMask = svptrue_b32();
                    activeMask = svpfalse_b();
                } else {
                    activeMask = svnot_b_z(pg, doneMask);
                    h = svcompact_u32(activeMask, h);
                    probeKey0 = svcompact_u32(activeMask, probeKey0);
                    probeKey1 = svcompact_u32(activeMask, probeKey1);
                    probeNullMask = svcompact_u32(activeMask, probeNullMask);
                    probeRow = svcompact_u32(activeMask, probeRow);
                    activeMask = svwhilelt_b32_s32(0, kLaneCount - incI);
                    invMask = svnot_b_z(pg, activeMask);
                }
                i += incI;
            }

            const int32_t activeTail = iter > 0 ? kLaneCount - incI : 0;
            if (activeTail > 0) {
                uint32_t remainingKey0[kLaneCount];
                uint32_t remainingKey1[kLaneCount];
                uint32_t remainingNullMasks[kLaneCount];
                uint32_t remainingRows[kLaneCount];
                uint32_t remainingHashes[kLaneCount];
                svst1_u32(pg, remainingKey0, probeKey0);
                svst1_u32(pg, remainingKey1, probeKey1);
                svst1_u32(pg, remainingNullMasks, probeNullMask);
                svst1_u32(pg, remainingRows, probeRow);
                svst1_u32(pg, remainingHashes, h);
                for (int32_t k = 0; k < activeTail; ++k) {
                    LookupSingleKey(Key {remainingKey0[k], remainingKey1[k], remainingNullMasks[k]},
                        remainingRows[k], remainingHashes[k], hitRows, hitHandles, total.hitCount, missRows,
                        missKeys, missSlots, total.missCount);
                }
            }

            for (int32_t j = i + activeTail; j < groupSize; ++j) {
                LookupSingleKey(Key {key0[groupStart + j], key1[groupStart + j], nullMasks[groupStart + j]},
                    rowIds[groupStart + j], hashes[j], hitRows, hitHandles, total.hitCount, missRows, missKeys,
                    missSlots, total.missCount);
            }
        }
        return total;
    }
};

} // namespace hashmap
} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_OPERATOR_HASHMAP_SVE_AGG_AOS_HASH_TABLE32_PAIR_H
