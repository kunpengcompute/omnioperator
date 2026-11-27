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

package nova.hetu.omniruntime.operator.limit;

import static java.util.Objects.requireNonNull;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;

/**
 * The type Omni distinct limit operator factory.
 *
 * @since 2021-06-30
 */
public class OmniDistinctLimitOperatorFactory
        extends OmniOperatorFactory<OmniDistinctLimitOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni distinct limit operator factory.
     *
     * @param sourceTypes the data types of each column
     * @param distinctCols the column index
     * @param hashCol col index of precomputed hash values
     * @param limit the limit count
     * @param operatorConfig the operator config
     */
    public OmniDistinctLimitOperatorFactory(DataType[] sourceTypes, int[] distinctCols, int hashCol, long limit,
            OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, distinctCols, hashCol, limit, operatorConfig));
    }

    /**
     * Instantiates a new Omni distinct limit operator factory with default operator
     * config.
     *
     * @param sourceTypes the data types of each column
     * @param distinctCols the column index
     * @param hashCol col index of precomputed hash values
     * @param limit the limit count
     */
    public OmniDistinctLimitOperatorFactory(DataType[] sourceTypes, int[] distinctCols, int hashCol, long limit) {
        this(sourceTypes, distinctCols, hashCol, limit, new OperatorConfig());
    }

    private static native long createDistinctLimitOperatorFactory(String sourceTypes, int[] distinctCols, int hashCol,
            long limit);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        return createDistinctLimitOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes),
                context.distinctCols, context.hashCol, context.limit);
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final int[] distinctCols;

        private final int hashCol;

        private final long limit;

        private final OperatorConfig operatorConfig;

        /**
         * Instantiates a new Context.
         *
         * @param sourceTypes the data types of each column
         * @param distinctCols the column index
         * @param hashCol col index of precomputed hash values
         * @param limit the limit count
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, int[] distinctCols, int hashCol, long limit,
                OperatorConfig operatorConfig) {
            this.sourceTypes = requireNonNull(sourceTypes, "Source types array is null.");
            this.distinctCols = requireNonNull(distinctCols, "Distinct cols array is null.");
            this.limit = limit;
            this.hashCol = hashCol;
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
            FactoryContext that = (FactoryContext) obj;
            return Arrays.equals(sourceTypes, that.sourceTypes) && Arrays.equals(distinctCols, that.distinctCols)
                    && limit == that.limit && hashCol == that.hashCol && operatorConfig.equals(that.operatorConfig);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), Arrays.hashCode(distinctCols), limit, hashCol,
                    operatorConfig);
        }
    }
}
