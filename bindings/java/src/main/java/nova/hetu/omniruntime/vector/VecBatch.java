/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
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

import nova.hetu.omniruntime.OmniLibs;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.io.Closeable;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * vec batch.
 *
 * @since 2021-07-17
 */
public class VecBatch implements Closeable {
    // 加载jni所需动态库
    static {
        OmniLibs.load();
    }

    private final Vec[] vectors;

    private final int rowCount;

    private final long nativeVectorBatch;

    private AtomicBoolean isClosed = new AtomicBoolean(false);

    /**
     * The routine will use vectors and row count to initialize a new vector.
     *
     * @param vectors vectors in vecBatch,it may be empty
     * @param rowCount the row count of vector batch
     */
    public VecBatch(Vec[] vectors, int rowCount) {
        this.vectors = vectors;
        this.rowCount = rowCount;
        long[] nativeVectors = new long[vectors.length];
        for (int i = 0; i < vectors.length; i++) {
            nativeVectors[i] = vectors[i].getNativeVector();
        }
        this.nativeVectorBatch = newVectorBatchNative(nativeVectors, rowCount);
    }

    /**
     * it is recommended to use the VecBatch(Vec[] vectors, rowCount) construct to
     * prevent exceptions caused by empty vectors.
     *
     * @param vectors vectors in vecBatch,it may be empty
     */
    public VecBatch(Vec[] vectors) {
        this(vectors, vectors[0].getSize());
    }

    public VecBatch(List<Vec> vectors, int rowCount) {
        this(vectors.toArray(new Vec[vectors.size()]), rowCount);
    }

    /**
     * it is recommended to use the VecBatch(List<Vec>, rowCount) construct to
     * prevent exceptions caused by empty vectors.
     *
     * @param vectors vectors in vecBatch,it may be empty
     */
    public VecBatch(List<Vec> vectors) {
        this(vectors.toArray(new Vec[vectors.size()]));
    }

    /**
     * This constructor is for native to call.
     *
     * @param nativeVecBatch native vector batch address
     * @param nativeVectors native vector array
     * @param nativeVectorValueBufAddresses valueBuf address of native vector
     * @param nativeVectorNullBufAddresses nullBuf address of native vector
     * @param nativeVectorOffsetBufAddresses offsetBuf address of native vector
     * @param encodings the encoding type array of vector batch
     * @param dataTypeIds the type array of this vector batch
     * @param rowCount the row count of vector batch
     */
    public VecBatch(long nativeVecBatch, long[] nativeVectors, long[] nativeVectorValueBufAddresses,
            long[] nativeVectorNullBufAddresses, long[] nativeVectorOffsetBufAddresses, int[] encodings,
            int[] dataTypeIds, int rowCount) {
        int vecCount = nativeVectors.length;
        Vec[] newVectors = new Vec[vecCount];
        for (int idx = 0; idx < vecCount; idx++) {
            long nativeVector = nativeVectors[idx];
            DataType dataType = DataType.create(dataTypeIds[idx]);
            newVectors[idx] = VecFactory.create(nativeVector, nativeVectorValueBufAddresses[idx],
                    nativeVectorNullBufAddresses[idx], nativeVectorOffsetBufAddresses[idx], rowCount,
                    VecEncoding.values()[encodings[idx]], dataType);
        }
        this.rowCount = rowCount;
        this.nativeVectorBatch = nativeVecBatch;
        this.vectors = newVectors;
    }

    /**
     * create vector batch based on the number of vectors.
     *
     * @param nativeVectors native vector array
     * @param rowCount the row count of vector batch
     * @return vector batch address
     */
    public static native long newVectorBatchNative(long[] nativeVectors, int rowCount);

    /**
     * release vector batch.
     *
     * @param nativeVectorBatch vector batch address
     */
    public static native void freeVectorBatchNative(long nativeVectorBatch);

    /**
     * row count in the vecBatch.
     *
     * @return row count
     */
    public int getRowCount() {
        return rowCount;
    }

    /**
     * vector count in the vecBatch.
     *
     * @return vector count
     */
    public int getVectorCount() {
        return vectors.length;
    }

    public Vec[] getVectors() {
        return vectors;
    }

    /**
     * get specified vector at the specified absolute.
     *
     * @param index the element offset in vec
     * @return vector
     */
    public Vec getVector(int index) {
        return vectors[index];
    }

    public long getNativeVectorBatch() {
        return nativeVectorBatch;
    }

    /**
     * release all vectors resource of vector batch.
     */
    public void releaseAllVectors() {
        for (Vec vector : vectors) {
            vector.close();
        }
    }

    @Override
    public void close() {
        if (isClosed.compareAndSet(false, true)) {
            freeVectorBatchNative(nativeVectorBatch);
        } else {
            throw new OmniRuntimeException(OmniErrorType.OMNI_DOUBLE_FREE, "vec batch has been closed:" + this
                    + ",threadName:" + Thread.currentThread().getName() + ",native:" + nativeVectorBatch);
        }
    }
}
