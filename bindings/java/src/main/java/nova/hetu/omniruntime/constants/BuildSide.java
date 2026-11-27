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
 * The Build Side.
 *
 * @since 2025-01-14
 */
public class BuildSide extends Constant {
    /**
     * The constant BUILD_UNKNOWN. The representative doesn't need to know
     */
    public static final BuildSide BUILD_UNKNOWN = new BuildSide(0);

    /**
     * The constant BUILD_LEFT.
     */
    public static final BuildSide BUILD_LEFT = new BuildSide(1);

    /**
     * The constant BUILD_RIGHT.
     */
    public static final BuildSide BUILD_RIGHT = new BuildSide(2);

    private static final long serialVersionUID = -4047841645954651422L;

    /**
     * Instantiates a new Build Side.
     *
     * @param value the value
     */
    public BuildSide(int value) {
        super(value);
    }
}
