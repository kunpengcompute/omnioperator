/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
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
import nova.hetu.omniruntime.utils.NullsBufHelper;
import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.io.Closeable;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * base class of vec.
 *
 * @since 2021-07-17
 */
public abstract class Vec implements Closeable {
    /**
     * indicates a null value in a nulls buffer.
     */
    public static final byte NULL = 1;

    /**
     * indicates a not null value in a nulls buffer.
     */
    public static final byte NOT_NULL = 0;

    static {
        OmniLibs.load();
    }

    /**
     * The value buffer.
     */
    protected OmniBuffer valuesBuf;

    /**
     * The nulls of vector, it is a bitmap.
     */
    protected OmniBuffer nullsBuf;

    /**
     * The native vector address.
     */
    protected final long nativeVector;

    /**
     * The capacity in bytes of this vector.
     */
    protected int capacityInBytes;

    /**
     * The actual number of value.
     */
    protected int size;

    /**
     * The {@link DataType} of this vector.
     */
    private DataType dataType;

    /**
     * When a vector has been sliced. The current vector and sliced vector are
     * unwritable.
     */
    private boolean isWritable = true;

    private boolean isCloseable = true;

    private AtomicBoolean isClosed = new AtomicBoolean(false);

    /**
     * The routine will use the specialized vector allocator to allocate new vector.
     *
     * @param capacityInBytes the capacity in bytes of vector
     * @param size the actual number of value of vector
     * @param encoding the encoding type of vector
     * @param datatype the data type of this vector
     */
    public Vec(int capacityInBytes, int size, VecEncoding encoding, DataType datatype) {
        this(newVectorNative(size, encoding.ordinal(), datatype.getId().toValue(), capacityInBytes), capacityInBytes,
                size, datatype, true);
    }

    public Vec(Vec dictionary, int[] ids, int capacityInBytes, DataType dataType) {
        this(newDictionaryVectorNative(dictionary.nativeVector, ids, ids.length, dataType.getId().toValue()),
                capacityInBytes, ids.length, dataType, true);
    }

    /**
     * The routine is just for slicing and copyRegion vector operator.
     *
     * @param vec the vector need to be sliced or copyRegion
     * @param offset When a vector has been sliced or copyRegion, this value will
     *            point to where is the new slice {@link Vec} start
     * @param length the number of value
     * @param capacityInBytes the number of capacityInBytes
     */
    protected Vec(Vec vec, int offset, int length, int capacityInBytes) {
        this(sliceVectorNative(vec.nativeVector, offset, length), capacityInBytes, length, vec.dataType, false);
    }

    /**
     * The routine is just for copyPosition vector operator.
     *
     * @param vec the vector need to be copy
     * @param positions the original vector positions
     * @param offset offset of positions in the input parameter
     * @param length number of elements copied
     * @param capacityInBytes the number of capacityInBytes
     */
    protected Vec(Vec vec, int[] positions, int offset, int length, int capacityInBytes) {
        this(copyPositionsNative(vec.nativeVector, positions, offset, length), capacityInBytes, length, vec.dataType,
                true);
    }

    /**
     * The routine will use native vector to initialize a new fixed width vector.
     *
     * @param nativeVector native vector address
     * @param dataType the type of this vector
     * @param typeLength the number of typeLength
     */
    protected Vec(long nativeVector, DataType dataType, int typeLength) {
        this(nativeVector, getSizeNative(nativeVector) * typeLength, getSizeNative(nativeVector), dataType, true);
    }

