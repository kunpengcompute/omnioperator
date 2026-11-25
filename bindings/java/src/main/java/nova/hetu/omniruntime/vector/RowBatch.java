/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * row batch : to collect all row.
 *
 * @since 2024-05-16
 */
public class RowBatch implements AutoCloseable {
    /**
     * native address of row batch.
     */
    protected final long nativeRowBatch;

    private Row[] rows;

    private int rowCount;

    private AtomicBoolean isClosed = new AtomicBoolean(false);

    /**
     * construct row batch
     *
     * @param nativeAddress address of row batch
     * @param rows actual rows
     * @param rowCount total row count of batch
     */
    public RowBatch(long nativeAddress, Row[] rows, int rowCount) {
        this.rows = rows;
        this.rowCount = rowCount;
        this.nativeRowBatch = nativeAddress;
    }

    /**
     * construct row batch from vector batch
     *
     * @param vb vector batch
     */
    public RowBatch(VecBatch vb) {
        long rb = transFromVectorBatch(vb.getNativeVectorBatch());
        this.rowCount = vb.getRowCount();
        this.nativeRowBatch = rb;
    }

    /**
     * construct row batch from rows
     *
     * @param rows all rows of batch
     * @param rowCount row count of row batch
     */
    public RowBatch(Row[] rows, int rowCount) {
        this(newRowBatchNative(rows, rowCount), rows, rowCount);
    }

    // only release rowBatch
    private static native void freeRowBatchNative(long nativeVectorBatch);

    private static native long newRowBatchNative(Row[] rows, int rowCount);

    private static native long transFromVectorBatch(long vbAddress);

    public long getNativeRowBatch() {
        return nativeRowBatch;
    }

    public Row[] getRows() {
        return rows;
    }

    public int getRowCount() {
        return rowCount;
    }

    @Override
    public void close() {
        if (isClosed.compareAndSet(false, true)) {
            freeRowBatchNative(getNativeRowBatch());
        } else {
            throw new OmniRuntimeException(OmniErrorType.OMNI_DOUBLE_FREE, "row batch has been closed:" + this
                    + ",threadName:" + Thread.currentThread().getName() + ",native:" + nativeRowBatch);
        }
    }
}
