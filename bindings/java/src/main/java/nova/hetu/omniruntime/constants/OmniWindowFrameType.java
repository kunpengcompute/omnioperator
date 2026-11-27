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

package nova.hetu.omniruntime.constants;

/**
 * The type window frame type.
 *
 * @since 2022-04-15
 */
public class OmniWindowFrameType extends Constant {
    /**
     * The constant OMNI_FRAME_TYPE_RANGE.
     */
    public static final OmniWindowFrameType OMNI_FRAME_TYPE_RANGE = new OmniWindowFrameType(0);

    /**
     * The constant OMNI_FRAME_TYPE_ROWS.
     */
    public static final OmniWindowFrameType OMNI_FRAME_TYPE_ROWS = new OmniWindowFrameType(1);

    private static final long serialVersionUID = -3453499979001168717L;

    /**
     * Instantiates a new window frame type.
     *
     * @param value the value
     */
    public OmniWindowFrameType(int value) {
        super(value);
    }
}
