/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
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
 * The Omni lookup join with expression operator factory.
 *
 * @since 2021-10-16
 */
public class OmniLookupJoinWithExprOperatorFactory
        extends OmniOperatorFactory<OmniLookupJoinWithExprOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni lookup join with expression operator factory.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     * @param operatorConfig the operator config
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
            Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan, OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, filterExpression, isShuffleExchangeBuildPlan, operatorConfig));
    }

    /**
     * Instantiates a new Omni lookup join with expression operator factory.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param operatorConfig the operator config
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
            Optional<String> filterExpression, OperatorConfig operatorConfig) {
        super(new FactoryContext(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, filterExpression, false, operatorConfig));
    }

    /**
     * Instantiates a new Omni lookup join with expression operator factory with
     * default operator config.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
            Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan) {
        this(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, filterExpression, isShuffleExchangeBuildPlan, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join with expression operator factory with
     * default operator config.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param filterExpression the join filter expression
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
            Optional<String> filterExpression) {
        this(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, filterExpression, false, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join with expression operator factory with
     * default operator config.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
            boolean isShuffleExchangeBuildPlan) {
        this(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, Optional.empty(), isShuffleExchangeBuildPlan, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni lookup join with expression operator factory with
     * default operator config.
     *
     * @param probeTypes the probe input types
     * @param probeOutputCols the probe output columns
     * @param probeHashKeys the probe hash keys
     * @param buildOutputCols the build output columns
     * @param buildOutputTypes the build output column types
     * @param hashBuilderWithExprOperatorFactory the hash builder operator factory
     */
    public OmniLookupJoinWithExprOperatorFactory(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
            int[] buildOutputCols, DataType[] buildOutputTypes,
            OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory) {
        this(probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderWithExprOperatorFactory, Optional.empty(), false, new OperatorConfig());
    }

    private static native long createLookupJoinWithExprOperatorFactory(String probeTypes, int[] probeOutputCols,
            String[] probeHashKeys, int[] buildOutputCols, String buildOutputTypes,
            long hashBuilderWithExprOperatorFactory, String filterExpression,
             boolean isShuffleExchangeBuildPlan, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createLookupJoinWithExprOperatorFactory(DataTypeSerializer.serialize(context.probeTypes),
                context.probeOutputCols, context.probeHashKeys, context.buildOutputCols,
                DataTypeSerializer.serialize(context.buildOutputTypes),
                context.getHashBuilderWithExprOperatorFactory(), context.filterExpression,
                context.isShuffleExchangeBuildPlan, OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The Factory context.
     *
     * @since 2021-10-16
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] probeTypes;

        private final int[] probeOutputCols;

        private final String[] probeHashKeys;

        private final int[] buildOutputCols;

        private final DataType[] buildOutputTypes;

        private final long hashBuilderWithExprOperatorFactory;

        private final String filterExpression;

        private final boolean isShuffleExchangeBuildPlan;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param probeTypes the probe types
         * @param probeOutputCols the probe output cols
         * @param probeHashKeys the probe hash keys
         * @param buildOutputCols the build output cols
         * @param buildOutputTypes the build output types
         * @param hashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory
         * @param filterExpression the join filter expression
         * @param isShuffleExchangeBuildPlan build plan is shuffleExchange
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] probeTypes, int[] probeOutputCols, String[] probeHashKeys,
                int[] buildOutputCols, DataType[] buildOutputTypes,
                OmniHashBuilderWithExprOperatorFactory hashBuilderWithExprOperatorFactory,
                Optional<String> filterExpression, boolean isShuffleExchangeBuildPlan, OperatorConfig operatorConfig) {
            this.probeTypes = requireNonNull(probeTypes, "probeTypes");
            this.probeOutputCols = requireNonNull(probeOutputCols, "probeOutputCols");
            this.probeHashKeys = requireNonNull(probeHashKeys, "probeHashKeys");
            this.buildOutputCols = requireNonNull(buildOutputCols, "buildOutputCols");
            this.buildOutputTypes = requireNonNull(buildOutputTypes, "buildOutputTypes");
            this.hashBuilderWithExprOperatorFactory = hashBuilderWithExprOperatorFactory.getNativeOperatorFactory();
            this.filterExpression = filterExpression.isPresent() ? filterExpression.get() : "";
            this.isShuffleExchangeBuildPlan = isShuffleExchangeBuildPlan;
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(probeTypes), Arrays.hashCode(probeOutputCols),
                    Arrays.hashCode(probeHashKeys), Arrays.hashCode(buildOutputCols), Arrays.hashCode(buildOutputTypes),
                    hashBuilderWithExprOperatorFactory, filterExpression, isShuffleExchangeBuildPlan, operatorConfig);
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
                    && (hashBuilderWithExprOperatorFactory == that.hashBuilderWithExprOperatorFactory)
                    && filterExpression.equals(that.filterExpression)
                    && isShuffleExchangeBuildPlan == that.isShuffleExchangeBuildPlan
                    && operatorConfig.equals(that.operatorConfig);
        }

        public long getHashBuilderWithExprOperatorFactory() {
            return hashBuilderWithExprOperatorFactory;
        }
    }
}
