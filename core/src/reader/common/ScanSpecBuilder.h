/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Predicate JSON → ScanSpec + per-column Filter (shared by ORC/Parquet).

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include "type/data_type.h"
#include "codegen/ScanSpec.h"
#include "reader/common/PredicateUtil.h"

namespace omniruntime::reader {

// Return true when all selected columns are scalar projection types currently
// supported by the selective reader. ARRAY, MAP, and STRUCT remain unsupported.
bool allSelectedColumnsAreSupported(const omniruntime::type::RowType &rowType);

// usable=false → parse failed, fall back to legacy path; needResidual=true → residualPredicate is the
// unpushed subtree (must call init(batchLen)).
std::shared_ptr<omniruntime::codegen::ScanSpec> makeScanSpec(
    const omniruntime::type::RowType &rowType,
    const std::shared_ptr<nlohmann::json> &enhancementJson,
    bool &usable, bool &needResidual,
    std::shared_ptr<::common::PredicateCondition> &residualPredicate);

} // namespace omniruntime::reader
