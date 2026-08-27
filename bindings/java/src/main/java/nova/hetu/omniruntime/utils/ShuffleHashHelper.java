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
     * use to compute shuffle hash partitionIds
     *
     * @param vecAddrArray the array of nativeVec
     * @param partitionNum the num of partition
     * @param rowCount the num of row
     * @return the partitionIds of vec
     */
    public static native long computePartitionIds(long[] vecAddrArray, int partitionNum, int rowCount);

    public static native long computeMixedPartitionIds(long handler, int partitionNum, int rowCount);
}
