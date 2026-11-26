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

package nova.hetu.omniruntime.type;

/**
 * container data type.
 *
 * @since 2021-07-17
 */
public class ContainerDataType extends DataType {
    /**
     * Container singleton.
     */
    public static final ContainerDataType CONTAINER = new ContainerDataType();

    private static final long serialVersionUID = 7653293048781110462L;

    private DataType[] fieldTypes;

    /**
     * The construct of container data type.
     *
     * @param fieldTypes the types of data
     */
    public ContainerDataType(DataType[] fieldTypes) {
        super(DataTypeId.OMNI_CONTAINER);
        this.fieldTypes = fieldTypes;
    }

    /**
     * Container construct.
     */
    public ContainerDataType() {
        super(DataTypeId.OMNI_CONTAINER);
    }

    /**
     * get number of filed types.
     *
     * @return the number of filedTypes
     */
    public int size() {
        return fieldTypes.length;
    }

    /**
     * get field types.
     *
     * @return field types
     */
    public DataType[] getFieldTypes() {
        return fieldTypes;
    }
}
