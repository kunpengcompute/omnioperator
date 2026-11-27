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

package omniruntime.udf;

import static java.lang.Math.addExact;

import org.apache.hadoop.hive.ql.exec.UDF;

/**
 * AddIntUDF for test
 *
 * @since 2022-8-3
 */
public class AddIntUDF extends UDF {
    /**
     * Calculates the sum of two int values for test.
     *
     * @param paramA the first param
     * @param paramB the second param
     * @return return the sum of paramA and paramA
     */
    public Integer evaluate(Integer paramA, Integer paramB) {
        if (paramA == null || paramB == null) {
            return null;
        }
        return addExact(paramA, paramB);
    }
}
