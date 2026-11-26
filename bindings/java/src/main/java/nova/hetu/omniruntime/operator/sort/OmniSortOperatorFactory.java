/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
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
 * The type Omni sort operator factory.
 *
 * @since 2021-06-30
 */
public class OmniSortOperatorFactory extends OmniOperatorFactory<OmniSortOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni sort operator factory.
     *
     * @param sourceTypes the source types
     * @param outputColumns the output columns
     * @param sortColumns the sort columns
     * @param sortAscendings the sort ascendings
     * @param sortNullFirsts the sort null firsts
     * @param operatorConfig the operator config
     */
    public OmniSortOperatorFactory(DataType[] sourceTypes, int[] outputColumns, String[] sortColumns,
            int[] sortAscendings, int[] sortNullFirsts, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, outputColumns, sortColumns, sortAscendings, sortNullFirsts,
                operatorConfig));
    }

    /**
     * Instantiates a new Omni sort operator factory with default operator config.
     *
     * @param sourceTypes the source types
     * @param outputColumns the output columns
     * @param sortColumns the sort columns
     * @param sortAscendings the sort ascendings
     * @param sortNullFirsts the sort null firsts
     */
    public OmniSortOperatorFactory(DataType[] sourceTypes, int[] outputColumns, String[] sortColumns,
            int[] sortAscendings, int[] sortNullFirsts) {
        this(sourceTypes, outputColumns, sortColumns, sortAscendings, sortNullFirsts, new OperatorConfig());
    }

    private static native long createSortOperatorFactory(String sourceTypes, int[] outputCols, String[] sortCols,
            int[] ascendings, int[] nullFirsts, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createSortOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes), context.outputColumns,
                context.sortColumns, context.sortAscendings, context.sortNullFirsts,
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int[] outputColumns;

        private final String[] sortColumns;

        private final int[] sortAscendings;

        private final int[] sortNullFirsts;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param outputColumns the output columns
         * @param sortColumns the sort columns
         * @param sortAscendings the sort ascendings
         * @param sortNullFirsts the sort null firsts
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int[] outputColumns, String[] sortColumns, int[] sortAscendings,
                int[] sortNullFirsts, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.outputColumns = outputColumns;
            this.sortColumns = sortColumns;
            this.sortAscendings = sortAscendings;
            this.sortNullFirsts = sortNullFirsts;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), Arrays.hashCode(outputColumns),
                    Arrays.hashCode(sortColumns), Arrays.hashCode(sortAscendings), Arrays.hashCode(sortNullFirsts),
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
            FactoryContext that = (FactoryContext) obj;
            return Arrays.equals(sourceTypes, that.sourceTypes) && Arrays.equals(outputColumns, that.outputColumns)
                    && Arrays.equals(sortColumns, that.sortColumns)
                    && Arrays.equals(sortAscendings, that.sortAscendings)
                    && Arrays.equals(sortNullFirsts, that.sortNullFirsts)
                    && Objects.equals(operatorConfig, that.operatorConfig);
        }
    }
}
