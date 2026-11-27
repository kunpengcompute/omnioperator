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

package nova.hetu.omniruntime.operator.join;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni sort merge buffered table with expression operator factory.
 *
 * @since 2023-07-30
 */
public class OmniSmjBufferedTableWithExprOperatorFactoryV3
        extends OmniOperatorFactory<OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext> {
    /**
     * Instantiates a new Omni sort merge buffered table factory.
     *
     * @param soruceTypes the all input vector types
     * @param equalKeyExprs equal condition key expressions
     * @param outputChannels output of streamed table
     * @param smjStreamedTableOperatorFactoryV3 streamed table operator factory
     *            instance
     * @param operatorConfig the operator config
     */
    public OmniSmjBufferedTableWithExprOperatorFactoryV3(DataType[] soruceTypes, String[] equalKeyExprs,
            int[] outputChannels, OmniSmjStreamedTableWithExprOperatorFactoryV3 smjStreamedTableOperatorFactoryV3,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(soruceTypes, equalKeyExprs, outputChannels, operatorConfig,
                smjStreamedTableOperatorFactoryV3));
    }

    /**
     * Instantiates a new Omni sort merge buffered table factory with default
     * operator config.
     *
     * @param soruceTypes the all input vector types
     * @param equalKeyExprs equal condition key expressions
     * @param outputChannels output of streamed table
     * @param smjStreamedTableOperatorFactoryV3 streamed table operator factory
     *            instance
     */
    public OmniSmjBufferedTableWithExprOperatorFactoryV3(DataType[] soruceTypes, String[] equalKeyExprs,
            int[] outputChannels, OmniSmjStreamedTableWithExprOperatorFactoryV3 smjStreamedTableOperatorFactoryV3) {
        this(soruceTypes, equalKeyExprs, outputChannels, smjStreamedTableOperatorFactoryV3, new OperatorConfig());
    }

    private static native long createSmjBufferedTableWithExprOperatorFactoryV3(String soruceTypes,
            String[] equalKeyExprs, int[] outputChannels, long smjStreamedTableOperatorFactoryV3,
            String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createSmjBufferedTableWithExprOperatorFactoryV3(DataTypeSerializer.serialize(context.soruceTypes),
                context.equalKeyExprs, context.outputChannels, context.getStreamedTableOperatorFactoryV3(),
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 2023-07-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] soruceTypes;

        private final String[] equalKeyExprs;

        private final int[] outputChannels;

        private final OperatorConfig operatorConfig;

        private final long streamedTableOperatorFactoryV3;

        /**
         * Instantiates a new Context.
         *
         * @param soruceTypes the all input vector types
         * @param equalKeyExps equal condition key expressions
         * @param outputChannels output of streamed table
         * @param operatorConfig the operator config
         * @param streamedTableOperatorFactoryV3 streamedTableOperatorFactory
         */
        public FactoryContext(DataType[] soruceTypes, String[] equalKeyExps, int[] outputChannels,
                OperatorConfig operatorConfig,
                OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedTableOperatorFactoryV3) {
            this.soruceTypes = requireNonNull(soruceTypes, "soruceTypes");
            this.equalKeyExprs = requireNonNull(equalKeyExps, "equalKeyExprs");
            this.outputChannels = requireNonNull(outputChannels, "outputChannels");
            this.operatorConfig = requireNonNull(operatorConfig, "operatorConfig");
            this.streamedTableOperatorFactoryV3 = streamedTableOperatorFactoryV3.getNativeOperatorFactory();
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(soruceTypes), Arrays.hashCode(equalKeyExprs),
                    Arrays.hashCode(outputChannels), operatorConfig, streamedTableOperatorFactoryV3);
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
            return Arrays.equals(soruceTypes, that.soruceTypes) && Arrays.equals(equalKeyExprs, that.equalKeyExprs)
                    && Arrays.equals(outputChannels, that.outputChannels) && operatorConfig.equals(that.operatorConfig)
                    && streamedTableOperatorFactoryV3 == that.streamedTableOperatorFactoryV3;
        }

        public long getStreamedTableOperatorFactoryV3() {
            return streamedTableOperatorFactoryV3;
        }
    }
}
