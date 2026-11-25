/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
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

import nova.hetu.omniruntime.operator.OmniOperator;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
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
 * The type Omni nested loop join builder operator factory.
 *
 * @since 2021-06-30
 */
public class OmniNestedLoopJoinBuildOperatorFactory
        extends OmniOperatorFactory<OmniNestedLoopJoinBuildOperatorFactory.FactoryContext> {
    /**
     * The global lock is used for sharing nested builder operator and factory
     * concurrently.
     */
    public static final Lock gLock = new ReentrantLock();

    /**
     * The hashmap cached the shared nested builder operator, the key is the build
     * plan node id.
     */
    private static Map<Integer, OmniOperator> operatorCache = new HashMap<>();

    /**
     * The hashmap cached the shared nested builder operator factory, the key is the
     * build plan node id.
     */
    private static Map<Integer, OmniNestedLoopJoinBuildOperatorFactory> factoryCache = new HashMap<>();

    /**
     * The hashmap stores the num of lookup which is in-use shared nested builder
     * operator
     * the key is the build plan node id.
     */
    private static Map<Integer, AtomicInteger> ref = new HashMap<>();

    /**
     * Instantiates a new Omni nested loop join builder operator factory.
     *
     * @param buildTypes the build types
     * @param buildOutputCols the build output cols
     */
    public OmniNestedLoopJoinBuildOperatorFactory(DataType[] buildTypes, int[] buildOutputCols) {
        super(new FactoryContext(buildTypes, buildOutputCols));
    }

    /**
     * save a new shared builder operator and factory into cache
     *
     * @param builderNodeId the build plan node id
     * @param factory the shared hash builder operator factory
     * @param operator the shared hash builder operator
     */
    public static void saveNestedLoopJoinBuilderOperatorAndFactory(Integer builderNodeId,
            OmniNestedLoopJoinBuildOperatorFactory factory, OmniOperator operator) {
        operatorCache.put(builderNodeId, operator);
        factoryCache.put(builderNodeId, factory);
    }

    /**
     * try get a shared nested builder factory form cache
     *
     * @param builderNodeId the build plan node id
     * @return the shared nested builder operator factory
     */
    public static OmniNestedLoopJoinBuildOperatorFactory getNestedLoopJoinBuilderOperatorFactory(
            Integer builderNodeId) {
        ref.computeIfAbsent(builderNodeId, key -> new AtomicInteger(0));
        ref.get(builderNodeId).incrementAndGet();
        return factoryCache.get(builderNodeId);
    }

    private static native long createNestedLoopJoinBuildOperatorFactory(String buildTypes, int[] buildOutputCols);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createNestedLoopJoinBuildOperatorFactory(DataTypeSerializer.serialize(context.buildTypes),
                context.buildOutputCols);
    }

    /**
     * try close and remove a shared nested builder factory form cache
     *
     * @param builderNodeId the build plan node id
     */
    public static void dereferenceNestedBuilderOperatorAndFactory(Integer builderNodeId) {
        if (ref.get(builderNodeId).decrementAndGet() == 0) {
            ref.remove(builderNodeId);
            operatorCache.get(builderNodeId).close();
            operatorCache.remove(builderNodeId);
            factoryCache.get(builderNodeId).close();
            factoryCache.remove(builderNodeId);
        }
    }

    /**
     * The type Factory context.
     *
     * @since 20241210
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] buildTypes;

        private final int[] buildOutputCols;

        /**
         * Instantiates a new Context.
         *
         * @param buildTypes the build types
         * @param buildOutputCols the build nested loop join cols
         */
        public FactoryContext(DataType[] buildTypes, int[] buildOutputCols) {
            this.buildTypes = requireNonNull(buildTypes, "buildTypes");
            this.buildOutputCols = requireNonNull(buildOutputCols, "buildOutputCols");
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(buildTypes), Arrays.hashCode(buildOutputCols));
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
            return Arrays.equals(buildTypes, that.buildTypes) && Arrays.equals(buildOutputCols, that.buildOutputCols);
        }
    }
}
