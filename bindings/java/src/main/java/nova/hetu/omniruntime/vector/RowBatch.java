/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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
     * construct row batch from vector batch (legacy: passes nmmHandle=0 → nullBridge)
     *
     * @param vb vector batch
     */
    public RowBatch(VecBatch vb) {
        this(vb, 0L);
    }

    /**
     * construct row batch from vector batch with a per-task memory pool address.
     *
     * @param vb vector batch
     * @param nmmHandle per-task pool address (jlong); 0L = legacy nullBridge path
     */
    public RowBatch(VecBatch vb, long nmmHandle) {
        long rb = transFromVectorBatch(vb.getNativeVectorBatch(), nmmHandle);
        this.rowCount = vb.getRowCount();
        this.nativeRowBatch = rb;
    }

    /**
     * construct row batch from rows (legacy: passes nmmHandle=0 → nullBridge)
     *
     * @param rows all rows of batch
     * @param rowCount row count of row batch
     */
    public RowBatch(Row[] rows, int rowCount) {
        this(rows, rowCount, 0L);
    }

    /**
     * construct row batch from rows with a per-task memory pool address.
     *
     * @param rows all rows of batch
     * @param rowCount row count of row batch
     * @param nmmHandle per-task pool address (jlong); 0L = legacy nullBridge path
     */
    public RowBatch(Row[] rows, int rowCount, long nmmHandle) {
        this(newRowBatchNative(rows, rowCount, nmmHandle), rows, rowCount);
    }

    // only release rowBatch
    private static native void freeRowBatchNative(long nativeVectorBatch);

    // The trailing `nmmHandle` is a raw OmniTaskMemoryPool* address as jlong.
    // 0L = legacy nullBridge path.
    private static native long newRowBatchNative(Row[] rows, int rowCount, long nmmHandle);

    private static native long transFromVectorBatch(long vbAddress, long nmmHandle);

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