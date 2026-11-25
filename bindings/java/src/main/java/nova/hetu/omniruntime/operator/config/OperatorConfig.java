/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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

package nova.hetu.omniruntime.operator.config;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_INNER_ERROR;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.util.Objects;

/**
 * operator config.
 *
 * @since 2022-04-16
 */
public class OperatorConfig {
    /**
     * NONE operator config.
     */
    public static final OperatorConfig NONE = new OperatorConfig();

    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    private SpillConfig spillConfig;

    private OverflowConfig overflowConfig;

    /**
     * When set to true, statistical aggregate function returns Double.NaN
     * if divide by zero occurred during expression evaluation, otherwise, it returns null.
     * Before Spark version 3.1.0, it returns NaN in divideByZero case by default.
     */
    private boolean isStatisticalAggregate;

    private boolean isSkipExpressionVerify;

    private int adaptivityThreshold = -1;

    private boolean isRowOutput = false;

    /**
     * Operator config default constructor.
     */
    public OperatorConfig() {
        this(SpillConfig.NONE, new OverflowConfig(), false);
    }

    /**
     * Operator config constructor.
     *
     * @param spillConfig the spill config
     */
    public OperatorConfig(SpillConfig spillConfig) {
        this(spillConfig, new OverflowConfig(), false);
    }

    /**
     * Operator config constructor.
     *
     * @param overflowConfig overflowConfig
     */
    public OperatorConfig(OverflowConfig overflowConfig) {
        this(SpillConfig.NONE, overflowConfig, false);
    }

    /**
     * Operator config constructor.
     *
     * @param spillConfig spillConfig
     * @param overflowConfig overflowConfig
     */
    public OperatorConfig(SpillConfig spillConfig, OverflowConfig overflowConfig) {
        this(spillConfig, overflowConfig, false);
    }

    /**
     * Operator config constructor.
     *
     * @param spillConfig the spill config
     * @param overflowConfig the overflow config
     * @param isSkipExpressionVerify whether to skip exprVerify
     */
    public OperatorConfig(SpillConfig spillConfig, OverflowConfig overflowConfig, boolean isSkipExpressionVerify) {
        this.spillConfig = spillConfig;
        this.overflowConfig = overflowConfig;
        this.isSkipExpressionVerify = isSkipExpressionVerify;
        this.isStatisticalAggregate = false;
    }

    public OperatorConfig(SpillConfig spillConfig, boolean isStatisticalAggregate, OverflowConfig overflowConfig,
            boolean isSkipExpressionVerify) {
        this.spillConfig = spillConfig;
        this.overflowConfig = overflowConfig;
        this.isSkipExpressionVerify = isSkipExpressionVerify;
        this.isStatisticalAggregate = isStatisticalAggregate;
    }

    /**
     * Operator config constructor.
     *
     * @param spillConfig the spill config
     * @param overflowConfig the overflow config
     * @param isSkipExpressionVerify whether to skip exprVerify
     * @param adaptivityThreshold an int for adaptivity of operator. For example,
     *            radix sort threshold for Sort
     */
    public OperatorConfig(SpillConfig spillConfig, OverflowConfig overflowConfig, boolean isSkipExpressionVerify,
            int adaptivityThreshold) {
        this(spillConfig, overflowConfig, isSkipExpressionVerify);
        this.adaptivityThreshold = adaptivityThreshold;
    }

    /**
     * Operator config constructor.
     *
     * @param spillConfig the spill config
     * @param overflowConfig the overflow config
     * @param isSkipExpressionVerify whether to skip exprVerify
     * @param isRowOutput true mean operator need to output row batch,
     */
    public OperatorConfig(SpillConfig spillConfig, OverflowConfig overflowConfig, boolean isSkipExpressionVerify,
            boolean isRowOutput) {
        this(spillConfig, overflowConfig, isSkipExpressionVerify);
        this.isRowOutput = isRowOutput;
    }

    /**
     * Get spill config.
     *
     * @return the spill config
     */
    public SpillConfig getSpillConfig() {
        return spillConfig;
    }

    /**
     * Get overflow config.
     *
     * @return the overflow config
     */
    public OverflowConfig getOverflowConfig() {
        return overflowConfig;
    }

    /**
     * Set spill config.
     *
     * @param spillConfig the spill config
     */
    public void setSpillConfig(SpillConfig spillConfig) {
        this.spillConfig = spillConfig;
    }

    /**
     * Set overflow config.
     *
     * @param overflowConfig overflowConfig
     */
    public void setOverflowConfig(OverflowConfig overflowConfig) {
        this.overflowConfig = overflowConfig;
    }

    /**
     * Set skipExpressionVerify
     *
     * @param isSkipExpressionVerify whether to skip exprVerify
     */
    public void setSkipExpressionVerify(boolean isSkipExpressionVerify) {
        this.isSkipExpressionVerify = isSkipExpressionVerify;
    }

    /**
     * Get skipExpressionVerify
     *
     * @return skipExpressionVerify
     */
    public boolean isSkipExpressionVerify() {
        return isSkipExpressionVerify;
    }

    /**
     * Get statisticalAggregate
     *
     * @return statisticalAggregate
     */
    public boolean isStatisticalAggregate() {
        return isStatisticalAggregate;
    }

    /**
     * Set statisticalAggregate.
     *
     * @param isStatisticalAggregate boolean
     */
    public void setStatisticalAggregate(boolean isStatisticalAggregate) {
        this.isStatisticalAggregate = isStatisticalAggregate;
    }

    /**
     * Set adaptivityThreshold
     *
     * @param adaptivityThreshold a threshold for some kind of adaptivity in operator
     */
    public void setAdaptivityThreshold(int adaptivityThreshold) {
        this.adaptivityThreshold = adaptivityThreshold;
    }

    public void setIsRowOutput(boolean inputRowOutput) {
        this.isRowOutput = inputRowOutput;
    }

    /**
     * Get adaptivityThreshold
     *
     * @return adaptivityThreshold
     */
    public int getAdaptivityThreshold() {
        return adaptivityThreshold;
    }

    public boolean getIsRowOutput() {
        return isRowOutput;
    }

    /**
     * Serialize operator config to string.
     *
     * @param operatorConfig the operator config
     * @return the string result of serialization
     */
    public static String serialize(OperatorConfig operatorConfig) {
        try {
            return OBJECT_MAPPER.writeValueAsString(operatorConfig);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Serialization failed.", e);
        }
    }

    /**
     * Deserialize string to the operator config.
     *
     * @param operatorConfigString the operator config string
     * @return the operator config of deserialization
     */
    public static OperatorConfig deserialize(String operatorConfigString) {
        try {
            return OBJECT_MAPPER.readerFor(OperatorConfig.class).readValue(operatorConfigString);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Deserialization failed.", e);
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        OperatorConfig that = (OperatorConfig) obj;
        return Objects.equals(spillConfig, that.spillConfig) && Objects.equals(overflowConfig, that.overflowConfig)
                && isSkipExpressionVerify == that.isSkipExpressionVerify
                && isStatisticalAggregate == that.isStatisticalAggregate
                && adaptivityThreshold == that.adaptivityThreshold && isRowOutput == that.isRowOutput;
    }

    @Override
    public int hashCode() {
        return Objects.hash(spillConfig, overflowConfig, isSkipExpressionVerify, isStatisticalAggregate,
                adaptivityThreshold, isRowOutput);
    }
}