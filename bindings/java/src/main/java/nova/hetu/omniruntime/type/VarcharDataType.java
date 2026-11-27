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
 * varchar data type.
 *
 * @since 2021-08-05
 */
public class VarcharDataType extends DataType {
    /**
     * max width for varchar data type.
     */
    public static final int MAX_WIDTH = 1024 * 1024;

    /**
     * Varchar singleton.
     */
    public static final VarcharDataType VARCHAR = new VarcharDataType(MAX_WIDTH);

    private static final long serialVersionUID = -4778484134512020833L;

    /**
     * average length of a varchar.
     */
    @JsonProperty
    protected final int width;

    /**
     * The construct of varchar data type.
     *
     * @param width the width of varchar
     */
    public VarcharDataType(@JsonProperty("width") int width) {
        super(DataTypeId.OMNI_VARCHAR);
        this.width = Math.min(MAX_WIDTH, width);
    }

    /**
     * The construct of varchar data type.
     *
     * @param width the width of varchar
     * @param dataTypeId the types of data
     */
    protected VarcharDataType(int width, DataTypeId dataTypeId) {
        super(dataTypeId);
        this.width = width;
    }

    public int getWidth() {
        return width;
    }

    @Override
    public int hashCode() {
        return Objects.hash(width, super.getId());
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        VarcharDataType other = (VarcharDataType) obj;
        return (Objects.equals(width, other.getWidth()) && Objects.equals(super.getId(), other.getId()));
    }
}
