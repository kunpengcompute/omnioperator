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

import nova.hetu.omniruntime.type.DataType;

/**
 * base class of fixed width vec.
 *
 * @since 2021-07-17
 */
public abstract class FixedWidthVec extends Vec {
    public FixedWidthVec(int capacityInBytes, int size, VecEncoding encoding, DataType type) {
        super(capacityInBytes, size, encoding, type);
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
