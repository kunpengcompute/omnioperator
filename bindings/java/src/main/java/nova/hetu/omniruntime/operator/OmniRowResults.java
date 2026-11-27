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

package nova.hetu.omniruntime.operator;

import nova.hetu.omniruntime.constants.Status;
import nova.hetu.omniruntime.vector.RowBatch;

import java.io.Closeable;

/**
 * The type Omni results.
 *
 * @since 2024-05-16
 */
public class OmniRowResults implements Closeable {
    private final RowBatch rowBatch;

    private final Status status;

    /**
     * Instantiates a new Omni results.
     *
     * @param rowBatch the vec batch
     * @param status the status
     */
    public OmniRowResults(RowBatch rowBatch, int status) {
        this.rowBatch = rowBatch;
        this.status = new Status(status);
    }

    /**
     * Get vec batches vec batch [ ].
     *
     * @return the vec batch [ ]
     */
    public RowBatch getRowBatch() {
        return rowBatch;
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
        if (rowBatch != null) {
            rowBatch.close();
        }
    }
}
