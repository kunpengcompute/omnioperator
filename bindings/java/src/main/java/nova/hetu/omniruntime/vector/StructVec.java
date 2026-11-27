/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_ENCODING_STRUCT;

import nova.hetu.omniruntime.type.StructDataType;

/**
 * struct vec.
 *
 * @since 2025-8-29
 */
public class StructVec extends ComplexVec {

    private Vec[] children;

    public StructVec(StructDataType type, int size) {
        this(type, size, false, 0L);
    }

    public StructVec(StructDataType type, int size, boolean isEmpty) {
        this(type, size, isEmpty, 0L);
    }

    // Pool-aware ctors. nmmHandle is the per-task OmniTaskMemoryPool address so the
    // complex wrapper's top-level validity buffer is pool-accounted (0L = no pool).
    // Mirrors Vec's nmmHandle convention.
    public StructVec(StructDataType type, int size, long nmmHandle) {
        this(type, size, false, nmmHandle);
    }

    public StructVec(StructDataType type, int size, boolean isEmpty, long nmmHandle) {
        this(isEmpty ? newEmptyComplexVectorNative(size, OMNI_ENCODING_STRUCT.ordinal(), type.getFieldTypes(), nmmHandle)
                : newComplexVectorNative(size, OMNI_ENCODING_STRUCT.ordinal(), type.getFieldTypes(), nmmHandle), type, size, isEmpty);
    }
    
    public StructVec(long nativeVector, StructDataType type) {
        this(nativeVector, type, getSizeNative(nativeVector), false);
    }

    public StructVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size, StructDataType type) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress,
            getComplexCapacityNative(nativeVector, OMNI_ENCODING_STRUCT.ordinal()), size, type);
        children = new Vec[type.getFieldTypes().length];
        initChildren(type);
    }

    public StructVec(long nativeVector, StructDataType type, int size) {
        this(nativeVector, type, size, false);
    }

    public StructVec(long nativeVector, StructDataType type, int size, boolean isEmpty) {
        super(nativeVector, getComplexCapacityNative(nativeVector, OMNI_ENCODING_STRUCT.ordinal()), size, type);
        children = new Vec[type.getFieldTypes().length];
        if (!isEmpty) {
            initChildren(type);
        }
    }

    private StructVec(StructVec vector, int offset, int length) {
        super(vector, offset, length, getComplexCapacityNative(vector.getNativeVector(), OMNI_ENCODING_STRUCT.ordinal()));
        children = new Vec[((StructDataType) getType()).getFieldTypes().length];
        initChildren((StructDataType) getType());
    }

    private void initChildren(StructDataType type) {
        children = new Vec[type.getFieldTypes().length];
        for (int i = 0; i < children.length; i++) {
            children[i] = createVec(getChildAddrNative(nativeVector, i), type.getFieldType(i));
        }
    }

    @Override
    public StructVec slice(int start, int length) {
        return new StructVec(this, start, length);
    }

    @Override
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_ENCODING_STRUCT;
    }

    @Override
    public Vec copyPositions(int[] positions, int offset, int length) {
        return null;
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return 0;
    }

    public Vec[] getChildren() {
        return children;
    }

    public Vec getChild(int index) {
        return children[index];
    }

    public void setChild(int index, Vec vec) {
        children[index] = vec;
        setChildNative(this.nativeVector, index, vec.nativeVector);
    }

    public void addChild(Vec addedVec) {
        addChildNative(this.nativeVector, addedVec.nativeVector);
    }

    protected static native void setChildNative(long nativeVector, int index, long vec);

    protected static native void addChildNative(long nativeVector, long addedVec);

    protected static native long getChildAddrNative(long nativeVector, long vec);
}
