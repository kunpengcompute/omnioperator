/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
 */

package nova.hetu.omniruntime.constants;

/**
 * Function / aggregation / window type constants.
 * <p>
 * The declaration order defines the integer values (0, 1, 2, ...). This order MUST match
 * the C++ enum in core/src/operator/util/function_type.h exactly. When adding a new type,
 * append one line at the end in both this file and the C++ enum to avoid merge conflicts.
 *
 * @since 2021-06-30
 */
public class FunctionType extends Constant {
    private static final long serialVersionUID = 5337378607473315604L;

    private static int nextId = 0;

    private static FunctionType next() {
        return new FunctionType(nextId++);
    }

    // Order must match C++ function_type.h
    public static final FunctionType OMNI_AGGREGATION_TYPE_SUM = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COUNT_COLUMN = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COUNT_ALL = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_AVG = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_SAMP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_STD_POP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_VAR_SAMP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_VAR_POP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_MAX = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_MIN = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_DNV = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_LAST_IGNORENULL = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_INVALID = next();
    public static final FunctionType OMNI_WINDOW_TYPE_ROW_NUMBER = next();
    public static final FunctionType OMNI_WINDOW_TYPE_RANK = next();
    public static final FunctionType OMNI_WINDOW_TYPE_PERCENT_RANK = next();
    public static final FunctionType OMNI_WINDOW_TYPE_CUME_DIST = next();
    public static final FunctionType OMNI_WINDOW_TYPE_LEAD = next();
    public static final FunctionType OMNI_WINDOW_TYPE_LAG = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_TRY_SUM = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_TRY_AVG = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_BLOOM_FILTER = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_MIN_BY = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_MAX_BY = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_BIT_AND = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_BIT_OR = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_BIT_XOR = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_CORR = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COVAR_POP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COVAR_SAMP = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COLLECT_SET = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COLLECT_LIST = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_KURTOSIS = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_SKEWNESS = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_COUNT = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_INTERCEPT = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_R2 = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_SLOPE = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_SXX = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_SXY = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_SYY = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_REPLACEMENT = next();
    public static final FunctionType OMNI_WINDOW_TYPE_NTH_VALUE = next();
    public static final FunctionType OMNI_WINDOW_TYPE_NTILE = next();
    public static final FunctionType OMNI_WINDOW_TYPE_DENSE_RANK = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_AVGX = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_REGR_AVGY = next();
    public static final FunctionType OMNI_AGGREGATION_TYPE_COUNT_DISTINCT = next();

    public FunctionType(int value) {
        super(value);
    }
}
