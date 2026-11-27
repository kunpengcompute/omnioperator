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

package nova.hetu.omniruntime.type;

/**
 * double data type.
 *
 * @since 2021-08-05
 */
public class DoubleDataType extends DataType {
    /**
     * Double singleton.
     */
    public static final DoubleDataType DOUBLE = new DoubleDataType();

    private static final long serialVersionUID = -5517157056853810138L;

    /**
     * The construct.
     */
    public DoubleDataType() {
        super(DataTypeId.OMNI_DOUBLE);
    }
}
