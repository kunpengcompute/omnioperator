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
 * The type Agg type.
 *
 * @since 2021-06-30
 */
public class FunctionType extends Constant {
    /**
     * The constant OMNI_AGGREGATION_TYPE_SUM.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_SUM = new FunctionType(0);

    /**
     * The constant OMNI_AGGREGATION_TYPE_COUNT_COLUMN.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_COUNT_COLUMN = new FunctionType(1);

    /**
     * The constant OMNI_AGGREGATION_TYPE_COUNT_ALL.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_COUNT_ALL = new FunctionType(2);

    /**
     * The constant OMNI_AGGREGATION_TYPE_AVG.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_AVG = new FunctionType(3);

    /**
     * The constant OMNI_AGGREGATION_TYPE_SAMP.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_SAMP = new FunctionType(4);

    /**
     * The constant OMNI_AGGREGATION_TYPE_MAX.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_MAX = new FunctionType(5);

    /**
     * The constant OMNI_AGGREGATION_TYPE_MIN.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_MIN = new FunctionType(6);

    /**
     * The constant OMNI_AGGREGATION_TYPE_DNV.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_DNV = new FunctionType(7);

    /**
     * The constant OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL = new FunctionType(8);

    /**
     * The constant OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL = new FunctionType(9);

    /**
     * The constant OMNI_AGGREGATION_TYPE_INVALID.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_INVALID = new FunctionType(10);

    /**
     * The constant OMNI_WINDOW_TYPE_ROW_NUMBER.
     */
    public static final FunctionType OMNI_WINDOW_TYPE_ROW_NUMBER = new FunctionType(11);

    /**
     * The constant OMNI_WINDOW_TYPE_RANK.
     */
    public static final FunctionType OMNI_WINDOW_TYPE_RANK = new FunctionType(12);

    /**
     * The constant OMNI_AGGREGATION_TYPE_TRY_SUM.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_TRY_SUM = new FunctionType(13);

    /**
     * The constant OMNI_AGGREGATION_TYPE_TRY_AVG.
     */
    public static final FunctionType OMNI_AGGREGATION_TYPE_TRY_AVG = new FunctionType(14);

    private static final long serialVersionUID = 5337378607473315604L;

    /**
     * Instantiates a new Agg type.
     *
     * @param value the value
     */
    public FunctionType(int value) {
        super(value);
    }
}
