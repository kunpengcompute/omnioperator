/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
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

package nova.hetu.omniruntime.constants;

/**
 * The Join type.
 *
 * @since 2021-06-30
 */
public class JoinType extends Constant {
    /**
     * The constant OMNI_JOIN_TYPE_INNER.
     */
    public static final JoinType OMNI_JOIN_TYPE_INNER = new JoinType(0);

    /**
     * The constant OMNI_JOIN_TYPE_LEFT.
     */
    public static final JoinType OMNI_JOIN_TYPE_LEFT = new JoinType(1);

    /**
     * The constant OMNI_JOIN_TYPE_RIGHT.
     */
    public static final JoinType OMNI_JOIN_TYPE_RIGHT = new JoinType(2);

    /**
     * The constant OMNI_JOIN_TYPE_FULL.
     */
    public static final JoinType OMNI_JOIN_TYPE_FULL = new JoinType(3);

    /**
     * The constant OMNI_JOIN_TYPE_LEFT_SEMI.
     */
    public static final JoinType OMNI_JOIN_TYPE_LEFT_SEMI = new JoinType(4);

    /**
     * The constant OMNI_JOIN_TYPE_LEFT_ANTI.
     */
    public static final JoinType OMNI_JOIN_TYPE_LEFT_ANTI = new JoinType(5);

    /**
     * The constant OMNI_JOIN_TYPE_EXISTENCE.
     */
    public static final JoinType OMNI_JOIN_TYPE_EXISTENCE = new JoinType(6);

    private static final long serialVersionUID = -4086671645951741450L;

    /**
     * Instantiates a new Join type.
     *
     * @param value the value
     */
    public JoinType(int value) {
        super(value);
    }
}
