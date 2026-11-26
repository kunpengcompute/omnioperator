/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package nova.hetu.omniruntime.operator;

import nova.hetu.omniruntime.constants.Status;
import nova.hetu.omniruntime.vector.VecBatch;

import java.io.Closeable;

/**
 * The type Omni results.
 *
 * @since 2021-06-30
 */
public class OmniResults implements Closeable {
    private final VecBatch vecBatch;

    private final Status status;

    /**
     * Instantiates a new Omni results.
     *
     * @param vecBatch the vec batch
     * @param status the status
     */
    public OmniResults(VecBatch vecBatch, int status) {
        this.vecBatch = vecBatch;
        this.status = new Status(status);
    }

    /**
     * Get vec batches vec batch [ ].
     *
     * @return the vec batch [ ]
     */
    public VecBatch getVecBatch() {
        return vecBatch;
    }

    /**
     * Gets status.
     *
     * @return the status
     */
    public Status getStatus() {
        return status;
    }

    @Override
    public void close() {
        if (vecBatch != null) {
            vecBatch.releaseAllVectors();
            vecBatch.close();
        }
    }
}
