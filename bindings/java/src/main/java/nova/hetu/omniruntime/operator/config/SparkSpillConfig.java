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

package nova.hetu.omniruntime.operator.config;

import java.util.Objects;

/**
 * spark spill config.
 *
 * @since 2022-04-16
 */
public class SparkSpillConfig extends SpillConfig {
    private int numElementsForSpillThreshold;
    private double memUsageFractionForSpillThreshold;

    /**
     * Instantiates a new spark spill config.
     */
    public SparkSpillConfig() {
        super();
        numElementsForSpillThreshold = Integer.MAX_VALUE;
        memUsageFractionForSpillThreshold = 0.9;
    }

    /**
     * Instantiates a new spark spill config.
     *
     * @param spillPath the spill path
     * @param numElementsForSpillThreshold the num elements for spill threshold
     */
    public SparkSpillConfig(String spillPath, int numElementsForSpillThreshold) {
        this(true, spillPath, DEFAULT_MAX_SPILL_BYTES, numElementsForSpillThreshold);
        this.memUsageFractionForSpillThreshold = 0.9; // default memory usage fraction for spill threshold
    }

    /**
     * Instantiates a new spark spill config.
     *
     * @param isSpillEnabled the spill enabled
     * @param spillPath the spill path
     * @param maxSpillBytes the max spill bytes
     * @param numElementsForSpillThreshold the num elements for spill threshold
     */
    public SparkSpillConfig(boolean isSpillEnabled, String spillPath, long maxSpillBytes,
            int numElementsForSpillThreshold) {
        super(SpillConfigId.SPILL_CONFIG_SPARK, isSpillEnabled, spillPath, maxSpillBytes, DEFAULT_WRITE_BUFFER_SIZE);
        this.numElementsForSpillThreshold = numElementsForSpillThreshold;
        this.memUsageFractionForSpillThreshold = 0.9; // default memory usage fraction for spill threshold
    }

    /**
     * Instantiates a new spark spill config.
     *
     * @param isSpillEnabled the spill enabled
     * @param spillPath the spill path
     * @param maxSpillBytes the max spill bytes
     * @param numElementsForSpillThreshold the num elements for spill threshold
     * @param memUsageFractionForSpillThreshold the memory usage fraction for spill threshold
     * @param writeBufferSize the spill write buffer size
     */
    public SparkSpillConfig(boolean isSpillEnabled, String spillPath, long maxSpillBytes,
            int numElementsForSpillThreshold, double memUsageFractionForSpillThreshold, long writeBufferSize) {
        super(SpillConfigId.SPILL_CONFIG_SPARK, isSpillEnabled, spillPath, maxSpillBytes, writeBufferSize);
        this.numElementsForSpillThreshold = numElementsForSpillThreshold;
        setMemUsageFractionForSpillThreshold(memUsageFractionForSpillThreshold);
    }

    /**
     * get the num elements for spill threshold.
     *
     * @return the num elements for spill threshold
     */
    public int getNumElementsForSpillThreshold() {
        return numElementsForSpillThreshold;
    }

    /**
     * set the num elements for spill threshold.
     *
     * @param numElementsForSpillThreshold the num elements for spill threshold
     */
    public void setNumElementsForSpillThreshold(int numElementsForSpillThreshold) {
        this.numElementsForSpillThreshold = numElementsForSpillThreshold;
    }

    /**
     * set the memory usage fraction for spill threshold.
     *
     * @param memUsageFractionForSpillThreshold the memory usage fraction for spill
     *            threshold
     */
    public void setMemUsageFractionForSpillThreshold(double memUsageFractionForSpillThreshold) {
        if (!(memUsageFractionForSpillThreshold > 0.0 && memUsageFractionForSpillThreshold <= 1.0)) {
            throw new IllegalArgumentException("Spill memory fraction must be in (0, 1]");
        }
        this.memUsageFractionForSpillThreshold = memUsageFractionForSpillThreshold;
    }

    /**
     * get the memory usage fraction for spill threshold.
     *
     * @return the memory usage fraction in (0, 1]
     */
    public double getMemUsageFractionForSpillThreshold() {
        return memUsageFractionForSpillThreshold;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        if (!super.equals(obj)) {
            return false;
        }
        SparkSpillConfig that = (SparkSpillConfig) obj;
        return numElementsForSpillThreshold == that.numElementsForSpillThreshold
                && Double.compare(memUsageFractionForSpillThreshold, that.memUsageFractionForSpillThreshold) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), numElementsForSpillThreshold, memUsageFractionForSpillThreshold);
    }
}
