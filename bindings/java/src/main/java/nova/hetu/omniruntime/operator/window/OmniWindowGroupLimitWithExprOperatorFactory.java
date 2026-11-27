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

package nova.hetu.omniruntime.operator.window;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type omni window group limit n with expression operator factory.
 *
 * @since 2025-01-15
 */
public class OmniWindowGroupLimitWithExprOperatorFactory
        extends OmniOperatorFactory<OmniWindowGroupLimitWithExprOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new omni top n sort with expression operator factory.
     *
     * @param sourceTypes the source types
     * @param n means limit n
     * @param funcName row_number/rank/dense_rank
     * @param partitionKeys the partition keys
     * @param sortKeys the sort keys
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     * @param operatorConfig the operator config
     */
    public OmniWindowGroupLimitWithExprOperatorFactory(DataType[] sourceTypes, int n, String funcName,
            String[] partitionKeys, String[] sortKeys, int[] sortAssendings, int[] sortNullFirsts,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, n, funcName, partitionKeys, sortKeys, sortAssendings, sortNullFirsts,
                operatorConfig));
    }

    /**
     * Instantiates a new omni window group limit with expression operator factory
     * with default
     * operator config.
     *
     * @param sourceTypes the source types
     * @param n means limit n
     * @param funcName row_number/rank/dense_rank
     * @param partitionKeys the partition keys
     * @param sortKeys the sort keys
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     */
    public OmniWindowGroupLimitWithExprOperatorFactory(DataType[] sourceTypes, int n, String funcName,
            String[] partitionKeys, String[] sortKeys, int[] sortAssendings, int[] sortNullFirsts) {
        this(sourceTypes, n, funcName, partitionKeys, sortKeys, sortAssendings, sortNullFirsts, new OperatorConfig());
    }

    private static native long createWindowGroupLimitWithExprOperatorFactory(String sourceTypes, int n, String funcName,
            String[] partitionKeys, String[] sortKeys, int[] sortAssendings, int[] sortNullFirsts,
            String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createWindowGroupLimitWithExprOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes),
                context.n, context.funcName, context.partitionKeys, context.sortKeys, context.sortAssendings,
                context.sortNullFirsts, OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Context.
     *
     * @since 2025-1-15
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int n;

        private final String funcName;

        private final String[] partitionKeys;

        private final String[] sortKeys;

        private final int[] sortAssendings;

        private final int[] sortNullFirsts;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param n means limit n
         * @param funcName row_number/rank/dense_rank
         * @param partitionKeys the partition keys
         * @param sortKeys the sort keys
         * @param sortAssendings the sort assendings
         * @param sortNullFirsts the sort null firsts
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int n, String funcName, String[] partitionKeys, String[] sortKeys,
                int[] sortAssendings, int[] sortNullFirsts, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.n = n;
            this.funcName = funcName;
            this.partitionKeys = partitionKeys;
            this.sortKeys = sortKeys;
            this.sortAssendings = sortAssendings;
            this.sortNullFirsts = sortNullFirsts;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), n, funcName, Arrays.hashCode(partitionKeys),
                    Arrays.hashCode(sortKeys), Arrays.hashCode(sortAssendings), Arrays.hashCode(sortNullFirsts),
                    operatorConfig);
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null || getClass() != obj.getClass()) {
                return false;
            }
            FactoryContext context = (FactoryContext) obj;
            return n == context.n && Arrays.equals(sourceTypes, context.sourceTypes) && funcName == context.funcName
                    && Arrays.equals(partitionKeys, context.partitionKeys) && Arrays.equals(sortKeys, context.sortKeys)
                    && Arrays.equals(sortAssendings, context.sortAssendings)
                    && Arrays.equals(sortNullFirsts, context.sortNullFirsts)
                    && operatorConfig.equals(context.operatorConfig);
        }
    }
}
