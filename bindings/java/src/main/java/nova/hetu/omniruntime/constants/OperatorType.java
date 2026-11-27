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
 * OperatorType
 *
 * @since 2022-12-19
 */
public class OperatorType extends Constant {
    /**
     * The constant OMNI_FILTER_AND_PROJECT.
     */
    public static final OperatorType OMNI_FILTER_AND_PROJECT = new OperatorType(0);

    /**
     * The constant OMNI_PROJECT.
     */
    public static final OperatorType OMNI_PROJECT = new OperatorType(1);

    /**
     * The constant OMNI_LIMIT.
     */
    public static final OperatorType OMNI_LIMIT = new OperatorType(2);

    /**
     * The constant OMNI_DISTINCT_LIMIT.
     */
    public static final OperatorType OMNI_DISTINCT_LIMIT = new OperatorType(3);

    /**
     * The constant OMNI_SORT.
     */
    public static final OperatorType OMNI_SORT = new OperatorType(4);

    /**
     * The constant OMNI_TOPN.
     */
    public static final OperatorType OMNI_TOPN = new OperatorType(5);

    /**
     * The constant OMNI_AGGREGATION.
     */
    public static final OperatorType OMNI_AGGREGATION = new OperatorType(6);

    /**
     * The constant OMNI_HASH_AGGREGATION.
     */
    public static final OperatorType OMNI_HASH_AGGREGATION = new OperatorType(7);

    /**
     * The constant OMNI_WINDOW.
     */
    public static final OperatorType OMNI_WINDOW = new OperatorType(8);

    /**
     * The constant OMNI_HASH_BUILDER.
     */
    public static final OperatorType OMNI_HASH_BUILDER = new OperatorType(9);

    /**
     * The constant OMNI_LOOKUP_JOIN.
     */
    public static final OperatorType OMNI_LOOKUP_JOIN = new OperatorType(10);

    /**
     * The constant OMNI_LOOKUP_OUTER_JOIN.
     */
    public static final OperatorType OMNI_LOOKUP_OUTER_JOIN = new OperatorType(11);

    /**
     * The constant OMNI_SMJ_BUFFER.
     */
    public static final OperatorType OMNI_SMJ_BUFFER = new OperatorType(12);

    /**
     * The constant OMNI_SMJ_STREAM.
     */
    public static final OperatorType OMNI_SMJ_STREAM = new OperatorType(13);

    /**
     * The constant OMNI_PARTITIONED_OUTPUT.
     */
    public static final OperatorType OMNI_PARTITIONED_OUTPUT = new OperatorType(14);

    /**
     * The constant OMNI_UNION.
     */
    public static final OperatorType OMNI_UNION = new OperatorType(15);

    /**
     * The constant OMNI_FUSION.
     */
    public static final OperatorType OMNI_FUSION = new OperatorType(16);

    private static final long serialVersionUID = -4350951859220483149L;

    /**
     * Instantiates a new operator type.
     *
     * @param value the value
     */
    public OperatorType(int value) {
        super(value);
    }
}
