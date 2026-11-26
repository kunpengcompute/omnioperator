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

package nova.hetu.omniruntime.operator.filter;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * The type Omni filter and project operator factory.
 *
 * @since 2021-06-30
 */
public class OmniFilterAndProjectOperatorFactory
        extends OmniOperatorFactory<OmniFilterAndProjectOperatorFactory.FactoryContext> {
    private boolean isSupported;

    /**
     * Instantiates a new Omni filter and project operator factory.
     *
     * @param expression the expression
     * @param inputTypes the input types
     * @param projections the projections
     * @param operatorConfig the operator config
     */
    public OmniFilterAndProjectOperatorFactory(String expression, DataType[] inputTypes, List<String> projections,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(expression, inputTypes, projections, operatorConfig));
    }

    /**
     * Instantiates a new Omni filter and project operator factory with default
     * operator config.
     *
     * @param expression the expression
     * @param inputTypes the input types
     * @param projections the projections
     */
    public OmniFilterAndProjectOperatorFactory(String expression, DataType[] inputTypes, List<String> projections) {
        this(expression, inputTypes, projections, new OperatorConfig());
    }

    /**
     * Instantiates a new Omni filter and project operator factory with configured
     * expression parsing format.
     *
     * @param expression the expression
     * @param inputTypes the input types
     * @param projections the projections
     * @param parseFormat the format to parse expression into
     * @param operatorConfig the operator config
     */
    public OmniFilterAndProjectOperatorFactory(String expression, DataType[] inputTypes, List<String> projections,
            int parseFormat, OperatorConfig operatorConfig) {
        super(new FactoryContext(expression, inputTypes, projections, parseFormat, operatorConfig));
    }

    /**
     * Instantiates a new Omni filter and project operator factory with configured
     * expression parsing format with default operator config.
     *
     * @param expression the expression
     * @param inputTypes the input types
     * @param projections the projections
     * @param parseFormat the format to parse expression into
     */
    public OmniFilterAndProjectOperatorFactory(String expression, DataType[] inputTypes, List<String> projections,
            int parseFormat) {
        this(expression, inputTypes, projections, parseFormat, new OperatorConfig());
    }

    private static native long createFilterAndProjectOperatorFactory(String inputTypes, int inputLength,
            String expression, Object[] projections, int projectLength, int parseFormat, String operatorConfig);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        long factoryAddr = createFilterAndProjectOperatorFactory(DataTypeSerializer.serialize(context.inputTypes),
                context.inputTypes.length, context.expression, context.projections.toArray(),
                context.projections.size(), context.parseFormat, OperatorConfig.serialize(context.operatorConfig));
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
     * @since 20210630
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] inputTypes;

        private final String expression;

        private final List<String> projections;

        private final int parseFormat;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param expression the expression
         * @param inputTypes the input types
         * @param projections the projections
         * @param operatorConfig the operator config
         */
        public FactoryContext(String expression, DataType[] inputTypes, List<String> projections,
                OperatorConfig operatorConfig) {
            this(expression, inputTypes, projections, 0, operatorConfig);
        }

        /**
         * Instantiates a new Context with configured parsing format of the expression.
         *
         * @param expression the expression
         * @param inputTypes the input types
         * @param projections the projections
         * @param parseFormat the parsing format of expressions
         * @param operatorConfig the operator config
         */
        public FactoryContext(String expression, DataType[] inputTypes, List<String> projections, int parseFormat,
                OperatorConfig operatorConfig) {
            this.inputTypes = requireNonNull(inputTypes, "Input types array is null.");
            this.expression = requireNonNull(expression, "Expression is null.");
            this.projections = requireNonNull(projections, "Project indices is null.");
            this.parseFormat = parseFormat;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public int hashCode() {
            return Objects.hash(expression, Arrays.hashCode(inputTypes), Objects.hashCode(projections), parseFormat,
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
            return Objects.equals(expression, that.expression) && Arrays.equals(inputTypes, that.inputTypes)
                    && Objects.equals(projections.stream().sorted().collect(Collectors.toList()),
                            that.projections.stream().sorted().collect(Collectors.toList()))
                    && parseFormat == that.parseFormat && operatorConfig.equals(that.operatorConfig);
        }
    }
}
