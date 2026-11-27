/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

package nova.hetu.omniruntime.vector.serialize;

import nova.hetu.omniruntime.vector.VecBatch;

/**
 * define the serialization interface of VecBatch.
 *
 * @since 2021-09-13
 */
public interface VecBatchSerializer {
    /**
     * serialize vecBatch.
     *
     * @param vecBatch the vecbatch to be serialized
     * @return byte array
     */
    byte[] serialize(VecBatch vecBatch);

    /**
     * deserialize vecbatch.
     *
     * @param bytes serialized vecbatch
     * @return vec batch
     */
    VecBatch deserialize(byte[] bytes);

    /**
     * deserialize vecbatch with a per-task pool handle (0L = no pool).
     *
     * @param bytes serialized vecbatch
     * @param nmmHandle per-task OmniTaskMemoryPool address (0 = no pool)
     * @return vec batch
     */
    default VecBatch deserialize(byte[] bytes, long nmmHandle) {
        return deserialize(bytes);
    }
}