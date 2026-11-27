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

package nova.hetu.omniruntime.operator;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.config.SparkSpillConfig;
import nova.hetu.omniruntime.operator.config.SpillConfig;

import org.testng.annotations.Test;

import java.nio.file.Paths;

/**
 * The omni operator config test.
 *
 * @since 2022-04-16
 */
public class OmniOperatorConfigTest {
    @Test
    public void TestSerialization() {
        final String spillPath = Paths.get("").toAbsolutePath().toString();

        // disable jit
        String noneConfigString = OperatorConfig.serialize(OperatorConfig.NONE);
        assertEquals(OperatorConfig.NONE, OperatorConfig.deserialize(noneConfigString));

        OperatorConfig invalidSpillConfig = new OperatorConfig(SpillConfig.INVALID);
        String invalidConfigString = OperatorConfig.serialize(invalidSpillConfig);
        assertEquals(invalidSpillConfig, OperatorConfig.deserialize(invalidConfigString));

        OperatorConfig sparkOperatorConfig1 = new OperatorConfig(new SparkSpillConfig(spillPath, 5));
        String sparkConfigString1 = OperatorConfig.serialize(sparkOperatorConfig1);
        assertEquals(sparkOperatorConfig1, OperatorConfig.deserialize(sparkConfigString1));

        OperatorConfig sparkOperatorConfig2 = new OperatorConfig(new SparkSpillConfig(false, spillPath, 1024, 1));
        String sparkConfigString2 = OperatorConfig.serialize(sparkOperatorConfig2);
        assertEquals(sparkOperatorConfig2, OperatorConfig.deserialize(sparkConfigString2));
    }

    @Test
    public void testSparkSpillFractionSerialization() {
        SparkSpillConfig defaults = new SparkSpillConfig("/tmp", 5);
        assertEquals(defaults.getMemUsageFractionForSpillThreshold(), 0.9);
        SparkSpillConfig fraction = new SparkSpillConfig(true, "/tmp", 1024, 5, 0.125, 0);
        OperatorConfig config = new OperatorConfig(fraction);
        String json = OperatorConfig.serialize(config);
        assertTrue(json.contains("\"memUsageFractionForSpillThreshold\":0.125"));
        assertEquals(OperatorConfig.deserialize(json), config);
    }

    @Test
    public void testSparkSpillFractionRejectsInvalidValues() {
        SparkSpillConfig config = new SparkSpillConfig("/tmp", 5);
        for (double value : new double[] {0.0, -0.1, 90.0, Double.NaN, Double.POSITIVE_INFINITY}) {
            try {
                config.setMemUsageFractionForSpillThreshold(value);
                fail("Expected invalid spill fraction to be rejected: " + value);
            } catch (IllegalArgumentException expected) {
                assertEquals(config.getMemUsageFractionForSpillThreshold(), 0.9);
            }
        }
    }

    @Test(expectedExceptions = IllegalArgumentException.class)
    public void testSparkSpillConstructorRejectsPercentage() {
        new SparkSpillConfig(true, "/tmp", 1024, 5, 90.0, 0);
    }
}
