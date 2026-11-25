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

/**
 * long data type.
 *
 * @since 2021-08-05
 */
public class LongDataType extends DataType {
    /**
     * Long singleton.
     */
    public static final LongDataType LONG = new LongDataType();

    private static final long serialVersionUID = -1589352305079680921L;

    /**
     * The construct.
     */
    public LongDataType() {
        super(DataTypeId.OMNI_LONG);
    }
}
