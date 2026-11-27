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
 * The type frame bound type for window operator.
 *
 * @since 2022-04-15
 */
public class OmniWindowFrameBoundType extends Constant {
    /**
     * The constant OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING.
     */
    public static final OmniWindowFrameBoundType OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING = new OmniWindowFrameBoundType(0);

    /**
     * The constant OMNI_FRAME_BOUND_PRECEDING.
     */
    public static final OmniWindowFrameBoundType OMNI_FRAME_BOUND_PRECEDING = new OmniWindowFrameBoundType(1);

    /**
     * The constant OMNI_FRAME_BOUND_CURRENT_ROW.
     */
    public static final OmniWindowFrameBoundType OMNI_FRAME_BOUND_CURRENT_ROW = new OmniWindowFrameBoundType(2);

    /**
     * The constant OMNI_FRAME_BOUND_FOLLOWING.
     */
    public static final OmniWindowFrameBoundType OMNI_FRAME_BOUND_FOLLOWING = new OmniWindowFrameBoundType(3);

    /**
     * The constant OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING.
     */
    public static final OmniWindowFrameBoundType OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING = new OmniWindowFrameBoundType(4);

    private static final long serialVersionUID = 3646147886114670835L;

    /**
     * Instantiates a new frame bound type.
     *
     * @param value the value
     */
    public OmniWindowFrameBoundType(int value) {
        super(value);
    }
}
