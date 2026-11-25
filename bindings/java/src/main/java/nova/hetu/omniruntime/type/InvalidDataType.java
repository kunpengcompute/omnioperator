/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
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
 * invalid data type. The data type of unsupported/invalid data.
 *
 * @since 2022-04-01
 */
public class InvalidDataType extends DataType {
    /**
     * Invalid singleton.
     */
    public static final InvalidDataType INVALID = new InvalidDataType();

    /**
     * The construct.
     */
    public InvalidDataType() {
        super(DataTypeId.OMNI_INVALID);
    }
}
