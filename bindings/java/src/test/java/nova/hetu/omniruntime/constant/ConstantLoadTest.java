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

package nova.hetu.omniruntime.constant;

import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_SUM;
import static nova.hetu.omniruntime.constants.Status.OMNI_STATUS_ERROR;
import static nova.hetu.omniruntime.constants.Status.OMNI_STATUS_FINISHED;
import static nova.hetu.omniruntime.constants.Status.OMNI_STATUS_NORMAL;
import static org.testng.Assert.assertEquals;

import nova.hetu.omniruntime.OmniLibs;
import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.constants.Status;

import org.testng.annotations.Test;

/**
 * The type Constant load test.
 *
 * @since 2021-07-13
 */
public class ConstantLoadTest {
    /**
     * Test load constant.
     */
    @Test
    public void testLoadConstant() {
        Status status = OMNI_STATUS_NORMAL;
        assertEquals(status.getValue(), 0);
        status = OMNI_STATUS_ERROR;
        assertEquals(status.getValue(), -1);
        status = OMNI_STATUS_FINISHED;
        assertEquals(status.getValue(), 1);

        FunctionType functionType = OMNI_AGGREGATION_TYPE_SUM;
        assertEquals(functionType.getValue(), 0);
    }

    /**
     * Test equals.
     */
    @Test
    public void testEquals() {
        Status status = OMNI_STATUS_NORMAL;
        assertEquals(status.getValue(), 0);
    }

    /**
     * Test getVersion.
     */
    @Test
    public void testGetVersion() {
        String version = OmniLibs.getVersion();
        String expected = "Product Name: Kunpeng BoostKit" + System.lineSeparator()
                + "Product Version: 26.1.0" + System.lineSeparator() + "Component Name: BoostKit-omniop"
                + System.lineSeparator() + "Component Version: 2.2.0";
        assertEquals(version, expected);
    }
}
