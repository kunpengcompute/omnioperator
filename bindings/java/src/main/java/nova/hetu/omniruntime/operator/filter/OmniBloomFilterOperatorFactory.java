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

package nova.hetu.omniruntime.operator.filter;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;

import java.util.Objects;

/**
 * The type Omni bloom filter operator factory.
 *
 * @since 2023-03-03
 */
public class OmniBloomFilterOperatorFactory extends OmniOperatorFactory<OmniBloomFilterOperatorFactory.FactoryContext> {
    public OmniBloomFilterOperatorFactory(int version, OperatorConfig operatorConfig) {
        super(new OmniBloomFilterOperatorFactory.FactoryContext(version, operatorConfig));
    }

    public OmniBloomFilterOperatorFactory(int version) {
        this(version, new OperatorConfig());
    }

    @Override
    protected long createNativeOperatorFactory(OmniBloomFilterOperatorFactory.FactoryContext context) {
        return createBloomFilterOperatorFactory(context.version);
    }

    private static native long createBloomFilterOperatorFactory(int version);

    /**
     * The type Factory context.
     *
     * @since 20230303
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final int version;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param version the bloom filter version
         * @param operatorConfig the operator config
         */
        public FactoryContext(int version, OperatorConfig operatorConfig) {
            this.version = version;
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
            return version == context.version && operatorConfig.equals(context.operatorConfig);
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.version, operatorConfig);
        }
    }
}
