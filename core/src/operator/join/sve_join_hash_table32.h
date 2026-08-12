#ifndef OMNI_RUNTIME_SVE_JOIN_HASH_TABLE32_H
#define OMNI_RUNTIME_SVE_JOIN_HASH_TABLE32_H

#include <cstdint>
#include <type_traits>

#include "operator/hashmap/sveht32.hpp"

namespace omniruntime {
namespace op {

template <typename KeyType, typename ValueHandleType>
class SveJoinHashTable32 : public ::omniruntime::op::hashmap::SveHashTable32<KeyType, ValueHandleType> {
public:
    using SveBase = ::omniruntime::op::hashmap::SveHashTable32<KeyType, ValueHandleType>;
    using ResultType = InsertResult<ValueHandleType>;

    static_assert(std::is_same_v<KeyType, uint32_t>, "SveJoinHashTable32 key type must be uint32_t");
    static_assert(std::is_same_v<ValueHandleType, uint32_t>, "SveJoinHashTable32 value type must be uint32_t handle");

    explicit SveJoinHashTable32(uint8_t initDegree) : SveBase(initDegree) {}

    ResultType InsertJoinKeysToHashmap(KeyType key)
    {
        return SveBase::EmplaceNotNullKey(key);
    }

    int32_t ProbeBatchSVE(
        const KeyType *queryKeys,
        const KeyType *queryPayloads,
        int32_t numKeys,
        ValueHandleType *outputHandles,
        KeyType *outputPayloads) const
    {
        // StoreKey=false: join only needs handle + payload (probe row index).
        const size_t numKeysU = static_cast<size_t>(numKeys);
        if (SveBase::HasEmptyKeyValue()) {
            return static_cast<int32_t>(SveBase::template probe_sve<true, false, true>(
                queryKeys, queryPayloads, numKeysU, nullptr, outputHandles, outputPayloads));
        }
        return static_cast<int32_t>(SveBase::template probe_sve<true, false, false>(
            queryKeys, queryPayloads, numKeysU, nullptr, outputHandles, outputPayloads));
    }
};

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_SVE_JOIN_HASH_TABLE32_H
