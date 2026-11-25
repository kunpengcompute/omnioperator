/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
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

package nova.hetu.omniruntime.operator;

/**
 * The type Omni operator factory context.
 *
 * @since 2021-06-30
 */
public abstract class OmniOperatorFactoryContext {
    /**
     * Switch for configuring factory cache defaults
     */
    private static boolean defaultNeedCacheValue = true;

    /**
     * Whether the omni operator factory needs to be cached.
     */
    private boolean isNeedCache = defaultNeedCacheValue;

    /**
     * Instantiates a new Omni operator factory context.
     */
    public OmniOperatorFactoryContext() {
    }

    /**
     * Interface for setting default values for engine initialization
     *
     * @param value enable op factory cache
     */
    public static void setDefaultNeedCacheValue(boolean value) {
        defaultNeedCacheValue = value;
    }

    /**
     * Get the flag needCache whether the omni operator factory needs to be cached.
     *
     * @return the flag needCache
     */
    public boolean isNeedCache() {
        return isNeedCache;
    }

    /**
     * Set the flag needCache whether the omni operator factory needs to be cached.
     *
     * @param isNeedCache the flag needCache
     */
    public void setNeedCache(boolean isNeedCache) {
        this.isNeedCache = isNeedCache;
    }
}
