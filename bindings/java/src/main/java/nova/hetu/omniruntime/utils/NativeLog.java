/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
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

package nova.hetu.omniruntime.utils;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * The global log init.
 *
 * @since 20220320
 */

public class NativeLog {
    private static volatile NativeLog instance;

    private static final Logger logger = LoggerFactory.getLogger(NativeLog.class);

    /**
     * Native Log init.
     *
     * @since 20220320
     */
    private NativeLog() {
        initLog();
    }

    /**
     * Native getInstance.
     *
     * @return new NativeLog
     */
    public static NativeLog getInstance() {
        if (instance == null) {
            synchronized (NativeLog.class) {
                if (instance == null) {
                    instance = new NativeLog();
                }
            }
        }
        return instance;
    }

    /**
     * Init global logger.
     */
    public static native void initLog();
}