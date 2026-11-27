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

package nova.hetu.omniruntime.vector;

import java.io.Closeable;

/**
 * row vec.
 *
 * @since 2024-05-16
 */
public class Row implements Closeable {
    /**
     * indicates native buffer address in cpp.
     */
    protected final long nativeRow;

    private int length;

    private OmniBuffer rowBuf;

    /**
     * new one row in c++
     *
     * @param dataAddr native address in cpp
     * @param len number of row's bytes
     */
    public Row(long dataAddr, int len) {
        nativeRow = dataAddr;
        length = len;
        this.rowBuf = OmniBufferFactory.create(dataAddr, len);
    }

    /**
     * return one row's capacity
     *
     * @return buf's capacity
     */
    public int getCapacity() {
        return rowBuf.getCapacity();
    }

    /**
     * return one row's real length
     *
     * @return buf's length
     */
    public int getLength() {
        return length;
    }

    /**
     * return one row's native address
     *
     * @return row's native address
     */
    public long getNativeRow() {
        return nativeRow;
    }

    /**
     * it may close raw buffer 's memory, now memory is deleted by adaptor
     */
    @Override
    public void close() {

    }

    /**
     * return heap bytes object
     *
     * @return bytes object
     */
    public byte[] getBytes() {
        return rowBuf.getBytes(0, length);
    }
}
