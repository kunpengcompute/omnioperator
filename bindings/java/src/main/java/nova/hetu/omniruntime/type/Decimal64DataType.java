/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
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

package nova.hetu.omniruntime.type;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Decimal64 data type.
 *
 * @since 2021-08-05
 */
public class Decimal64DataType extends DecimalDataType {
    /**
     * Default precision value of decimal.
     */
    public static final int DEFAULT_PRECISION = 18;

    /**
     * Default scale value of decimal.
     */
    public static final int DEFAULT_SCALE = 0;

    /**
     * Decimal64 singleton.
     */
    public static final Decimal64DataType DECIMAL64 = new Decimal64DataType(DEFAULT_PRECISION, DEFAULT_SCALE);

    private static final long serialVersionUID = -1858555622202917305L;

    /**
     * Construct of decimal64 data type.
     *
     * @param precision the precision of decimal
     * @param scale the scale of decimal
     */
    public Decimal64DataType(@JsonProperty("precision") int precision, @JsonProperty("scale") int scale) {
        super(precision, scale, DataTypeId.OMNI_DECIMAL64);
    }
}
