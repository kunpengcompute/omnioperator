/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.DataType;

/**
 * base class of fixed width vec.
 *
 * @since 2021-07-17
 */
public abstract class FixedWidthVec extends Vec {
    public FixedWidthVec(int capacityInBytes, int size, VecEncoding encoding, DataType type) {
        this(capacityInBytes, size, encoding, type, 0L);
    }

    public FixedWidthVec(int capacityInBytes, int size, VecEncoding encoding, DataType type, long nmmHandle) {
        super(capacityInBytes, size, encoding, type, nmmHandle);
    }

    public FixedWidthVec(Vec dictionary, int[] ids, int capacityInBytes, DataType type) {
        super(dictionary, ids, capacityInBytes, type);
    }

    public FixedWidthVec(FixedWidthVec vector, int offset, int length, int capacityInBytes) {
        super(vector, offset, length, capacityInBytes);
    }

    public FixedWidthVec(FixedWidthVec vector, int[] positions, int offset, int length, int capacityInBytes) {
        super(vector, positions, offset, length, capacityInBytes);
    }

    public FixedWidthVec(long nativeVector, DataType type, int typeLength) {
        super(nativeVector, type, typeLength);
    }

    public FixedWidthVec(long nativeVector, long nativeVectorValueBufAddress, long nativeVectorNullBufAddress,
            int capacityInBytes, int size, DataType type) {
        super(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, capacityInBytes, size, type);
    }
}
