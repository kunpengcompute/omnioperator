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

package nova.hetu.omniruntime.memory;

import nova.hetu.omniruntime.OmniLibs;
import nova.hetu.omniruntime.utils.ParseUtil;
import nova.hetu.omniruntime.vector.JvmUtils;

/**
 * memory manager.
 *
 * @since 2023-01-17
 */
public class MemoryManager implements AutoCloseable {
    /**
     * -1 means no memory limit
     */
    public static final long UNLIMITED = -1;

    static {
        OmniLibs.load();
    }

    /**
     * set the size of the memory that can be used by the root allocator,
     * this can limit memory usage at the process level
     */
    public static void setGlobalMemoryLimit() {
        // parse environment variable OMNI_OFFHEAP_MEMORY_SIZE
        String memorySize = System.getenv("OMNI_OFFHEAP_MEMORY_SIZE");
        long rootLimit = memorySize == null ? JvmUtils.estimateMaxDirectMemory() : ParseUtil.parserMemoryParameters(memorySize);
        // the off heap memory from director or environment variable, set global memory limit
        setGlobalMemoryLimit(rootLimit);
    }

    /**
     * set global memory limit about off-heap
     *
     * @param limit the number of global memory limit about off-heap
     * */
    public static void setGlobalMemoryLimit(long limit) {
        setGlobalMemoryLimitNative(limit);
    }

    /**
     * get allocated memory of current allocator
     *
     * @return allocated memory in bytes
     */
    public long getAllocatedMemory() {
        return getAllocatedMemoryNative();
    }

    /**
     * clear memory of current task and current executor
     * */
    public static void clearMemory() {
        memoryClearNative();
    }

    /**
     * Reclaim memory of current task if memory leak exists
     * */
    public static void reclaimMemory() {
        memoryReclamationNative();
    }

    @Override
    public void close() {}

    private static native void setGlobalMemoryLimitNative(long limit);

    private static native long getAllocatedMemoryNative();

    private static native long memoryClearNative();

    private static native long memoryReclamationNative();
}
