/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
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
 * The type Omni hash aggregation operator factory.
 *
 * @since 2021-06-30
 */
public class OmniHashAggregationOperatorFactory
        extends OmniOperatorFactory<OmniHashAggregationOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni hash aggregation operator factory.
     *
     * @param groupByChanel the grouping channels
     * @param groupByTypes the group by types
     * @param aggChannels the agg channels
     * @param aggTypes the agg types
     * @param aggFunctionTypes the agg function types
     * @param aggOutputTypes the agg output types
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     * @param operatorConfig the operator config
     */
    public OmniHashAggregationOperatorFactory(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
            DataType[] aggTypes, FunctionType[] aggFunctionTypes, DataType[] aggOutputTypes, boolean isInputRaw,
            boolean isOutputPartial, OperatorConfig operatorConfig) {
        super(new FactoryContext(groupByChanel, groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes,
                isInputRaw, isOutputPartial, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash aggregation operator factory.
     *
     * @param groupByChanel the grouping channels
     * @param groupByTypes the group by types
     * @param aggChannels the agg channels
     * @param aggTypes the agg types
     * @param aggFunctionTypes the agg function types
     * @param aggOutputTypes the agg output types
     * @param maskChannels mask channel list for aggregators
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     * @param operatorConfig the operator config
     */
    public OmniHashAggregationOperatorFactory(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
            DataType[] aggTypes, FunctionType[] aggFunctionTypes, int[] maskChannels, DataType[] aggOutputTypes,
            boolean isInputRaw, boolean isOutputPartial, OperatorConfig operatorConfig) {
        super(new FactoryContext(groupByChanel, groupByTypes, aggChannels, aggTypes, aggFunctionTypes, maskChannels,
                aggOutputTypes, isInputRaw, isOutputPartial, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash aggregation operator factory with jit
     * default.
     *
     * @param groupByChanel the grouping channels
     * @param groupByTypes the group by types
     * @param aggChannels the agg channels
     * @param aggTypes the agg types
     * @param aggFunctionTypes the agg function types
     * @param aggOutputTypes the agg output types
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     */
    public OmniHashAggregationOperatorFactory(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
            DataType[] aggTypes, FunctionType[] aggFunctionTypes, DataType[] aggOutputTypes, boolean isInputRaw,
            boolean isOutputPartial) {
        this(groupByChanel, groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, isInputRaw,
                isOutputPartial, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni hash aggregation operator factory with jit
     * default.
     *
     * @param groupByChanel the grouping channels
     * @param groupByTypes the group by types
     * @param aggChannels the agg channels
     * @param aggTypes the agg types
     * @param aggFunctionTypes the agg function types
     * @param maskChannels mask channel list for aggregators
     * @param aggOutputTypes the agg output types
     * @param isInputRaw the input raw
     * @param isOutputPartial the output partial
     */
    public OmniHashAggregationOperatorFactory(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
            DataType[] aggTypes, FunctionType[] aggFunctionTypes, int[] maskChannels, DataType[] aggOutputTypes,
            boolean isInputRaw, boolean isOutputPartial) {
        this(groupByChanel, groupByTypes, aggChannels, aggTypes, aggFunctionTypes, maskChannels, aggOutputTypes,
                isInputRaw, isOutputPartial, new OperatorConfig());
    }

    private static native long createHashAggregationOperatorFactory(String[] groupByChanel, String groupByTypes,
            String[] aggChannels, String aggTypes, int[] aggFunctionTypes, int[] maskChannels, String aggOutputTypes,
            boolean isInputRaw, boolean isOutputPartial, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createHashAggregationOperatorFactory(context.groupByChanel,
                DataTypeSerializer.serialize(context.groupByTypes), context.aggChannels,
                DataTypeSerializer.serialize(context.aggTypes), toNativeConstants(context.aggFunctionTypes),
                context.maskChannels, DataTypeSerializer.serialize(context.aggOutputTypes), context.isInputRaw,
                context.isOutputPartial, OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private static final int INVALID_MASK_CHANNEL = -1;

        private final String[] groupByChanel;

        private final DataType[] groupByTypes;

        private final String[] aggChannels;

        private final DataType[] aggTypes;

        private final FunctionType[] aggFunctionTypes;

        private final int[] maskChannels;

        private final DataType[] aggOutputTypes;

        private final boolean isInputRaw;

        private final boolean isOutputPartial;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param groupByChanel the grouping channels
         * @param groupByTypes the group by types
         * @param aggChannels the agg channels
         * @param aggTypes the agg types
         * @param aggFunctionTypes the agg function types
         * @param maskChannels mask channel list for aggregators
         * @param aggOutputTypes the agg output types
         * @param isInputRaw the input raw
         * @param isOutputPartial the output partial
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
                DataType[] aggTypes, FunctionType[] aggFunctionTypes, int[] maskChannels, DataType[] aggOutputTypes,
                boolean isInputRaw, boolean isOutputPartial, OperatorConfig operatorConfig) {
            this.groupByChanel = requireNonNull(groupByChanel, "requireNonNull");
            this.groupByTypes = requireNonNull(groupByTypes, "groupByTypes");
            this.aggChannels = requireNonNull(aggChannels, "aggChannels");
            this.aggTypes = requireNonNull(aggTypes, "aggTypes");
            this.aggFunctionTypes = requireNonNull(aggFunctionTypes, "aggFunctionTypes");
            this.maskChannels = requireNonNull(maskChannels, "maskChannels is null");
            this.aggOutputTypes = requireNonNull(aggOutputTypes, "aggOutputTypes");
            this.isInputRaw = isInputRaw;
            this.isOutputPartial = isOutputPartial;
            this.operatorConfig = operatorConfig;
        }

        /**
         * Instantiates a new Context.
         *
         * @param groupByChanel the grouping channels
         * @param groupByTypes the group by types
         * @param aggChannels the agg channels
         * @param aggTypes the agg types
         * @param aggFunctionTypes the agg function types
         * @param aggOutputTypes the agg output types
         * @param isInputRaw the input raw
         * @param isOutputPartial the output partial
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] groupByChanel, DataType[] groupByTypes, String[] aggChannels,
                DataType[] aggTypes, FunctionType[] aggFunctionTypes, DataType[] aggOutputTypes, boolean isInputRaw,
                boolean isOutputPartial, OperatorConfig operatorConfig) {
            this(groupByChanel, groupByTypes, aggChannels, aggTypes, aggFunctionTypes,
                    getDefaultMaskChannel(aggFunctionTypes), aggOutputTypes, isInputRaw, isOutputPartial,
                    operatorConfig);
        }

        private static int[] getDefaultMaskChannel(FunctionType[] aggFunctionTypes) {
            int[] maskChannelArray = new int[aggFunctionTypes.length]; // one mask channel for each function
            Arrays.fill(maskChannelArray, INVALID_MASK_CHANNEL);
            return maskChannelArray;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(groupByChanel), Arrays.hashCode(groupByTypes),
                    Arrays.hashCode(aggChannels), Arrays.hashCode(aggTypes), Arrays.hashCode(aggFunctionTypes),
                    Arrays.hashCode(maskChannels), Arrays.hashCode(aggOutputTypes), isInputRaw, isOutputPartial,
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
            return Arrays.equals(groupByChanel, that.groupByChanel) && Arrays.equals(groupByTypes, that.groupByTypes)
                    && Arrays.equals(aggChannels, that.aggChannels) && Arrays.equals(aggTypes, that.aggTypes)
                    && Arrays.equals(aggFunctionTypes, that.aggFunctionTypes)
                    && Arrays.equals(maskChannels, that.maskChannels)
                    && Arrays.equals(aggOutputTypes, that.aggOutputTypes) && isInputRaw == that.isInputRaw
                    && isOutputPartial == that.isOutputPartial && operatorConfig.equals(that.operatorConfig);
        }
    }
}
