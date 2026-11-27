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

import nova.hetu.omniruntime.constants.JoinType;
import nova.hetu.omniruntime.constants.BuildSide;
import nova.hetu.omniruntime.operator.OmniOperator;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * The Omni hash builder with expression operator factory.
 *
 * @since 2021-10-16
 */
public class OmniHashBuilderWithExprOperatorFactory
        extends OmniOperatorFactory<OmniHashBuilderWithExprOperatorFactory.FactoryContext> {
    /**
     * The global lock is used for sharing hash builder operator and factory
     * concurrently.
     */
    public static final Lock gLock = new ReentrantLock();

    /**
     * The hashmap cached the shared hash builder operator, the key is the build
     * plan node id.
     */
    private static Map<Integer, OmniOperator> operatorCache = new HashMap<>();

    /**
     * The hashmap cached the shared hash builder operator factory, the key is the
     * build plan node id.
     */
    private static Map<Integer, OmniHashBuilderWithExprOperatorFactory> factoryCache = new HashMap<>();

    /**
     * The hashmap stores the num of lookup which is in-use shared hash builder
     * operator
     * the key is the build plan node id.
     */
    private static Map<Integer, AtomicInteger> ref = new HashMap<>();

    /**
     * Instantiates a new Omni hash builder with expression operator factory.
     *
     * @param joinType the join type
     * @param buildTypes the build input types
     * @param buildHashKeys the build hash keys
     * @param operatorCount the operator count
     * @param operatorConfig the operator config
     */
    public OmniHashBuilderWithExprOperatorFactory(JoinType joinType, DataType[] buildTypes, String[] buildHashKeys,
            int operatorCount, OperatorConfig operatorConfig) {
        super(new FactoryContext(joinType, buildTypes, buildHashKeys, operatorCount, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash builder with expression operator factory.
     *
     * @param joinType the join type
     * @param buildSide the build side
     * @param buildTypes the build input types
     * @param buildHashKeys the build hash keys
     * @param operatorCount the operator count
     * @param operatorConfig the operator config
     */
    public OmniHashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide, DataType[] buildTypes,
            String[] buildHashKeys, int operatorCount, OperatorConfig operatorConfig) {
        super(new FactoryContext(joinType, buildSide, buildTypes, buildHashKeys, operatorCount, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash builder with expression operator factory with
     * default operator config.
     *
     * @param joinType the join type
     * @param buildTypes the build input types
     * @param buildHashKeys the build hash keys
     * @param operatorCount the operator count
     */
    public OmniHashBuilderWithExprOperatorFactory(JoinType joinType, DataType[] buildTypes, String[] buildHashKeys,
            int operatorCount) {
        this(joinType, buildTypes, buildHashKeys, operatorCount, new OperatorConfig());
    }

    private static native long createHashBuilderWithExprOperatorFactory(int joinType, int buildSide, String buildTypes,
            String[] buildHashKeys, int operatorCount, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createHashBuilderWithExprOperatorFactory(context.joinType.getValue(), context.buildSide.getValue(),
                DataTypeSerializer.serialize(context.buildTypes), context.buildHashKeys, context.operatorCount,
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * save a new shared hash builder operator and factory into cache
     *
     * @param builderNodeId the build plan node id
     * @param factory the shared hash builder operator factory
     * @param operator the shared hash builder operator
     */
    public static void saveHashBuilderOperatorAndFactory(Integer builderNodeId,
            OmniHashBuilderWithExprOperatorFactory factory, OmniOperator operator) {
        operatorCache.put(builderNodeId, operator);
        factoryCache.put(builderNodeId, factory);
    }

    /**
     * try get a shared hash builder factory form cache
     *
     * @param builderNodeId the build plan node id
     * @return the shared hash builder operator factory
     */
    public static OmniHashBuilderWithExprOperatorFactory getHashBuilderOperatorFactory(Integer builderNodeId) {
        ref.computeIfAbsent(builderNodeId, key -> new AtomicInteger(0));
        ref.get(builderNodeId).incrementAndGet();
        return factoryCache.get(builderNodeId);
    }

    /**
     * try close and remove a shared hash builder factory form cache
     *
     * @param builderNodeId the build plan node id
     */
    public static void dereferenceHashBuilderOperatorAndFactory(Integer builderNodeId) {
        if (ref.get(builderNodeId).decrementAndGet() == 0) {
            ref.remove(builderNodeId);
            operatorCache.get(builderNodeId).close();
            operatorCache.remove(builderNodeId);
            factoryCache.get(builderNodeId).close();
            factoryCache.remove(builderNodeId);
        }
    }

    /**
     * The Factory context.
     *
     * @since 2021-10-16
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final JoinType joinType;

        private final DataType[] buildTypes;

        private final String[] buildHashKeys;

        private final int operatorCount;

        private final OperatorConfig operatorConfig;

        private BuildSide buildSide = BuildSide.BUILD_UNKNOWN;

        /**
         * Instantiates a new Context.
         *
         * @param joinType the join type
         * @param buildTypes the build types
         * @param buildHashKeys the build hash keys
         * @param operatorCount the operator count
         * @param operatorConfig the operator config
         */
        public FactoryContext(JoinType joinType, DataType[] buildTypes, String[] buildHashKeys, int operatorCount,
                OperatorConfig operatorConfig) {
            this.joinType = requireNonNull(joinType, "joinType");
            this.buildTypes = requireNonNull(buildTypes, "buildTypes");
            this.buildHashKeys = requireNonNull(buildHashKeys, "buildHashKeys");
            this.operatorCount = operatorCount;
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        /**
         *  Instantiates a new Context.
         *
         * @param joinType the join type
         * @param buildSide the build side
         * @param buildTypes the build types
         * @param buildHashKeys the build hash keys
         * @param operatorCount the operator count
         * @param operatorConfig the operator config
         */
        public FactoryContext(JoinType joinType, BuildSide buildSide, DataType[] buildTypes, String[] buildHashKeys,
                int operatorCount, OperatorConfig operatorConfig) {
            this(joinType, buildTypes, buildHashKeys, operatorCount, operatorConfig);
            this.buildSide = buildSide;
        }

        @Override
        public int hashCode() {
            return Objects.hash(joinType, Arrays.hashCode(buildTypes), Arrays.hashCode(buildHashKeys), operatorCount,
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
            return joinType.equals(that.joinType) && Arrays.equals(buildTypes, that.buildTypes)
                    && Arrays.equals(buildHashKeys, that.buildHashKeys) && operatorCount == that.operatorCount
                    && operatorConfig.equals(that.operatorConfig);
        }
    }
}
