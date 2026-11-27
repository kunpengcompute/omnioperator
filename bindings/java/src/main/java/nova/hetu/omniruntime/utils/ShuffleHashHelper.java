/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
}
