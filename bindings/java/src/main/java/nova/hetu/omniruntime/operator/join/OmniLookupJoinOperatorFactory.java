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

package nova.hetu.omniruntime.operator.join;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;
import java.util.Optional;

/**
 * The Omni lookup join operator factory.
 *
 * @since 2021-06-30
 */
public class OmniLookupJoinOperatorFactory extends OmniOperatorFactory<OmniLookupJoinOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni lookup join operator factory.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     * @param operatorConfig the operator config
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
            Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan, OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, filterExpression, isShuffleExchangeBuildPlan, operatorConfig));
    }


    /**
     * Instantiates a new Omni lookup join operator factory.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param operatorConfig the operator config
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
            Optional<String> filterExpression, OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, filterExpression, false, operatorConfig));
    }

    /**
     * Instantiates a new Omni lookup join operator factory with default operator
     * config.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory, boolean isShuffleExchangeBuildPlan) {
        this(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                Optional.empty(), isShuffleExchangeBuildPlan, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join operator factory with default operator
     * config.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory) {
        this(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                Optional.empty(), false, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join operator factory with default operator
     * config.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
            Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan) {
        this(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                filterExpression, isShuffleExchangeBuildPlan, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join operator factory with default operator
     * config.
     *
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param probeHashCols the probe hash cols
     * @param buildOutputCols the build output cols
     * @param buildOutputTypes the build output types
     * @param hashBuilderOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     */
    public OmniLookupJoinOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
            Optional<String> filterExpression) {
        this(probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                filterExpression, false, new OperatorConfig());
    }

    private static native long createLookupJoinOperatorFactory(String probeTypes, int[] probeOutputCols,
            int[] probeHashCols, int[] buildOutputCols, String buildOutputTypes, long hashBuilderOperatorFactory,
            String filterExpression, boolean isShuffleExchangeBuildPlan, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createLookupJoinOperatorFactory(DataTypeSerializer.serialize(context.probeTypes),
                context.probeOutputCols, context.probeHashCols, context.buildOutputCols,
                DataTypeSerializer.serialize(context.buildOutputTypes),
                context.getHashBuilderOperatorFactory(), context.filterExpression,
                context.isShuffleExchangeBuildPlan, OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 20210630
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] probeTypes;

        private final int[] probeOutputCols;

        private final int[] probeHashCols;

        private final int[] buildOutputCols;

        private final DataType[] buildOutputTypes;

        private final long hashBuilderOperatorFactory;

        private final String filterExpression;

        private final boolean isShuffleExchangeBuildPlan;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param probeTypes the probe types
         * @param probeOutputCols the probe output cols
         * @param probeHashCols the probe hash cols
         * @param buildOutputCols the build output cols
         * @param buildOutputTypes the build output types
         * @param hashBuilderOperatorFactory hashBuilderOperatorFactory
         * @param filterExpression the join filter expression
         * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] probeTypes, int[] probeOutputCols, int[] probeHashCols, int[] buildOutputCols,
                DataType[] buildOutputTypes, OmniHashBuilderOperatorFactory hashBuilderOperatorFactory,
                Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan, OperatorConfig operatorConfig) {
            this.probeTypes = requireNonNull(probeTypes, "probeTypes");
            this.probeOutputCols = requireNonNull(probeOutputCols, "probeOutputCols");
            this.probeHashCols = requireNonNull(probeHashCols, "probeHashCols");
            this.buildOutputCols = requireNonNull(buildOutputCols, "buildOutputCols");
            this.buildOutputTypes = requireNonNull(buildOutputTypes, "buildOutputTypes");
            this.hashBuilderOperatorFactory = hashBuilderOperatorFactory.getNativeOperatorFactory();
            this.filterExpression = filterExpression.isPresent() ? filterExpression.get() : "";
            this.isShuffleExchangeBuildPlan = isShuffleExchangeBuildPlan;
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(probeTypes), Arrays.hashCode(probeOutputCols),
                    Arrays.hashCode(probeHashCols), Arrays.hashCode(buildOutputCols), Arrays.hashCode(buildOutputTypes),
                    hashBuilderOperatorFactory, filterExpression, isShuffleExchangeBuildPlan, operatorConfig);
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
                    && Arrays.equals(probeHashCols, that.probeHashCols)
                    && Arrays.equals(buildOutputCols, that.buildOutputCols)
                    && Arrays.equals(buildOutputTypes, that.buildOutputTypes)
                    && hashBuilderOperatorFactory == that.hashBuilderOperatorFactory
                    && Objects.equals(filterExpression, that.filterExpression)
                    && isShuffleExchangeBuildPlan == that.isShuffleExchangeBuildPlan
                    && Objects.equals(operatorConfig, that.operatorConfig);
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
