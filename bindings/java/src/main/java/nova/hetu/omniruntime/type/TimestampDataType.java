/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
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
 * timestamp data type.
 *
 * @since 2024-09-09
 */
public class TimestampDataType extends DataType {
    /**
     * timestamp singleton.
     */
    public static final TimestampDataType TIMESTAMP = new TimestampDataType();

    private static final long serialVersionUID = -165184964293631557L;

    /**
     * The construct.
     */
    public TimestampDataType() {
        super(DataTypeId.OMNI_TIMESTAMP);
    }
}
