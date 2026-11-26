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

import java.io.Serializable;
import java.util.Objects;

/**
 * OverflowConfig
 *
 * @since 2022-08-09
 */
public class OverflowConfig implements Serializable {
    private static final long serialVersionUID = 393901615896834842L;

    private OverflowConfigId overflowConfigId;

    public OverflowConfig() {
        this(OverflowConfigId.OVERFLOW_CONFIG_EXCEPTION);
    }

    public OverflowConfig(OverflowConfigId overflowConfigId) {
        this.overflowConfigId = overflowConfigId;
    }

    public void setOverflowConfigId(OverflowConfigId overflowConfigId) {
        this.overflowConfigId = overflowConfigId;
    }

    public OverflowConfigId getOverflowConfigId() {
        return overflowConfigId;
    }

    /**
     * OverflowConfigId
     *
     * @since 2022-08-09
     */
    public enum OverflowConfigId {
        OVERFLOW_CONFIG_EXCEPTION,
        OVERFLOW_CONFIG_NULL
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        OverflowConfig that = (OverflowConfig) obj;
        return overflowConfigId == that.overflowConfigId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(overflowConfigId);
    }
}
