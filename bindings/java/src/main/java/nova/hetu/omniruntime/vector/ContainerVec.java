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

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_VEC_ENCODING_CONTAINER;

import nova.hetu.omniruntime.type.ContainerDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.nio.ByteBuffer;

/**
 * container vec.
 *
 * @since 2021-08-05
 */
public class ContainerVec extends FixedWidthVec {
    private static final int BYTES = Long.BYTES;

    private int positionCount;

    private DataType[] dataTypes;

    /**
     * The routine will use the specialized vector allocator to allocate new vector.
     *
     * @param vectorCount the number of vector
     * @param positionCount the actual number of value of vector
     * @param vectorAddresses the address of vector
     * @param dataTypes the data type of this vector
     */
    public ContainerVec(int vectorCount, int positionCount, long[] vectorAddresses, DataType[] dataTypes) {
        super(vectorCount * BYTES, positionCount, OMNI_VEC_ENCODING_CONTAINER, ContainerDataType.CONTAINER);
        this.positionCount = positionCount;
        this.dataTypes = dataTypes;
        setDataTypesNative(getNativeVector(), DataTypeSerializer.serialize(dataTypes));
        put(vectorAddresses, 0, 0, vectorAddresses.length);
    }

    /**
     * this constructor is for JNI to initiate. The 'positionCount' is the number of
     * row of all vectors in this ContainerVec. The number of element in this
     * ContainerVec is the 'size' attribute of Vec.
     *
     * @param nativeVector native vector address
     */
    public ContainerVec(long nativeVector) {
        super(nativeVector, ContainerDataType.CONTAINER, BYTES);
        // get other attributes from native
        this.positionCount = getPositionNative(nativeVector);
        this.dataTypes = DataTypeSerializer.deserialize(getDataTypesNative(nativeVector));
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param nativeValueBufAddress valueBuf address of native vector
     * @param nativeVectorNullBufAddress nullBuf address of native vector
     * @param size the actual number of value of vector
     */
    public ContainerVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, size * BYTES, size,
                ContainerDataType.CONTAINER);
        // get other attributes from native
        this.positionCount = getPositionNative(nativeVector);
        this.dataTypes = DataTypeSerializer.deserialize(getDataTypesNative(nativeVector));
    }

    /**
     * This constructor of vector is just for shuffle compilation to pass, it will
     * be removed later.
     *
     * @param data data of vector
     * @param capacityInBytes size in bytes of data
     */
    @Deprecated
    public ContainerVec(ByteBuffer data, int capacityInBytes) {
        super(capacityInBytes, data.limit(), OMNI_VEC_ENCODING_CONTAINER, ContainerDataType.CONTAINER);
    }

    private ContainerVec(ContainerVec containerVec, int start, int length, DataType[] dataTypes) {
        super(containerVec, start, length, dataTypes.length * BYTES);
        this.positionCount = length;
        this.dataTypes = dataTypes;
    }

    private ContainerVec(ContainerVec vector, int[] positions, int offset, int length, DataType[] dataTypes) {
        super(vector, positions, offset, length, dataTypes.length * BYTES);
        this.positionCount = length;
        this.dataTypes = dataTypes;
    }

    private static native int getPositionNative(long nativeVector);

    private static native String getDataTypesNative(long nativeVector);

    private static native void setDataTypesNative(long nativeVector, String dataTypes);

    /**
     * get the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @return the value of long
     */
    public long get(int index) {
        return valuesBuf.getLong(index * BYTES);
    }

    /**
     * Sets the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, long value) {
        valuesBuf.setLong((index) * BYTES, value);
    }

    /**
     * Batch sets the specified long at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(long[] values, int offset, int start, int length) {
        valuesBuf.setLongArray(offset, values, start, length * BYTES);
    }

    /**
     * get position count.
     *
     * @return positionCount
     */
    public int getPositionCount() {
        return this.positionCount;
    }

    /**
     * get data types.
     *
     * @return dataTypes
     */
    public DataType[] getDataTypes() {
        return this.dataTypes;
    }

    /**
     * get the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @return get(index)
     */
    public long getVector(int index) {
        return get(index);
    }

    /**
     * get position count.
     *
     * @return positionCount
     */
    public int getSize() {
        return positionCount;
    }

    @Override
    public ContainerVec slice(int start, int length) {
        return new ContainerVec(this, start, length, dataTypes);
    }

    @Override
    public ContainerVec copyPositions(int[] positions, int offset, int length) {
        return new ContainerVec(this, positions, offset, length, dataTypes);
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return getCapacityInBytes();
    }

    @Override
    public VecEncoding getEncoding() {
        return OMNI_VEC_ENCODING_CONTAINER;
    }

    /**
     * get the encoding of vector at index position.
     *
     * @param index the element offset in vec
     * @return encoding of vector at index position
     */
    public VecEncoding getVecEncoding(int index) {
        return VecEncoding.values()[getVecEncodingNative(getVector(index))];
    }
}
