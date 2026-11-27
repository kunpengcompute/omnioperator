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

package nova.hetu.omniruntime.utils;

/**
 * The enum Omni error type.
 *
 * @since 2021-06-30
 */
public enum OmniErrorType {
    /**
     * Omni undefined omni error type.
     */
    OMNI_UNDEFINED(1),
    /**
     * Omni nosupport omni error type.
     */
    OMNI_NOSUPPORT(2),
    /**
     * Omni native error omni error type.
     */
    OMNI_NATIVE_ERROR(3),

    /**
     * Omni runtime param error.
     */
    OMNI_PARAM_ERROR(4),

    /**
     * Omni inner error.
     */
    OMNI_INNER_ERROR(5),

    /**
     * Omni vec or vectbatch double free
     */
    OMNI_DOUBLE_FREE(6),

    /**
     * Omni java udf error
     */
    OMNI_JAVA_UDF_ERROR(7);

    private final int value;

    OmniErrorType(int value) {
        this.value = value;
    }

    /**
     * Gets value.
     *
     * @return the value
     */
    public int getValue() {
        return value;
    }
}
