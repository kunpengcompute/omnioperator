/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
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
import nova.hetu.omniruntime.utils.JsonUtils;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni aggregation with expression operator factory.
 *
 * @since 2021-10-21
 */
public class OmniHashAggregationWithExprOperatorFactory
        extends OmniOperatorFactory<OmniHashAggregationWithExprOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni hash aggregation with expression operator factory.
     *
     * @param groupByChanel the grouping channels
     * @param aggChannels the agg channels
     * @param aggChannelsFilter the agg filter Expr
     * @param sourceTypes the source types
     * @param aggFunctionTypes the agg function types
     * @param aggOutputTypes the agg output types
     * @param isInputRaws the input raw flags
     * @param isOutputPartials the output partial flags
     * @param operatorConfig the operator config
     */
    public OmniHashAggregationWithExprOperatorFactory(String[] groupByChanel, String[][] aggChannels,
            String[] aggChannelsFilter, DataType[] sourceTypes, FunctionType[] aggFunctionTypes,
            DataType[][] aggOutputTypes, boolean[] isInputRaws, boolean[] isOutputPartials,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(groupByChanel, aggChannels, aggChannelsFilter, sourceTypes, aggFunctionTypes,
                aggOutputTypes, isInputRaws, isOutputPartials, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash aggregation with expression operator factory.
     *
     * @param groupByChanel the grouping channels
     * @param aggChannels the agg channels
     * @param aggChannelsFilter the agg filter Expr
     * @param sourceTypes the source types
     * @param aggFunctionTypes the agg function types
     * @param maskChannels mask channel list for aggregators
     * @param aggOutputTypes the agg output types
     * @param isInputRaws the input raw flags
     * @param isOutputPartials the output partial flags
     * @param operatorConfig the operator config
     */
    public OmniHashAggregationWithExprOperatorFactory(String[] groupByChanel, String[][] aggChannels,
            String[] aggChannelsFilter, DataType[] sourceTypes, FunctionType[] aggFunctionTypes, int[] maskChannels,
            DataType[][] aggOutputTypes, boolean[] isInputRaws, boolean[] isOutputPartials,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(groupByChanel, aggChannels, aggChannelsFilter, sourceTypes, aggFunctionTypes,
                maskChannels, aggOutputTypes, isInputRaws, isOutputPartials, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash aggregation with expression operator factory
     * with jit default.
     *
     * @param groupByChanel the grouping channels
     * @param aggChannels the agg channels
     * @param aggChannelsFilter the agg filter Expr
     * @param sourceTypes the source types
     * @param aggFunctionTypes the agg function types
     * @param aggOutputTypes the agg output types
     * @param isInputRaws the input raw flags
     * @param isOutputPartials the output partial flags
     */
    public OmniHashAggregationWithExprOperatorFactory(String[] groupByChanel, String[][] aggChannels,
            String[] aggChannelsFilter, DataType[] sourceTypes, FunctionType[] aggFunctionTypes,
            DataType[][] aggOutputTypes, boolean[] isInputRaws, boolean[] isOutputPartials) {
        this(groupByChanel, aggChannels, aggChannelsFilter, sourceTypes, aggFunctionTypes, aggOutputTypes, isInputRaws,
                isOutputPartials, new OperatorConfig());
    }

    private static native long createHashAggregationWithExprOperatorFactory(String[] groupByChanel,
            String[] aggChannels, String[] aggChannelsFilter, String sourceTypes, int[] aggFunctionTypes,
            int[] maskChannels, String[] aggOutputTypes, boolean[] isInputRaws, boolean[] isOutputPartials,
            String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createHashAggregationWithExprOperatorFactory(context.groupByChanel,
                JsonUtils.jsonStringArray(context.aggChannels), context.aggChannelsFilter,
                DataTypeSerializer.serialize(context.sourceTypes), toNativeConstants(context.aggFunctionTypes),
                context.maskChannels, DataTypeSerializer.serialize(context.aggOutputTypes), context.isInputRaws,
                context.isOutputPartials, OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 20210630
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private static final int INVALID_MASK_CHANNEL = -1;

        private final String[] groupByChanel;

        private final String[][] aggChannels;

        private final String[] aggChannelsFilter;

        private final DataType[] sourceTypes;

        private final FunctionType[] aggFunctionTypes;

        private final int[] maskChannels;

        private final DataType[][] aggOutputTypes;

        private final boolean[] isInputRaws;

        private final boolean[] isOutputPartials;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param groupByChanel the grouping channels
         * @param aggChannels the agg channels
         * @param aggChannelsFilter the agg filter Expr
         * @param sourceTypes the source types
         * @param aggFunctionTypes the agg function types
         * @param maskChannels mask channel list for aggregators
         * @param aggOutputTypes the agg output types
         * @param isInputRaws the input raw flags
         * @param isOutputPartials the output partial flags
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] groupByChanel, String[][] aggChannels, String[] aggChannelsFilter,
                DataType[] sourceTypes, FunctionType[] aggFunctionTypes, int[] maskChannels,
                DataType[][] aggOutputTypes, boolean[] isInputRaws, boolean[] isOutputPartials,
                OperatorConfig operatorConfig) {
            this.groupByChanel = requireNonNull(groupByChanel, "requireNonNull");
            this.aggChannels = requireNonNull(aggChannels, "aggChannels");
            this.aggChannelsFilter = checkAggChannelsFilter(aggChannelsFilter);
            this.sourceTypes = requireNonNull(sourceTypes, "sourceTypes");
            this.aggFunctionTypes = requireNonNull(aggFunctionTypes, "aggFunctionTypes");
            this.maskChannels = requireNonNull(maskChannels, "maskChannels is null");
            this.aggOutputTypes = requireNonNull(aggOutputTypes, "aggOutputTypes");
            this.isInputRaws = requireNonNull(isInputRaws, "isInputRaws");
            this.isOutputPartials = requireNonNull(isOutputPartials, "isInputRaws");
            this.operatorConfig = operatorConfig;
        }

        /**
         * Instantiates a new Context.
         *
         * @param groupByChanel the grouping channels
         * @param aggChannels the agg channels
         * @param aggChannelsFilter the agg filter Expr
         * @param sourceTypes the source types
         * @param aggFunctionTypes the agg function types
         * @param aggOutputTypes the agg output types
         * @param isInputRaws the input raw flags
         * @param isOutputPartials the output partial flags
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] groupByChanel, String[][] aggChannels, String[] aggChannelsFilter,
                DataType[] sourceTypes, FunctionType[] aggFunctionTypes, DataType[][] aggOutputTypes,
                boolean[] isInputRaws, boolean[] isOutputPartials, OperatorConfig operatorConfig) {
            this(groupByChanel, aggChannels, aggChannelsFilter, sourceTypes, aggFunctionTypes,
                    getDefaultMaskChannel(aggFunctionTypes), aggOutputTypes, isInputRaws, isOutputPartials,
                    operatorConfig);
        }

        private static String[] checkAggChannelsFilter(String[] aggChannelsFilter) {
            for (int i = 0; i < aggChannelsFilter.length; i++) {
                aggChannelsFilter[i] = aggChannelsFilter[i] == null ? "" : aggChannelsFilter[i];
            }
            return aggChannelsFilter;
        }

        private static int[] getDefaultMaskChannel(FunctionType[] aggFunctionTypes) {
            int[] maskChannelArray = new int[aggFunctionTypes.length]; // one mask channel for each function
            Arrays.fill(maskChannelArray, INVALID_MASK_CHANNEL);
            return maskChannelArray;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(groupByChanel), Arrays.deepHashCode(aggChannels),
                    Arrays.hashCode(aggChannelsFilter), Arrays.hashCode(sourceTypes), Arrays.hashCode(aggFunctionTypes),
                    Arrays.hashCode(maskChannels), Arrays.deepHashCode(aggOutputTypes), Arrays.hashCode(isInputRaws),
                    Arrays.hashCode(isOutputPartials), operatorConfig);
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
            return Arrays.equals(groupByChanel, that.groupByChanel) && Arrays.deepEquals(aggChannels, that.aggChannels)
                    && Arrays.equals(aggChannelsFilter, that.aggChannelsFilter)
                    && Arrays.equals(sourceTypes, that.sourceTypes)
                    && Arrays.equals(aggFunctionTypes, that.aggFunctionTypes)
                    && Arrays.equals(maskChannels, that.maskChannels)
                    && Arrays.deepEquals(aggOutputTypes, that.aggOutputTypes)
                    && Arrays.equals(isInputRaws, that.isInputRaws)
                    && Arrays.equals(isOutputPartials, that.isOutputPartials)
                    && operatorConfig.equals(that.operatorConfig);
        }
    }
}
