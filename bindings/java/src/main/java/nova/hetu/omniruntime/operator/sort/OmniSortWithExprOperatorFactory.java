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

package nova.hetu.omniruntime.operator.sort;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The Omni sort with expression operator factory.
 *
 * @since 2021-10-16
 */
public class OmniSortWithExprOperatorFactory
        extends OmniOperatorFactory<OmniSortWithExprOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni sort with expression operator factory.
     *
     * @param sourceTypes the source types
     * @param outputColumns the output columns
     * @param sortKeys the sort keys
     * @param sortAscendings the sort ascendings
     * @param sortNullFirsts the sort null firsts
     */
    public OmniSortWithExprOperatorFactory(DataType[] sourceTypes, int[] outputColumns, String[] sortKeys,
            int[] sortAscendings, int[] sortNullFirsts) {
        super(new FactoryContext(sourceTypes, outputColumns, sortKeys, sortAscendings, sortNullFirsts,
                new OperatorConfig()));
    }

    /**
     * Instantiates a new Omni sort with expression operator factory with default
     * operator config.
     *
     * @param sourceTypes the source types
     * @param outputColumns the output columns
     * @param sortKeys the sort keys
     * @param sortAscendings the sort ascendings
     * @param sortNullFirsts the sort null firsts
     * @param operatorConfig the operator config
     */
    public OmniSortWithExprOperatorFactory(DataType[] sourceTypes, int[] outputColumns, String[] sortKeys,
            int[] sortAscendings, int[] sortNullFirsts, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, outputColumns, sortKeys, sortAscendings, sortNullFirsts, operatorConfig));
    }

    private static native long createSortWithExprOperatorFactory(String sourceTypes, int[] outputCols,
            String[] sortKeys, int[] ascendings, int[] nullFirsts, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createSortWithExprOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes),
                context.outputColumns, context.sortKeys, context.sortAscendings, context.sortNullFirsts,
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The Factory context.
     *
     * @since 2021-10-16
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int[] outputColumns;

        private final String[] sortKeys;

        private final int[] sortAscendings;

        private final int[] sortNullFirsts;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param outputColumns the output columns
         * @param sortKeys the sort keys
         * @param sortAscendings the sort ascendings
         * @param sortNullFirsts the sort null firsts
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int[] outputColumns, String[] sortKeys, int[] sortAscendings,
                int[] sortNullFirsts, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.outputColumns = outputColumns;
            this.sortKeys = sortKeys;
            this.sortAscendings = sortAscendings;
            this.sortNullFirsts = sortNullFirsts;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), Arrays.hashCode(outputColumns), Arrays.hashCode(sortKeys),
                    Arrays.hashCode(sortAscendings), Arrays.hashCode(sortNullFirsts), operatorConfig);
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null || getClass() != obj.getClass()) {
                return false;
            }
            FactoryContext that = (FactoryContext) obj;
            return Arrays.equals(sourceTypes, that.sourceTypes) && Arrays.equals(outputColumns, that.outputColumns)
                    && Arrays.equals(sortKeys, that.sortKeys) && Arrays.equals(sortAscendings, that.sortAscendings)
                    && Arrays.equals(sortNullFirsts, that.sortNullFirsts)
                    && Objects.equals(operatorConfig, that.operatorConfig);
        }
    }
}
