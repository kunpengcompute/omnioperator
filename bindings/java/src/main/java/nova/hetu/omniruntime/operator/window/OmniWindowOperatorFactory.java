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

package nova.hetu.omniruntime.operator.window;

import static nova.hetu.omniruntime.constants.ConstantHelper.toNativeConstants;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.constants.OmniWindowFrameBoundType;
import nova.hetu.omniruntime.constants.OmniWindowFrameType;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni window operator factory.
 *
 * @since 20210630
 */
public class OmniWindowOperatorFactory extends OmniOperatorFactory<OmniWindowOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni window operator factory.
     *
     * @param sourceTypes the source types
     * @param outputChannels the output channels
     * @param windowFunction the window function
     * @param partitionChannels the partition channels
     * @param preGroupedChannels the pre grouped channels
     * @param sortChannels the sort channels
     * @param sortOrder the sort order
     * @param sortNullFirsts the sort null firsts
     * @param preSortedChannelPrefix the pre sorted channel prefix
     * @param expectedPositions the expected positions
     * @param argumentChannels the argument channels
     * @param windowFunctionReturnType the window function return type
     * @param windowFrameTypes frame types of the window
     * @param windowFrameStartTypes start types of frame in window
     * @param winddowFrameStartChannels channels value of frame start value
     * @param windowFrameEndTypes end types of frame in window
     * @param winddowFrameEndChannels channel values of frame end value
     * @param operatorConfig the operator config
     */
    public OmniWindowOperatorFactory(DataType[] sourceTypes, int[] outputChannels, FunctionType[] windowFunction,
            int[] partitionChannels, int[] preGroupedChannels, int[] sortChannels, int[] sortOrder,
            int[] sortNullFirsts, int preSortedChannelPrefix, int expectedPositions, int[] argumentChannels,
            DataType[] windowFunctionReturnType, OmniWindowFrameType[] windowFrameTypes,
            OmniWindowFrameBoundType[] windowFrameStartTypes, int[] winddowFrameStartChannels,
            OmniWindowFrameBoundType[] windowFrameEndTypes, int[] winddowFrameEndChannels,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, outputChannels, windowFunction, partitionChannels, preGroupedChannels,
                sortChannels, sortOrder, sortNullFirsts, preSortedChannelPrefix, expectedPositions, argumentChannels,
                windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes, winddowFrameStartChannels,
                windowFrameEndTypes, winddowFrameEndChannels, operatorConfig));
    }

    /**
     * Instantiates a new Omni window operator factory with default operator config.
     *
     * @param sourceTypes the source types
     * @param outputChannels the output channels
     * @param windowFunction the window function
     * @param partitionChannels the partition channels
     * @param preGroupedChannels the pre grouped channels
     * @param sortChannels the sort channels
     * @param sortOrder the sort order
     * @param sortNullFirsts the sort null firsts
     * @param preSortedChannelPrefix the pre sorted channel prefix
     * @param expectedPositions the expected positions
     * @param argumentChannels the argument channels
     * @param windowFunctionReturnType the window function return type
     * @param windowFrameTypes frame types of the window
     * @param windowFrameStartTypes start types of frame in window
     * @param winddowFrameStartChannels channels value of frame start value
     * @param windowFrameEndTypes end types of frame in window
     * @param winddowFrameEndChannels channel values of frame end value
     */
    public OmniWindowOperatorFactory(DataType[] sourceTypes, int[] outputChannels, FunctionType[] windowFunction,
            int[] partitionChannels, int[] preGroupedChannels, int[] sortChannels, int[] sortOrder,
            int[] sortNullFirsts, int preSortedChannelPrefix, int expectedPositions, int[] argumentChannels,
            DataType[] windowFunctionReturnType, OmniWindowFrameType[] windowFrameTypes,
            OmniWindowFrameBoundType[] windowFrameStartTypes, int[] winddowFrameStartChannels,
            OmniWindowFrameBoundType[] windowFrameEndTypes, int[] winddowFrameEndChannels) {
        this(sourceTypes, outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels,
                sortOrder, sortNullFirsts, preSortedChannelPrefix, expectedPositions, argumentChannels,
                windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes, winddowFrameStartChannels,
                windowFrameEndTypes, winddowFrameEndChannels, new OperatorConfig());
    }

    private static native long createWindowOperatorFactory(String sourceTypes, int[] outputChannels, int[] windFunction,
            int[] partitionChannels, int[] preGroupedChannels, int[] sortChannels, int[] sortOrder,
            int[] sortNullFirsts, int preSortedChannelPrefix, int expectedPositions, int[] argumentChannels,
            String windowFunctionReturnType, int[] windowFrameTypes, int[] windowFrameStartTypes,
            int[] windowFrameStartChannels, int[] windowFrameEndTypes, int[] windowFrameEndChannels,
            String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createWindowOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes), context.outputChannels,
                toNativeConstants(context.windFunction), context.partitionChannels, context.preGroupedChannels,
                context.sortChannels, context.sortOrder, context.sortNullFirsts, context.preSortedChannelPrefix,
                context.expectedPositions, context.argumentChannels,
                DataTypeSerializer.serialize(context.windowFunctionReturnType), toNativeConstants(context.frameTypes),
                toNativeConstants(context.frameStartTypes), context.frameStartChannels,
                toNativeConstants(context.frameEndTypes), context.frameEndChannels,
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 20210630
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int[] outputChannels;

        private final FunctionType[] windFunction;

        private final int[] partitionChannels;

        private final int[] sortChannels;

        private final int[] sortOrder;

        private final int[] preGroupedChannels;

        private final int[] sortNullFirsts;

        private final int preSortedChannelPrefix;

        private final int expectedPositions;

        private final int[] argumentChannels;

        private final DataType[] windowFunctionReturnType;

        private final OmniWindowFrameType[] frameTypes;

        private final OmniWindowFrameBoundType[] frameStartTypes;

        private final int[] frameStartChannels;

        private final OmniWindowFrameBoundType[] frameEndTypes;

        private final int[] frameEndChannels;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the source types
         * @param outputChannels the output channels
         * @param windowFunction the window function
         * @param partitionChannels the partition channels
         * @param preGroupedChannels the pre grouped channels
         * @param sortChannels the sort channels
         * @param sortOrder the sort order
         * @param sortNullFirsts the sort null firsts
         * @param preSortedChannelPrefix the pre sorted channel prefix
         * @param expectedPositions the expected positions
         * @param argumentChannels the argument channels
         * @param windowFunctionReturnType the window function return type
         * @param windowFrameTypes frame types of the window
         * @param windowFrameStartTypes start types of frame in window
         * @param winddowFrameStartChannels channels value of frame start value
         * @param windowFrameEndTypes end types of frame in window
         * @param winddowFrameEndChannels channel values of frame end value
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int[] outputChannels, FunctionType[] windowFunction,
                int[] partitionChannels, int[] preGroupedChannels, int[] sortChannels, int[] sortOrder,
                int[] sortNullFirsts, int preSortedChannelPrefix, int expectedPositions, int[] argumentChannels,
                DataType[] windowFunctionReturnType, OmniWindowFrameType[] windowFrameTypes,
                OmniWindowFrameBoundType[] windowFrameStartTypes, int[] winddowFrameStartChannels,
                OmniWindowFrameBoundType[] windowFrameEndTypes, int[] winddowFrameEndChannels,
                OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.outputChannels = outputChannels;
            this.windFunction = windowFunction;
            this.partitionChannels = partitionChannels;
            this.preGroupedChannels = preGroupedChannels;
            this.sortChannels = sortChannels;
            this.sortOrder = sortOrder;
            this.sortNullFirsts = sortNullFirsts;
            this.preSortedChannelPrefix = preSortedChannelPrefix;
            this.expectedPositions = expectedPositions;
            this.argumentChannels = argumentChannels;
            this.windowFunctionReturnType = windowFunctionReturnType;
            this.frameTypes = windowFrameTypes;
            this.frameStartTypes = windowFrameStartTypes;
            this.frameStartChannels = winddowFrameStartChannels;
            this.frameEndTypes = windowFrameEndTypes;
            this.frameEndChannels = winddowFrameEndChannels;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), Arrays.hashCode(outputChannels),
                    Arrays.hashCode(windFunction), Arrays.hashCode(partitionChannels),
                    Arrays.hashCode(preGroupedChannels), Arrays.hashCode(sortChannels), Arrays.hashCode(sortOrder),
                    Arrays.hashCode(sortNullFirsts), preSortedChannelPrefix, expectedPositions,
                    Arrays.hashCode(argumentChannels), Arrays.hashCode(windowFunctionReturnType),
                    Arrays.hashCode(frameTypes), Arrays.hashCode(frameStartTypes), Arrays.hashCode(frameStartChannels),
                    Arrays.hashCode(frameEndTypes), Arrays.hashCode(frameEndChannels), operatorConfig);
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
            return preSortedChannelPrefix == context.preSortedChannelPrefix
                    && expectedPositions == context.expectedPositions && Arrays.equals(sourceTypes, context.sourceTypes)
                    && Arrays.equals(outputChannels, context.outputChannels)
                    && Arrays.equals(windFunction, context.windFunction)
                    && Arrays.equals(partitionChannels, context.partitionChannels)
                    && Arrays.equals(preGroupedChannels, context.preGroupedChannels)
                    && Arrays.equals(sortChannels, context.sortChannels) && Arrays.equals(sortOrder, context.sortOrder)
                    && Arrays.equals(sortNullFirsts, context.sortNullFirsts)
                    && Arrays.equals(argumentChannels, context.argumentChannels)
                    && Arrays.equals(windowFunctionReturnType, context.windowFunctionReturnType)
                    && Arrays.equals(frameTypes, context.frameTypes)
                    && Arrays.equals(frameStartTypes, context.frameStartTypes)
                    && Arrays.equals(frameStartChannels, context.frameStartChannels)
                    && Arrays.equals(frameEndTypes, context.frameEndTypes)
                    && Arrays.equals(frameEndChannels, context.frameEndChannels)
                    && operatorConfig.equals(context.operatorConfig);
        }
    }
}
