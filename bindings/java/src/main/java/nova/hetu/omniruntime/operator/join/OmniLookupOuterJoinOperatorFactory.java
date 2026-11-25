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
 * The Omni lookup outer join operator factory.
 *
 * @since 2022-08-30
 */
public class OmniLookupOuterJoinOperatorFactory
        extends OmniOperatorFactory<OmniLookupOuterJoinOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni lookup outer join operator factory.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param operatorConfig the operator config
     */
    public OmniLookupOuterJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] buildOutputCols,
            DataType[] buildOutputTypes, OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, operatorConfig));
    }

    /**
     * Instantiates a new Omni lookup outer join operator factory with default operator config.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     */
    public OmniLookupOuterJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] buildOutputCols,
            DataType[] buildOutputTypes, OmniHashBuilderOperatorFactory hashBuilderOperatorFactory) {
        this(probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
    }

    private static native long createLookupOuterJoinOperatorFactory(String probeTypes, int[] probeOutputCols,
            int[] buildOutputCols, String buildOutputTypes, long hashBuilderOperatorFactory);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createLookupOuterJoinOperatorFactory(DataTypeSerializer.serialize(context.probeTypes),
                context.probeOutputCols, context.buildOutputCols,
                DataTypeSerializer.serialize(context.buildOutputTypes), context.getHashBuilderOperatorFactory());
    }

    /**
     * The type Factory context.
     *
     * @since 20220830
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] probeTypes;

        private final int[] probeOutputCols;

        private final int[] buildOutputCols;

        private final DataType[] buildOutputTypes;

        private final long hashBuilderOperatorFactory;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param probeTypes the probe types
         * @param probeOutputCols the probe output cols
         * @param buildOutputCols the build output cols
         * @param buildOutputTypes the build output types
         * @param hashBuilderOperatorFactory hashBuilderOperatorFactory
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] probeTypes, int[] probeOutputCols, int[] buildOutputCols,
                DataType[] buildOutputTypes, OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
                OperatorConfig operatorConfig) {
            this.probeTypes = requireNonNull(probeTypes, "probeTypes");
            this.probeOutputCols = requireNonNull(probeOutputCols, "probeOutputCols");
            this.buildOutputCols = requireNonNull(buildOutputCols, "buildOutputCols");
            this.buildOutputTypes = requireNonNull(buildOutputTypes, "buildOutputTypes");
            this.hashBuilderOperatorFactory = hashBuilderOperatorFactory.getNativeOperatorFactory();
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(probeTypes), Arrays.hashCode(probeOutputCols),
                    Arrays.hashCode(buildOutputCols), Arrays.hashCode(buildOutputTypes), hashBuilderOperatorFactory,
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
            return Arrays.equals(probeTypes, that.probeTypes) && Arrays.equals(probeOutputCols, that.probeOutputCols)
                    && Arrays.equals(buildOutputCols, that.buildOutputCols)
                    && Arrays.equals(buildOutputTypes, that.buildOutputTypes)
                    && hashBuilderOperatorFactory == that.hashBuilderOperatorFactory
                    && operatorConfig.equals(that.operatorConfig);
        }

        /**
         * Gets hash builder operator factory.
         *
         * @return the hash builder operator factory
         */
        public long getHashBuilderOperatorFactory() {
            return hashBuilderOperatorFactory;
        }
    }
}
