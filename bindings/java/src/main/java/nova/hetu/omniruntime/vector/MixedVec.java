/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A read-only vector for native batch on mixed mode
 * Stores a MixedVectorBatch handler
 *
 * @since 2026-04-17
 */
public class MixedVec extends Vec {
    private long constValueLong;
    private AtomicBoolean isClosed = new AtomicBoolean(false);

    public MixedVec(long nativeVector) {
        super(nativeVector, 0L, 0L, 0,
                0, LongDataType.LONG, false);
        constValueLong = nativeVector;
    }

    @Override
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_VEC_ENCODING_FLAT;
    }

    public long getConstLong() {
        return constValueLong;
    }

    @Override
    public MixedVec slice(int start, int length) {
        throw new UnsupportedOperationException("ConstVec does not support slice");
    }

    @Override
    public Vec copyPositions(int[] positions, int offset, int length) {
        throw new UnsupportedOperationException("ConstVec does not support copyPositions");
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return 0;
    }

    @Override
    public void close() {
        if (isClosed.compareAndSet(false, true)) {
            freeMixedVectorBatchNative(this.nativeVector);
        } else {
            throw new OmniRuntimeException(OmniErrorType.OMNI_DOUBLE_FREE,
                "MixedVec has been closed:" + this + ",batchHandle:" + nativeVector +
                ",threadName:" + Thread.currentThread().getName());
        }
    }

    private static native void freeMixedVectorBatchNative(long batchHandle);
}