    /**
     * The routine will use native vector to initialize a new variable width vector.
     *
     * @param nativeVector native vector address
     * @param dataType the type of this vector
     */
    protected Vec(long nativeVector, DataType dataType) {
        this(nativeVector, getCapacityInBytesNative(nativeVector), getSizeNative(nativeVector), dataType, true);
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param nativeVectorValueBufAddress valueBuf address of native vector
     * @param nativeVectorNullBufAddress nullBuf address of native vector
     * @param capacityInBytes capacity in bytes of vector
     * @param size the actual number of value of vector
     * @param dataType the type of this vector
     */
    protected Vec(long nativeVector, long nativeVectorValueBufAddress, long nativeVectorNullBufAddress,
            int capacityInBytes, int size, DataType dataType) {
        this(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, capacityInBytes, size, dataType,
                true);
    }

    protected Vec(long nativeVector, long nativeVectorValueBufAddress, long nativeVectorNullBufAddress,
            int capacityInBytes, int size, DataType dataType, boolean isWritable) {
        this.capacityInBytes = capacityInBytes;
        this.size = size;
        this.dataType = dataType;
        this.nativeVector = nativeVector;
        this.valuesBuf = OmniBufferFactory.create(nativeVectorValueBufAddress, capacityInBytes);
        this.nullsBuf = OmniBufferFactory.create(nativeVectorNullBufAddress, NullsBufHelper.nBytes(size));
        this.isWritable = isWritable;
    }

    private Vec(long nativeVector, int capacityInBytes, int size, DataType dataType, boolean isWritable) {
        this(nativeVector, getValuesNative(nativeVector), getValueNullsNative(nativeVector), capacityInBytes, size,
                dataType, isWritable);
    }

    private static native long newVectorNative(int size, int vecEncodingId, int dataTypeId, int capacityInBytes);

    private static native long newDictionaryVectorNative(long dictionaryNativeVector, int[] ids, int size,
            int dataTypeId);

    private static native void freeVectorNative(long nativeVector);

    private static native long sliceVectorNative(long nativeVector, int offset, int length);

    private static native long copyPositionsNative(long nativeVector, int[] positions, int offset, int length);

    /**
     * get capacity in Bytes from native vector
     *
     * @param nativeVector nativeVector address
     * @return the CapacityInBytes of native vector
     */
    protected static native int getCapacityInBytesNative(long nativeVector);

    /**
     * get size of native vector.
     *
     * @param nativeVector native vector
     * @return size
     */
    protected static native int getSizeNative(long nativeVector);

    private static native int setSizeNative(long nativeVector, int valueCount);

    /**
     * get value address from native vector.
     *
     * @param nativeVector native vector address
     * @return value address of native vector
     */
    protected static native long getValuesNative(long nativeVector);

    /**
     * get encoding of native vector.
     *
     * @param nativeVector native vector
     * @return encoding type id
     */
    protected static native int getVecEncodingNative(long nativeVector);

    /**
     * get null address of native vector.
     *
     * @param nativeVector native vector
     * @return null address of native vector
     */
    protected static native long getValueNullsNative(long nativeVector);

    /**
     * merge two vectors.
     *
     * @param destNativeVector target native vector
     * @param positionOffset position offset
     * @param srcNativeVector source native vector
     * @param length the number of element
     */
    protected static native void appendVectorNative(long destNativeVector, int positionOffset, long srcNativeVector,
            int length);

    private static native void setNullFlagNative(long nativeVector, boolean hasNull);

    private static native boolean hasNullNative(long nativeVector);

    /**
     * get native vector.
     *
     * @return native vector address
     */
    public long getNativeVector() {
        return nativeVector;
    }

    /**
     * the size of vector.
     *
     * @return size
     */
    public int getSize() {
        return size;
    }

    /**
     * set size of vector.
     * @param size size value
     */
    public void setSize(int size) {
        this.size = size;
        setSizeNative(nativeVector, size);
    }

    /**
     * capacity in bytes of vector.
     *
     * @return capacity
     */
    public int getCapacityInBytes() {
        return capacityInBytes;
    }

    /**
     * vector data type.
     *
     * @return vector data type
     */
    public DataType getType() {
        return dataType;
    }

    /**
     * vector encoding.
     *
     * @return vector encoding
     */
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_VEC_ENCODING_FLAT;
    }

    /**
     * get values buffer.
     *
     * @return values buffer
     */
    public OmniBuffer getValuesBuf() {
        return valuesBuf;
    }

    /**
     * set values buffer. VarcharVec cannot use this interface.
     *
     * @param buf buf of data
     */
    public void setValuesBuf(byte[] buf) {
        valuesBuf.setBytes(0, buf, 0, buf.length);
    }

    /**
     * set values buffer and length. VarcharVec cannot use this interface.
     *
     * @param buf buf of data
     * @param length the number of element
     */
    public void setValuesBuf(byte[] buf, int length) {
        valuesBuf.setBytes(0, buf, 0, length);
    }

    /**
     * get value nulls buffer.
     *
     * @return nulls value buffer
     */
    public OmniBuffer getValueNullsBuf() {
        return nullsBuf;
    }

    /**
     * specify whether the position element is null.
     *
     * @param index the element offset in vec
     * @return if it is null, return true, otherwise return false
     */
    public boolean isNull(int index) {
        return NullsBufHelper.isSet(nullsBuf, index) == 1;
    }

