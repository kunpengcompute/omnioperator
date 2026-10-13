/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#pragma once
#include "connectors/Connector.h"
#include "codegen/ScanSpec.h"
#include "type/Subfield.h"
#include "type/data_type.h"
#include "reader/Reader.h"
#include "reader/ReaderFactory.h"
#include "reader/orc/OrcReader.h"
#include "reader/BufferInput.h"
#include "reader/common/UriInfo.h"
#include "HiveConfig.h"
#include "HiveConnectorSplit.h"
#include "HiveConnectorUtil.h"
#include <orc/sargs/SearchArgument.hh>

// Forward decl — full definition pulled in by reader/orc/OrcReader.h above,
// but we keep this here to make the dependency explicit for the optional
// orcMemoryPool parameter on configureReaderOptions().
namespace orc { class MemoryPool; }

namespace omniruntime::connector::hive {

class HiveColumnHandle;
class HiveTableHandle;
class HiveConfig;
struct HiveConnectorSplit;
using omniruntime::reader::ReaderOptions;

struct SpecialColumnNames {
    std::optional<std::string> rowIndex;
    std::optional<std::string> rowId;
};

std::shared_ptr<codegen::ScanSpec> makeScanSpec(
    const vec::RowTypePtr& rowType,
    const std::unordered_map<std::string, std::vector<const type::Subfield*>>&
    outputSubfields,
    const vec::RowTypePtr& dataColumns,
    const std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>>&
    partitionKeys,
    const std::unordered_map<std::string, std::shared_ptr<HiveColumnHandle>>&
    infoColumns,
    const SpecialColumnNames& specialColumns);

// orcMemoryPool, when non-null, is used by the ORC branch instead of
// orc::getDefaultPool() so internal ORC allocations route through per-task
// accounting. Caller (SplitReader) owns the lifetime — typically a
// SplitReader-member OmniOrcPoolAdapter bound to the per-task pool threaded
// down from TableScanOperator.
void configureReaderOptions(
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    const std::shared_ptr<const HiveConnectorSplit> &hiveSplit,
    std::shared_ptr<ReaderOptions> &baseReaderOpts,
    ::orc::MemoryPool *orcMemoryPool = nullptr);

void configureRowReaderOptions(
    const std::shared_ptr<const HiveTableHandle>  &hiveTableHandle,
    const omniruntime::type::RowTypePtr &rowType,
    const omniruntime::type::RowTypePtr& fileRowType,
    const std::shared_ptr<omniruntime::codegen::ScanSpec> &scanSpec,
    const std::shared_ptr<const HiveConnectorSplit> &hiveSplit,
    const std::shared_ptr<const HiveConfig> &hiveConfig,
    std::shared_ptr<ReaderOptions> &baseReaderOpts);

std::shared_ptr<UriInfo> stringToUriInfo(std::string uriString);

}
