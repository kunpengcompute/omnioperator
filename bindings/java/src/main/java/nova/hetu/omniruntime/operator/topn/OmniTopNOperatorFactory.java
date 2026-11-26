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

package nova.hetu.omniruntime.operator.topn;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni top n operator factory.
 *
 * @since 2021-06-30
 */
public class OmniTopNOperatorFactory extends OmniOperatorFactory<OmniTopNOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni top n operator factory.
     *
     * @param sourceTypes the source types
     * @param limitN the limit n
     * @param sortCols the sort cols
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     * @param operatorConfig the operator config
     */
    public OmniTopNOperatorFactory(DataType[] sourceTypes, int limitN, String[] sortCols, int[] sortAssendings,
            int[] sortNullFirsts, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, limitN, 0, sortCols, sortAssendings, sortNullFirsts, operatorConfig));
    }

    /**
     * Instantiates a new Omni top n operator factory.
     *
     * @param sourceTypes the source types
     * @param limitN the limit n
     * @param offsetN the offset n
     * @param sortCols the sort cols
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     * @param operatorConfig the operator config
     */
    public OmniTopNOperatorFactory(DataType[] sourceTypes, int limitN, int offsetN, String[] sortCols,
            int[] sortAssendings, int[] sortNullFirsts, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, limitN, offsetN, sortCols, sortAssendings, sortNullFirsts,
                operatorConfig));
    }

    /**
     * Instantiates a new Omni top n operator factory with default operator config.
     *
     * @param sourceTypes the source types
     * @param limitN the limit n
     * @param sortCols the sort cols
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     */
    public OmniTopNOperatorFactory(DataType[] sourceTypes, int limitN, String[] sortCols, int[] sortAssendings,
            int[] sortNullFirsts) {
        this(sourceTypes, limitN, 0, sortCols, sortAssendings, sortNullFirsts, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni top n operator factory with default operator config.
     *
     * @param sourceTypes the source types
     * @param limitN the limit n
     * @param offsetN the offset n
     * @param sortCols the sort cols
     * @param sortAssendings the sort assendings
     * @param sortNullFirsts the sort null firsts
     */
    public OmniTopNOperatorFactory(DataType[] sourceTypes, int limitN, int offsetN, String[] sortCols,
            int[] sortAssendings, int[] sortNullFirsts) {
        this(sourceTypes, limitN, offsetN, sortCols, sortAssendings, sortNullFirsts, new OperatorConfig());
    }

    private static native long createTopNOperatorFactory(String sourceTypes, int limitN, int offsetN, String[] sortCols,
            int[] sortAssendings, int[] sortNullFirsts);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createTopNOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes), context.limitN,
                context.offsetN, context.sortCols, context.sortAssendings, context.sortNullFirsts);
    }

    /**
     * The type Context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int limitN;

        private final int offsetN;

        private final String[] sortCols;

        private final int[] sortAssendings;

        private final int[] sortNullFirsts;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param limitN the limit n
         * @param offsetN the offset n
         * @param sortCols the sort cols
         * @param sortAssendings the sort assendings
         * @param sortNullFirsts the sort null firsts
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int limitN, int offsetN, String[] sortCols, int[] sortAssendings,
                int[] sortNullFirsts, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.limitN = limitN;
            this.offsetN = offsetN;
            this.sortCols = sortCols;
            this.sortAssendings = sortAssendings;
            this.sortNullFirsts = sortNullFirsts;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), limitN, offsetN, Arrays.hashCode(sortCols),
                    Arrays.hashCode(sortAssendings), Arrays.hashCode(sortNullFirsts), operatorConfig);
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
            return limitN == context.limitN && offsetN == context.offsetN
                    && Arrays.equals(sourceTypes, context.sourceTypes) && Arrays.equals(sortCols, context.sortCols)
                    && Arrays.equals(sortAssendings, context.sortAssendings)
                    && Arrays.equals(sortNullFirsts, context.sortNullFirsts)
                    && operatorConfig.equals(context.operatorConfig);
        }
    }
}
