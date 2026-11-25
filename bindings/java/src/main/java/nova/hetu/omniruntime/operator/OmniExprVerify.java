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

package nova.hetu.omniruntime.operator;

import nova.hetu.omniruntime.OmniLibs;

/**
 * To verify if expr is supported before codegen
 *
 * @since 2022-05-16
 */
public class OmniExprVerify {
    static {
        OmniLibs.load();
    }

    private static native long exprVerify(String inputTypes, int inputLength, String expression, Object[] projections,
            int projectLength, int parseFormat);

    /**
     * exprVerifyNative
     *
     * @param inputTypes the input types
     * @param inputLength the length of input types
     * @param filterExpr filter expression
     * @param projections a set of projection expressions
     * @param projectLength the length of projection expressions
     * @param parseFormat json or string
     * @return if expr is supported
     */
    public long exprVerifyNative(String inputTypes, int inputLength, String filterExpr, Object[] projections,
            int projectLength, int parseFormat) {
        return exprVerify(inputTypes, inputLength, filterExpr, projections, projectLength, parseFormat);
    }
}
