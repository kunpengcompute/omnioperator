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

package nova.hetu.omniruntime.operator.join;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.constants.JoinType;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;
import java.util.Optional;

/**
 * The Omni nested loop lookup join operator factory.
 *
 * @since 2024-12-10
 */
public class OmniNestedLoopJoinLookupOperatorFactory
        extends OmniOperatorFactory<OmniNestedLoopJoinLookupOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni lookup join operator factory.
     *
     * @param joinType the join types
     * @param probeTypes the probe types
     * @param probeOutputCols the probe output cols
     * @param buildOpFactory the NestedLoopJoinBuildOperatorFactory
     * @param filter the json string for connecting conditional expressions
     * @param operatorConfig the operator config
     */
    public OmniNestedLoopJoinLookupOperatorFactory(JoinType joinType, DataType[] probeTypes, int[] probeOutputCols,
            Optional<String> filter, OmniNestedLoopJoinBuildOperatorFactory buildOpFactory,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(joinType, probeTypes, probeOutputCols, filter, buildOpFactory, operatorConfig));
    }

    private static native long createNestedLoopJoinLookupOperatorFactory(int joinType, String probeTypes,
            int[] probeOutputCols, String filter, long buildOpFactory, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(OmniNestedLoopJoinLookupOperatorFactory.FactoryContext context) {
        return createNestedLoopJoinLookupOperatorFactory(context.joinType.getValue(),
                DataTypeSerializer.serialize(context.probeTypes), context.probeOutputCols, context.filter,
                context.getNestedLoopJoinBuildOperatorFactory(), OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 20241210
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final JoinType joinType;

        private final DataType[] probeTypes;

        private final int[] probeOutputCols;

        private final long buildOpFactory;

        private final String filter;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param joinType the join types
         * @param probeTypes the probe types
         * @param probeOutputCols the probe output cols
         * @param buildOpFactory the NestedLoopJoinBuildOperatorFactory
         * @param filter the json string for connecting conditional expressions
         * @param operatorConfig the operator config
         */
        public FactoryContext(JoinType joinType, DataType[] probeTypes, int[] probeOutputCols, Optional<String> filter,
                OmniNestedLoopJoinBuildOperatorFactory buildOpFactory, OperatorConfig operatorConfig) {
            this.joinType = requireNonNull(joinType, "joinType");
            this.probeTypes = requireNonNull(probeTypes, "probeTypes");
            this.probeOutputCols = requireNonNull(probeOutputCols, "probeOutputCols");
            this.filter = filter.orElse("");
            this.buildOpFactory = buildOpFactory.getNativeOperatorFactory();
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(joinType, Arrays.hashCode(probeTypes), Arrays.hashCode(probeOutputCols), filter,
                    buildOpFactory, operatorConfig);
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
            return joinType.equals(that.joinType) && Arrays.equals(probeTypes, that.probeTypes)
                    && Arrays.equals(probeOutputCols, that.probeOutputCols) && Objects.equals(filter, that.filter)
                    && buildOpFactory == that.buildOpFactory && Objects.equals(operatorConfig, that.operatorConfig);
        }

        /**
         * Gets nested builder operator factory.
         *
         * @return the nested loop join builder operator factory
         */
        public long getNestedLoopJoinBuildOperatorFactory() {
            return buildOpFactory;
        }
    }
}
