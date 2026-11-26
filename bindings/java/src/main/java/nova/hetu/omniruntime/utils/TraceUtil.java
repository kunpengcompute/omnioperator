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

package nova.hetu.omniruntime.utils;

import static java.lang.Math.min;
import static java.lang.System.lineSeparator;

import java.util.Arrays;
import java.util.StringJoiner;

/**
 * trace util.
 *
 * @since 2021-10-21
 */
public class TraceUtil {
    /**
     * used for get stack trace of current thread.
     *
     * @return the stack trace of current thread
     */
    public static String stack() {
        StackTraceElement[] elements = Thread.currentThread().getStackTrace();
        StringJoiner stack = new StringJoiner(lineSeparator() + "\t");
        Arrays.stream(elements).skip(2).limit(min(25, elements.length - 1)).forEach(item -> {
            stack.add(item.toString());
        });
        return stack.toString();
    }
}
