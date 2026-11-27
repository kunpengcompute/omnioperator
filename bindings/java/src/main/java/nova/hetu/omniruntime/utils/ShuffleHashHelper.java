/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

package nova.hetu.omniruntime.utils;

/**
 * shuffle hash
 *
 * @since 2025-03-21
 */
public class ShuffleHashHelper {
    /**
     * use to compute shuffle hash partitionIds.
     *
     * <p>The trailing {@code nmmHandle} parameter is a raw
     * {@code omniruntime::mem::OmniTaskMemoryPool*} address (as a {@code long}),
     * obtained from
     * {@code OmniNativeMemoryManagerJniWrapper.getPoolAddress(gluten-mm-handle)}.
     * The omni-side native helper decodes this address to a pool-bound Allocator
     * and routes the partitionIds Vector allocation through the per-task pool.
     * Pass 0L when no task pool is available (test / mock) — the helper falls
     * back to the legacy unbound Vector ctor.
     *
     * @param vecAddrArray the array of nativeVec
     * @param partitionNum the num of partition
     * @param rowCount the num of row
     * @param nmmHandle raw OmniTaskMemoryPool* address (0 = no pool, legacy path)
     * @return the partitionIds of vec
     */
    public static native long computePartitionIds(long[] vecAddrArray, int partitionNum, int rowCount,
            long nmmHandle);
}
