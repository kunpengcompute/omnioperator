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

package nova.hetu.omniruntime.constants;

/**
 * The type Status.
 *
 * @since 2021-06-30
 */
public class Status extends Constant {
    /**
     * The constant OMNI_STATUS_NORMAL.
     */
    public static final Status OMNI_STATUS_NORMAL = new Status(0);

    /**
     * The constant OMNI_STATUS_ERROR.
     */
    public static final Status OMNI_STATUS_ERROR = new Status(-1);

    /**
     * The constant OMNI_STATUS_FINISHED.
     */
    public static final Status OMNI_STATUS_FINISHED = new Status(1);

    private static final long serialVersionUID = -3424552555224669902L;

    /**
     * Instantiates a new Status.
     *
     * @param value the value
     */
    public Status(int value) {
        super(value);
    }
}
