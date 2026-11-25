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

package nova.hetu.omniruntime.operator.project;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni project operator factory.
 *
 * @since 2021-06-30
 */
public class OmniProjectOperatorFactory extends OmniOperatorFactory<OmniProjectOperatorFactory.FactoryContext> {
    private boolean isSupported;

    /**
     * Instantiates a new Omni project operator factory.
     *
     * @param expressions the expressions
     * @param inputTypes the input types
     * @param operatorConfig the operator config
     */
    public OmniProjectOperatorFactory(String[] expressions, DataType[] inputTypes, OperatorConfig operatorConfig) {
        super(new FactoryContext(expressions, inputTypes, operatorConfig));
    }

    /**
     * Instantiates a new Omni project operator factory with default operator
     * config.
     *
     * @param expressions the expressions
     * @param inputTypes the input types
     */
    public OmniProjectOperatorFactory(String[] expressions, DataType[] inputTypes) {
        this(expressions, inputTypes, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni project operator factory with configured expression
     * parsing format.
     *
     * @param expressions the expressions
     * @param inputTypes the input types
     * @param parseFormat the parse format
     * @param operatorConfig the operator config
     */
    public OmniProjectOperatorFactory(String[] expressions, DataType[] inputTypes, int parseFormat,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(expressions, inputTypes, parseFormat, operatorConfig));
    }

    /**
     * Instantiates a new Omni project operator factory with configured expression
     * parsing format with default operator config.
     *
     * @param expressions the expressions
     * @param inputTypes the input types
     * @param parseFormat the parse format
     */
    public OmniProjectOperatorFactory(String[] expressions, DataType[] inputTypes, int parseFormat) {
        this(expressions, inputTypes, parseFormat, new OperatorConfig());
    }

    private static native long createProjectOperatorFactory(String inputTypes, int inputLength, Object[] expressions,
            int expressionsLength, int parseFormat, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        long factoryAddr = createProjectOperatorFactory(DataTypeSerializer.serialize(context.inputTypes),
                context.inputTypes.length, context.expressions, context.expressions.length, context.parseFormat,
                OperatorConfig.serialize(context.operatorConfig));
        if (factoryAddr != 0) {
            isSupported = true;
        }
        return factoryAddr;
    }

    public boolean isSupported() {
        return isSupported;
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] inputTypes;

        private final String[] expressions;

        private final int parseFormat;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param expressions the expressions
         * @param inputTypes the input types
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] expressions, DataType[] inputTypes, OperatorConfig operatorConfig) {
            this(expressions, inputTypes, 0, operatorConfig);
        }

        /**
         * Instantiates a new Context with configured parsing format of the expression.
         *
         * @param expressions the expressions
         * @param inputTypes the input types
         * @param parseFormat the parse format
         * @param operatorConfig the operator config
         */
        public FactoryContext(String[] expressions, DataType[] inputTypes, int parseFormat,
                OperatorConfig operatorConfig) {
            this.inputTypes = requireNonNull(inputTypes, "Input types array is null.");
            this.expressions = requireNonNull(expressions, "Expressions is null.");
            this.parseFormat = parseFormat;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(inputTypes), Arrays.hashCode(expressions), parseFormat, operatorConfig);
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
            return Arrays.equals(expressions, that.expressions) && Arrays.equals(inputTypes, that.inputTypes)
                    && parseFormat == that.parseFormat && operatorConfig.equals(that.operatorConfig);
        }
    }
}
