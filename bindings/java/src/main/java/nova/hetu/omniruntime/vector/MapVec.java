/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_ENCODING_MAP;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.MapDataType;

/**
 * map vec.
 *
 * @since 2025-8-29
 */
public class MapVec extends ComplexVec {

    private Vec keyVec;
    private Vec valueVec;
    protected OmniBuffer offsetsBuf;

    public MapVec(MapDataType type, int size) {
        this(type, size, false, 0L);
    }

    public MapVec(MapDataType type, int size, boolean isEmpty) {
        this(type, size, isEmpty, 0L);
    }

    // Pool-aware ctors. nmmHandle is the per-task OmniTaskMemoryPool address so the
    // complex wrapper's top-level offsets + validity buffers are pool-accounted
    // (0L = no pool). Mirrors Vec's nmmHandle convention.
    public MapVec(MapDataType type, int size, long nmmHandle) {
        this(type, size, false, nmmHandle);
    }

    public MapVec(MapDataType type, int size, boolean isEmpty, long nmmHandle) {
        this(isEmpty ? newEmptyComplexVectorNative(size, OMNI_ENCODING_MAP.ordinal(), new DataType[]{type.getKeyType(), type.getValueType()}, nmmHandle)
                : newComplexVectorNative(size, OMNI_ENCODING_MAP.ordinal(), new DataType[]{type.getKeyType(), type.getValueType()}, nmmHandle), type, size, isEmpty);
    }

    public MapVec(long nativeVector, MapDataType type) {
        this(nativeVector, type, getSizeNative(nativeVector), false);
    }

    public MapVec(long nativeVector, MapDataType type, int size) {
        this(nativeVector, type, size, false);
    }

    public MapVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size, MapDataType type) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress,
            getComplexCapacityNative(nativeVector, OMNI_ENCODING_MAP.ordinal()), size, type);
        this.keyVec = createVec(getKeysAddrNative(nativeVector), type.getKeyType());
        this.valueVec = createVec(getValuesAddrNative(nativeVector), type.getValueType());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()), (size + 1) * Long.BYTES);
    }

    public MapVec(long nativeVector, MapDataType type, int size, boolean isEmpty) {
        super(nativeVector, getComplexCapacityNative(nativeVector, OMNI_ENCODING_MAP.ordinal()), size, type);
        if (!isEmpty){
            this.keyVec = createVec(getKeysAddrNative(nativeVector), type.getKeyType());
            this.valueVec = createVec(getValuesAddrNative(nativeVector), type.getValueType());
        }
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()), (size + 1) * Long.BYTES);
    }

    private MapVec(MapVec vector, int offset, int length) {
        super(vector, offset, length, getComplexCapacityNative(vector.getNativeVector(), OMNI_ENCODING_MAP.ordinal()));
        this.keyVec = createVec(getKeysAddrNative(nativeVector), ((MapDataType) getType()).getKeyType());
        this.valueVec = createVec(getValuesAddrNative(nativeVector), ((MapDataType) getType()).getValueType());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()), (length + 1) * Long.BYTES);
    }

    @Override
    public MapVec slice(int start, int length) {
        return new MapVec(this, start, length);
    }

    @Override
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_ENCODING_MAP;
    }

    @Override
    public Vec copyPositions(int[] positions, int offset, int length) {
        return null;
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return 0;
    }

    @Override
    public int getRealOffsetBufCapacityInBytes() {
        return (size + 1) * Long.BYTES;
    }

    public long getOffset(long rowId) {
        return getOffsetNative(nativeVector, rowId);
    }

    public long getSize(long rowId) {
        return getSizeNative(nativeVector, rowId);
    }

    public void AddKeys(Vec keys){
        this.keyVec = keys;
        AddKeysNative(this.nativeVector, keys.nativeVector);
    }

    public void AddValues(Vec values){
        this.valueVec = values;
        AddValuesNative(this.nativeVector, values.nativeVector);
    }

    public void AddOffsets(int[] offsets){
        AddOffsetsNative(this.nativeVector, offsets);
    }

    public void setSize(int index, int size){
        setSizeByIndexNative(this.nativeVector, index, size);
    }

    protected static native long setSizeByIndexNative(long nativeVector, int index, int size);

    protected static native long getKeysAddrNative(long nativeVector);

    protected static native long getValuesAddrNative(long nativeVector);

    protected static native long getOffsetNative(long nativeVector, long rowId);

    protected static native long getSizeNative(long nativeVector, long rowId);

    protected static native void AddKeysNative(long nativeVector, long keys);

    protected static native void AddValuesNative(long nativeVector, long values);

    protected static native void AddOffsetsNative(long nativeVector, int[] offsets);

    protected static native long getValueOffsetsNative(long nativeVector);

    public Vec getKeyVec() {
        return keyVec;
    }

    public Vec getValueVec() {
        return valueVec;
    }

    public OmniBuffer getOffsetsBuf() {
        return offsetsBuf;
    }

    public void setOffsetsBuf(byte[] buf) {
        offsetsBuf.setBytes(0, buf, 0, buf.length);
    }
}
