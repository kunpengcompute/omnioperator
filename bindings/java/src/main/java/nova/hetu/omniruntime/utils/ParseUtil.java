/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Copyright 2010 Proofpoint, Inc.
 *
 * Project license notice for OmniOperator
 * Existing file-specific copyright and license terms continue to apply.
 *
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Third-party license and source notices
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package nova.hetu.omniruntime.utils;

import static nova.hetu.omniruntime.memory.MemoryManager.UNLIMITED;

import com.sun.management.OperatingSystemMXBean;

import java.lang.management.ManagementFactory;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * parse memory size
 *
 * @since 2022-04-02
 */
public class ParseUtil {
    private static final Pattern PATTERN = Pattern.compile("^\\s*(\\d+(?:\\.\\d+)?)\\s*([a-zA-Z]+)\\s*$");

    private ParseUtil() {
    }

    /**
     * parse memory size to byte, like 1B, 1KB, 1MB, 1GB.
     *
     * @param size capacity size with unit
     * @return size in bytes
     */
    public static long parserMemoryParameters(String size) {
        Matcher matcher = PATTERN.matcher(size);
        if (!matcher.matches()) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_PARAM_ERROR,
                    "size is not a valid data size string" + size);
        }

        long value = Long.parseLong(matcher.group(1));
        String unitString = matcher.group(2);

        for (Unit unit : Unit.values()) {
            if (unit.getUnitString().equals(unitString)) {
                long limit = value * unit.getFactor();
                long systemFreeMemory = getOperatorSystemFreeMemorySize();
                if (limit >= systemFreeMemory || limit < UNLIMITED) {
                    throw new OmniRuntimeException(OmniErrorType.OMNI_PARAM_ERROR,
                            "OMNI_OFFHEAP_MEMORY_SIZE exceeds system free memorySize:" + systemFreeMemory);
                }
                return limit;
            }
        }
        throw new OmniRuntimeException(OmniErrorType.OMNI_PARAM_ERROR, "Unknown unit:" + unitString);
    }

    private static long getOperatorSystemFreeMemorySize() {
        java.lang.management.OperatingSystemMXBean langOSMXBean = ManagementFactory.getOperatingSystemMXBean();
        if (langOSMXBean instanceof OperatingSystemMXBean) {
            OperatingSystemMXBean osmxb = (OperatingSystemMXBean) langOSMXBean;
            return osmxb.getFreePhysicalMemorySize();
        }
        throw new OmniRuntimeException(OmniErrorType.OMNI_UNDEFINED, "Cannot get system freeMemorySize");
    }

    enum Unit {
        BYTE(1L, "B"),
        KILOBYTE(1L << 10, "KB"),
        MEGABYTE(1L << 20, "MB"),
        GIGABYTE(1L << 30, "GB"),
        TERABYTE(1L << 40, "TB");

        private final long factor;
        private final String unitString;

        Unit(long factor, String unitString) {
            this.factor = factor;
            this.unitString = unitString;
        }

        long getFactor() {
            return factor;
        }

        String getUnitString() {
            return unitString;
        }
    }
}
