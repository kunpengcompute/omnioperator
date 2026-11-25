/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * @Description: vector batch spiller implements
 */
#include "error_code.h"
#include <unordered_map>

namespace omniruntime {
namespace op {
static std::unordered_map<ErrorCode, std::pair<std::string, std::string>> errorMessages = {
        { ErrorCode::SUCCESS,{ "SUCCESS", "" } },
        { ErrorCode::EMPTY_PATH, { "EMPTY_PATH", "Enable spill but do not config spill path." } },
        { ErrorCode::INVALID_PATH, { "INVALID_PATH", "The path is invalid." } },
        { ErrorCode::PATH_EXIST, { "PATH_EXIST", "It is dangerous to use existed path to spill." } },
        { ErrorCode::MKDIR_FAILED, { "MKDIR_FAILED", "Create spill directory failed." } },
        { ErrorCode::DISK_STAT_FAILED, { "DISK_STAT_FAILED", "Get disk space failed." } },
        { ErrorCode::DISK_SPACE_NOT_ENOUGH,{ "DISK_SPACE_NOT_ENOUGH", "The disk available size is smaller "
            "than the max spill size." } },
        { ErrorCode::MKSTEMP_FAILED, { "MKSTEMP_FAILED", "Create template file failed." } },
        { ErrorCode::UNLINK_FAILED, { "UNLINK_FAILED", "Unlink file failed." } },
        { ErrorCode::WRITE_FAILED, { "WRITE_FAILED", "Write data to file failed." } },
        { ErrorCode::EXCEED_SPILL_THRESHOLD,{ "EXCEED_SPILL_THRESHOLD", "Current the data size of spill "
            "exceeds the limit." } },
        { ErrorCode::READ_FAILED, { "READ_FAILED", "Read data from file failed." } },
        { ErrorCode::LOAD_LAZY_VECTOR_FAILED, { "LOAD_LAZY_VECTOR_FAILED", "Load lazy vector failed."} },
        { ErrorCode::MEM_CAP_EXCEEDED, { "MEM_CAP_EXCEEDED", "Exceeded memory cap of MB:"} } };

std::string &GetErrorCode(const ErrorCode &errorCode)
{
    return errorMessages[errorCode].first;
}

std::string &GetErrorMessage(const ErrorCode &errorCode)
{
    return errorMessages[errorCode].second;
}
}
}