    /**
     * set the element at the specified position to a null value.
     *
     * @param index the element offset in vec
     */
    public void setNull(int index) {
        NullsBufHelper.setBit(nullsBuf, index);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * set nulls in batch.
     *
     * @param index the offset of the element
     * @param isNulls array of null values, true is null otherwise non-null.
     * @param start array offset
     * @param length number of elements
     */
    public void setNulls(int index, boolean[] isNulls, int start, int length) {
        byte[] values = transformBooleanToByte(isNulls, start, length);
        NullsBufHelper.setBit(nullsBuf, index, values, 0, length);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * set nulls in batch.
     *
     * @param index the offset of the element
     * @param isNulls array of null values, true is null otherwise non-null.
     * @param start array offset
     * @param length number of elements
     */
    public void setNulls(int index, byte[] isNulls, int start, int length) {
        NullsBufHelper.setBit(nullsBuf, index, isNulls, start, length);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * set nulls in batch.
     *
     * @param index the offset of the element
     * @param isBitNulls array of null values, true is null otherwise non-null. (Bit)
     * @param start array offset (Bit)
     * @param length number of elements (Bit)
     */
    public void setNullsByBits(int index, byte[] isBitNulls, int start, int length) {
        NullsBufHelper.setBitByBits(nullsBuf, index, isBitNulls, start, length);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * set nulls buffer.
     *
     * @param buf buf of null
     */
    public void setNullsBuf(byte[] buf) {
        nullsBuf.setBytes(0, buf, 0, buf.length);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * set nulls buffer and length.
     *
     * @param buf buf of null
     * @param length the number of element
     */
    public void setNullsBuf(byte[] buf, int length) {
        nullsBuf.setBytes(0, buf, 0, length);
        setNullFlagNative(nativeVector, true);
    }

    /**
     * whether there is a null value.
     *
     * @return if yes, return true otherwise false
     */
    public boolean hasNull() {
        return hasNullNative(nativeVector);
    }

    /**
     * transform boolean array to byte array.
     *
     * @param values nulls array
     * @param start array offset
     * @param length number of elements
     * @return byte array
     */
    protected byte[] transformBooleanToByte(boolean[] values, int start, int length) {
        byte[] transformedBytes = new byte[length];
        for (int i = 0; i < length; i++) {
            if (values[i + start]) {
                transformedBytes[i] = (byte) 1;
            } else {
                transformedBytes[i] = (byte) 0;
            }
        }

        return transformedBytes;
    }

    /**
     * transform byte array to boolean array.
     *
     * @param values byte array, 1 means null, 0 means non-null
     * @param start array offset
     * @param length number of elements
     * @return boolean array
     */
    protected boolean[] transformByteToBoolean(byte[] values, int start, int length) {
        boolean[] transformedBoolean = new boolean[length];
        for (int i = 0; i < length; i++) {
            transformedBoolean[i] = values[i + start] == 1;
        }
        return transformedBoolean;
    }

    /**
     * return null value array from 0 to size + offset length.
     *
     * @return raw value nulls
     * @return raw nulls array
     */
    public byte[] getRawValueNulls() {
        // the length of the array is size + offset, so that the caller
        // and vec can have the same offset.
        byte[] rawValueNulls = new byte[NullsBufHelper.nBytes(size)];
        nullsBuf.getBytes(0, rawValueNulls, 0, rawValueNulls.length);
        return rawValueNulls;
    }

    /**
     * get the specified nulls array at the specified absolute.
     *
     * @param index the offset of element in vec
     * @param length the number of element
     * @return boolean array
     */
    public boolean[] getValuesNulls(int index, int length) {
        byte[] nullsArray = new byte[length];
        NullsBufHelper.getBytes(nullsBuf, index, nullsArray, 0, length);
        return transformByteToBoolean(nullsArray, 0, length);
    }

    /**
     * is vec writable.
     *
     * @return if it is writable, return true, otherwise return false
     */
    public boolean isWritable() {
        return isWritable;
    }

    /**
     * split a vec into two vec according to the specified index and length.
     *
     * @param start starting index
     * @param length number of elements
     * @return new vec
     */
    public abstract Vec slice(int start, int length);

    /**
     * copy a new vec based on the positions.
     *
     * @param positions all positions in vec
     * @param offset position offset
     * @param length the number of elements to be copied
     * @return new vec
     */
    public abstract Vec copyPositions(int[] positions, int offset, int length);

    /**
     * This method takes input a source vector to append to the destination vector
     * only If the destination vector has enough available positions.
     *
     * @param other Source Vector to be appended
     * @param offset Number of Positions already occupied
     * @param length Number of Positions in the Source Vector
     */
    public void append(Vec other, int offset, int length) {
        appendVectorNative(this.nativeVector, offset, other.nativeVector, length);
    }

    @Override
    public void close() {
        if (!isCloseable) {
            return;
        }
        if (isClosed.compareAndSet(false, true)) {
            freeVectorNative(this.nativeVector);
        } else {
            throw new OmniRuntimeException(OmniErrorType.OMNI_DOUBLE_FREE, "vec has been closed:" + this
                    + ",threadName:" + Thread.currentThread().getName() + ",native:" + nativeVector);
        }
    }

    /**
     * vec is closed.
     *
     * @return true is closed, otherwise it it not closed.
     */
    public boolean isClosed() {
        return isClosed.get();
    }

    /**
     * set whether vec can be closed.
     *
     * @param isCloseable can vec be closed
     */
    public void setClosable(boolean isCloseable) {
        this.isCloseable = isCloseable;
    }

    /**
     * returns the number of bytes of the data written.
     *
     * @return length in bytes
     */
    public abstract int getRealValueBufCapacityInBytes();

    /**
     * returns the number of bytes of the null buf.
     *
     * @return length in bytes
     */
    public int getRealNullBufCapacityInBytes() {
        return NullsBufHelper.nBytes(size);
    }

    /**
     * returns the number of bytes of the offsets, for VarcharVec returned according
     * to size calculation, other types of vec return 0.
     *
     * @return length in bytes
     */
    public int getRealOffsetBufCapacityInBytes() {
        return 0;
    }

    void setDataType(DataType dataType) {
        this.dataType = dataType;
    }
}