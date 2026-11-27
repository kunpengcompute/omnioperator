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

import java.util.Arrays;

/**
 * The type Constant helper.
 *
 * @since 2021-06-30
 */
public class ConstantHelper {
    private ConstantHelper() {
    }

    /**
     * To native constants int [ ].
     *
     * @param constants the constants
     * @return the int [ ]
     */
    public static int[] toNativeConstants(Constant[] constants) {
        return Arrays.stream(constants).map(Constant::getValue).mapToInt(Integer::intValue).toArray();
    }
}
