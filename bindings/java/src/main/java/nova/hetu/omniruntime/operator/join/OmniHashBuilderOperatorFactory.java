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

import nova.hetu.omniruntime.constants.JoinType;
import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni hash builder operator factory.
 *
 * @since 2021-06-30
 */
public class OmniHashBuilderOperatorFactory extends OmniOperatorFactory<OmniHashBuilderOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni hash builder operator factory.
     *
     * @param joinType the join type
     * @param buildTypes the build types
     * @param buildHashCols the build hash cols
     * @param operatorCount the operator count
     * @param operatorConfig the operator config
     */
    public OmniHashBuilderOperatorFactory(JoinType joinType, DataType[] buildTypes, int[] buildHashCols,
            int operatorCount, OperatorConfig operatorConfig) {
        super(new FactoryContext(joinType, buildTypes, buildHashCols, operatorCount, operatorConfig));
    }

    /**
     * Instantiates a new Omni hash builder operator factory with default operator
     * config.
     *
     * @param joinType the join type
     * @param buildTypes the build types
     * @param buildHashCols the build hash cols
     * @param operatorCount the operator count
     */
    public OmniHashBuilderOperatorFactory(JoinType joinType, DataType[] buildTypes, int[] buildHashCols,
            int operatorCount) {
        this(joinType, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
    }

    private static native long createHashBuilderOperatorFactory(int joinType, String buildTypes, int[] buildHashCols,
            int operatorCount, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createHashBuilderOperatorFactory(context.joinType.getValue(),
                DataTypeSerializer.serialize(context.buildTypes), context.buildHashCols, context.operatorCount,
                OperatorConfig.serialize(context.operatorConfig));
    }

    /**
     * The type Factory context.
     *
     * @since 20210630
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final JoinType joinType;

        private final DataType[] buildTypes;

        private final int[] buildHashCols;

        private final int operatorCount;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param joinType the join type
         * @param buildTypes the build types
         * @param buildHashCols the build hash cols
         * @param operatorCount the operator count
         * @param operatorConfig the operator config
         */
        public FactoryContext(JoinType joinType, DataType[] buildTypes, int[] buildHashCols, int operatorCount,
                OperatorConfig operatorConfig) {
            this.joinType = requireNonNull(joinType, "joinType");
            this.buildTypes = requireNonNull(buildTypes, "buildTypes");
            this.buildHashCols = requireNonNull(buildHashCols, "buildHashCols");
            this.operatorCount = operatorCount;
            this.operatorConfig = operatorConfig;
            setNeedCache(false);
        }

        @Override
        public int hashCode() {
            return Objects.hash(joinType, Arrays.hashCode(buildTypes), Arrays.hashCode(buildHashCols), operatorCount,
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
                    && Arrays.equals(buildHashCols, that.buildHashCols) && operatorCount == that.operatorCount
                    && operatorConfig.equals(that.operatorConfig);
        }
    }
}
