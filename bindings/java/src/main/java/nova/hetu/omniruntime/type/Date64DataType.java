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
 * date64 data type.
 *
 * @since 2021-08-05
 */
public class Date64DataType extends DataType {
    /**
     * Date64 singleton.
     */
    public static final Date64DataType DATE64 = new Date64DataType(DateUnit.DAY);

    private static final long serialVersionUID = -6927167052418618260L;

    @JsonProperty
    private final DataType.DateUnit dateUnit;

    /**
     * date 64 construct.
     *
     * @param dateUnit the unit of date
     */
    public Date64DataType(@JsonProperty("dateUnit") DataType.DateUnit dateUnit) {
        super(DataTypeId.OMNI_DATE64);
        this.dateUnit = dateUnit;
    }

    public DateUnit getDateUnit() {
        return dateUnit;
    }

    @Override
    public int hashCode() {
        return Objects.hash(dateUnit, super.getId());
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        Date64DataType other = (Date64DataType) obj;
        return (Objects.equals(dateUnit, other.getDateUnit()) && Objects.equals(super.getId(), other.getId()));
    }
}
