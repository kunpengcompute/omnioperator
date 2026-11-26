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

package nova.hetu.omniruntime.operator.aggregator;

import static java.util.Objects.requireNonNull;
import static nova.hetu.omniruntime.constants.ConstantHelper.toNativeConstants;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni aggregation operator factory.
 *
 * @since 2021-06-30
 */
public class OmniAggregationOperatorFactory extends OmniOperatorFactory<OmniAggregationOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni aggregation operator factory.
     *
     * @param sourceTypes the aggregation source types
     * @param aggFunctionTypes the aggregation function types
     * @param aggInputChannels the aggregation function input channels
     * @param maskChannels mask channel array for aggregations
     * @param aggOutputTypes the aggregation output types
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     * @param operatorConfig the operator config
     */
    public OmniAggregationOperatorFactory(DataType[] sourceTypes, FunctionType[] aggFunctionTypes,
            int[] aggInputChannels, int[] maskChannels, DataType[] aggOutputTypes, boolean isInputRaw,
            boolean isOutputPartial, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, aggFunctionTypes, aggInputChannels, maskChannels, aggOutputTypes,
                isInputRaw, isOutputPartial, operatorConfig));
    }

    /**
     * Instantiates a new Omni aggregation operator factory with default operator
     * config.
     *
     * @param sourceTypes the aggregation source types
     * @param aggFunctionTypes the aggregation function types
     * @param aggInputChannels the aggregation function input channels
     * @param maskChannels mask channel array for aggregations
     * @param aggOutputTypes the aggregation output types
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     */
    public OmniAggregationOperatorFactory(DataType[] sourceTypes, FunctionType[] aggFunctionTypes,
            int[] aggInputChannels, int[] maskChannels, DataType[] aggOutputTypes, boolean isInputRaw,
            boolean isOutputPartial) {
        this(sourceTypes, aggFunctionTypes, aggInputChannels, maskChannels, aggOutputTypes, isInputRaw, isOutputPartial,
                new OperatorConfig());
    }

    private static native long createAggregationOperatorFactory(String sourceTypes, int[] aggFunctionTypes,
            int[] aggInputChannels, int[] maskChannels, String aggOutputTypes, boolean isInputRaw,
            boolean isOutputPartial);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createAggregationOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes),
                toNativeConstants(context.aggFunctionTypes), context.aggInputChannels, context.maskChannels,
                DataTypeSerializer.serialize(context.aggOutputTypes), context.isInputRaw, context.isOutputPartial);
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final FunctionType[] aggFunctionTypes;

        private final int[] aggInputChannels;

        private final int[] maskChannels;

        private final DataType[] aggOutputTypes;

        private final boolean isInputRaw;

        private final boolean isOutputPartial;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param aggFunctionTypes the aggregation function types
         * @param aggInputChannels the aggregation input channels
         * @param maskChannels the aggregation mask channels
         * @param aggOutputTypes the aggregation output types
         * @param isInputRaw the input raw
         * @param isOutputPartial the output partial
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, FunctionType[] aggFunctionTypes, int[] aggInputChannels,
                int[] maskChannels, DataType[] aggOutputTypes, boolean isInputRaw, boolean isOutputPartial,
                OperatorConfig operatorConfig) {
            this.sourceTypes = requireNonNull(sourceTypes, "sourceTypes is null");
            this.aggFunctionTypes = requireNonNull(aggFunctionTypes, "aggFunctionTypes is null");
            this.aggInputChannels = requireNonNull(aggInputChannels, "aggInputChannels is null");
            this.maskChannels = requireNonNull(maskChannels, "maskChannels is null");
            this.aggOutputTypes = requireNonNull(aggOutputTypes, "aggOutputTypes is null");
            this.isInputRaw = isInputRaw;
            this.isOutputPartial = isOutputPartial;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), Arrays.hashCode(aggFunctionTypes),
                    Arrays.hashCode(aggInputChannels), Arrays.hashCode(maskChannels), Arrays.hashCode(aggOutputTypes),
                    isInputRaw, isOutputPartial, operatorConfig);
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
            return Arrays.equals(sourceTypes, that.sourceTypes)
                    && Arrays.equals(aggFunctionTypes, that.aggFunctionTypes)
                    && Arrays.equals(aggInputChannels, that.aggInputChannels)
                    && Arrays.equals(maskChannels, that.maskChannels)
                    && Arrays.equals(aggOutputTypes, that.aggOutputTypes) && isInputRaw == that.isInputRaw
                    && isOutputPartial == that.isOutputPartial && operatorConfig.equals(that.operatorConfig);
        }
    }
}
