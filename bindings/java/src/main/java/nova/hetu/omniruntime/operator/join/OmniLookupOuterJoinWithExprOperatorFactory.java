/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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
 * The Omni lookup outer join with expression operator factory.
 *
 * @since 2022-9-1
 */
public class OmniLookupOuterJoinWithExprOperatorFactory
        extends OmniOperatorFactory<OmniLookupOuterJoinWithExprOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni lookup outer join with expression operator factory.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param operatorConfig the operator config
     */
    public OmniLookupOuterJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols,
            String[] probeHashKeys, int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory, OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                operatorConfig, hashBuilderWithExprOperatorFactory));
    }

    /**
     * Instantiates a new Omni lookup outer join with expression operator factory with
     * default operator config.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     */
    public OmniLookupOuterJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols,
            String[] probeHashKeys, int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory) {
        this(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, new OperatorConfig());
    }

    private static native long createLookupOuterJoinWithExprOperatorFactory(String probeTypes, int[] probeOutputCols,
            String[] probeHashKeys, int[] buildOutputCols, String buildOutputTypes,
            long hashBuilderWithExprOperatorFactory);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createLookupOuterJoinWithExprOperatorFactory(DataTypeSerializer.serialize(context.probeTypes),
                context.probeOutputCols, context.probeHashKeys, context.buildOutputCols,
                DataTypeSerializer.serialize(context.buildOutputTypes),
                context.getHashBuilderWithExprOperatorFactory());
    }

    /**
     * The factory Context.
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] probeTypes;

        private final int[] probeOutputCols;

        private final String[] probeHashKeys;

        private final int[] buildOutputCols;

        private final DataType[] buildOutputTypes;

        private final OperatorConfig operatorConfig;

        private final long hashBuilderWithExprOperatorFactory;

        /**
         * Instantiates a new Context.
         *
         * @param probeTypes the probe types
         * @param probeOutputCols the probe output cols
         * @param probeHashKeys the probe hash keys
         * @param buildOutputCols the build output cols
         * @param buildOutputTypes the build output types
         * @param operatorConfig the operator config
         * @param hashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory
         */
        public FactoryContext(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
                int[] buildOutputCols, DataType[] buildOutputTypes, OperatorConfig operatorConfig,
                OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory) {
            this.probeTypes = requireNonNull(probeTypes, "probeTypes");
            this.probeOutputCols = requireNonNull(probeOutputCols, "probeOutputCols");
            this.probeHashKeys = requireNonNull(probeHashKeys, "probeHashKeys");
            this.buildOutputCols = requireNonNull(buildOutputCols, "buildOutputCols");
            this.buildOutputTypes = requireNonNull(buildOutputTypes, "buildOutputTypes");
            this.operatorConfig = operatorConfig;
            this.hashBuilderWithExprOperatorFactory = hashBuilderWithExprOperatorFactory.getNativeOperatorFactory();
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(probeTypes), Arrays.hashCode(probeOutputCols),
                    Arrays.hashCode(probeHashKeys), Arrays.hashCode(buildOutputCols), Arrays.hashCode(buildOutputTypes),
                    operatorConfig, hashBuilderWithExprOperatorFactory);
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
            return Arrays.equals(probeTypes, that.probeTypes) && Arrays.equals(probeOutputCols, that.probeOutputCols)
                    && Arrays.equals(probeHashKeys, that.probeHashKeys)
                    && Arrays.equals(buildOutputCols, that.buildOutputCols)
                    && Arrays.equals(buildOutputTypes, that.buildOutputTypes)
                    && operatorConfig.equals(that.operatorConfig)
                    && hashBuilderWithExprOperatorFactory == that.hashBuilderWithExprOperatorFactory;
        }

        public long getHashBuilderWithExprOperatorFactory() {
            return hashBuilderWithExprOperatorFactory;
        }
    }
}