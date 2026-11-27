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

package nova.hetu.omniruntime.type;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.Objects;

/**
 * Decimal data type.
 *
 * @since 2022-03-10
 */
public abstract class DecimalDataType extends DataType {
    private static final long serialVersionUID = -3389964658615782592L;

    @JsonProperty
    private final int precision;

    @JsonProperty
    private final int scale;

    /**
     * Construct of decimal data type.
     *
     * @param precision the precision of decimal
     * @param scale the scale of decimal
     * @param typeId the data typeId
     */
    public DecimalDataType(@JsonProperty("precision") int precision, @JsonProperty("scale") int scale,
            @JsonProperty("id") DataTypeId typeId) {
        super(typeId);
        this.precision = precision;
        this.scale = scale;
    }

    public int getPrecision() {
        return precision;
    }

    public int getScale() {
        return scale;
    }

    @Override
    public int hashCode() {
        return Objects.hash(precision, scale, super.getId());
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        DecimalDataType other = (DecimalDataType) obj;
        return (Objects.equals(precision, other.getPrecision()) && Objects.equals(scale, other.getScale())
                && Objects.equals(super.getId(), other.getId()));
    }
}
