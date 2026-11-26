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
 *
 * Existing third-party copyright and license terms continue to apply.
 *
 * Third-party license and source notices
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
