/**
* Copyright (C) 2025-2025. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PREDICATEUTIL_H
#define PREDICATEUTIL_H

#include "PredicateCondition.h"
#include <set>
#include "../common/TimeRebaseInfo.h"
#include "memory/omni_task_memory_pool.h"

namespace common {
    std::unique_ptr<PredicateCondition> BuildVecPredicateCondition(nlohmann::json &json, int32_t columnCount);

    // Build evaluator from residual JSON subtree (no outer wrap, no rebase).
    std::unique_ptr<PredicateCondition> BuildResidualPredicateCondition(nlohmann::json &jsonCondition,
                                                                        int32_t columnCount);

    std::unique_ptr<PredicateCondition> BuildVecPredicateConditionWithRebase(
            nlohmann::json &json,
            int32_t columnCount,
            std::unique_ptr<common::TimeRebaseInfo> rebaseInfo
    );

    // The trailing OmniTaskMemoryPool* threads the per-task pool through to the hot
    // Vector allocation at PredicateUtil.cpp:426. All in-tree callers (OrcReader.cpp:22
    // and ParquetReader.cpp:60) pass an explicit pool. Any future caller MUST thread
    // pool through explicitly.
    bool GetFlatBaseVectorsFromBitMark(std::vector<BaseVector *> &baseVectors, std::vector<BaseVector *> &result,
        uint8_t *bitMark, int32_t vectorSize, const std::set<int32_t>& isNullSet, const std::set<int32_t>& isNotNullSet,
        omniruntime::mem::OmniTaskMemoryPool *pool);
}

#endif //PREDICATEUTIL_H
