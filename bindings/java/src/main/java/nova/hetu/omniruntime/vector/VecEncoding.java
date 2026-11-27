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

package nova.hetu.omniruntime.vector;

/**
 * vec encoding.
 *
 * @since 2022-02-17
 */
public enum VecEncoding {
    OMNI_VEC_ENCODING_FLAT,
    OMNI_VEC_ENCODING_DICTIONARY,
    OMNI_VEC_ENCODING_CONTAINER,
    OMNI_ENCODING_MAP,
    OMNI_ENCODING_ARRAY,
    OMNI_ENCODING_STRUCT,
    OMNI_VEC_ENCODING_LAZY,
    OMNI_VEC_ENCODING_INVALID
}
