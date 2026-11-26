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

#include "planNode.h"

namespace omniruntime {
const SortOrderInfo K_ASC_NULLS_FIRST(true, true);
const SortOrderInfo K_ASC_NULLS_LAST(true, false);
const SortOrderInfo K_DESC_NULLS_FIRST(false, true);
const SortOrderInfo K_DESC_NULLS_LAST(false, false);
} // namespace omniruntime
