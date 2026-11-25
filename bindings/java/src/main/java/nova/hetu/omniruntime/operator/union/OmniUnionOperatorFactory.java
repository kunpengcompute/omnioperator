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

package nova.hetu.omniruntime.operator.union;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni union operator factory.
 *
 * @since 2021-06-30
 */
public class OmniUnionOperatorFactory extends OmniOperatorFactory<OmniUnionOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni union operator factory.
     *
     * @param sourceTypes the source type
     * @param isDistinct mark union or union all
     * @param operatorConfig the operator config
     */
    public OmniUnionOperatorFactory(DataType[] sourceTypes, boolean isDistinct, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, isDistinct, operatorConfig));
    }

    /**
     * Instantiates a new Omni union operator factory with default operator config.
     *
     * @param sourceTypes the source type
     * @param isDistinct mark union or union all
     */
    public OmniUnionOperatorFactory(DataType[] sourceTypes, boolean isDistinct) {
        this(sourceTypes, isDistinct, new OperatorConfig());
    }

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createUnionOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes), context.isDistinct);
    }

    private static native long createUnionOperatorFactory(String sourceTypes, boolean isDistinct);

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final boolean isDistinct;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Jit context.
         *
         * @param sourceTypes the source types
         * @param isDistinct the is distinct
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, boolean isDistinct, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.isDistinct = isDistinct;
            this.operatorConfig = operatorConfig;
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
            return isDistinct == context.isDistinct && Arrays.equals(sourceTypes, context.sourceTypes)
                    && operatorConfig.equals(context.operatorConfig);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), isDistinct, operatorConfig);
        }
    }
}
