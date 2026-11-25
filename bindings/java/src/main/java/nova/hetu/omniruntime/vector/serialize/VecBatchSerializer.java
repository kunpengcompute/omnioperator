/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
}