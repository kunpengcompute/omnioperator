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

#include "gtest/gtest.h"
#include "util/test_util.h"

namespace omniruntime {
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace std;
using namespace TestUtil;
using VarcharVector = vec::Vector<vec::LargeStringContainer<std::string_view>>;

const int32_t ROW_SIZE = 1000000;
const int32_t VAR_LEN = 10;
const int32_t ROUNDS = 10;

const std::string STRING = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static string GetString(int32_t index, int32_t offset, int32_t width)
{
    std::string str;
    for (int j = 0; j < width; j++) {
        char c = STRING.at((index + offset + j) % STRING.length());
        str.push_back(c);
    }
    return str;
}

static bool hashagg_equal(BaseVector *vector1, uint32_t offset1, BaseVector *vector2, uint32_t offset2)
{
    bool isInputNull1 = vector1->IsNull(offset1);
    bool isInputNull2 = vector2->IsNull(offset2);
    if (!isInputNull1 && !isInputNull2) {
        auto data1 = static_cast<VarcharVector *>(vector1)->GetValue(offset1);
        auto data2 = static_cast<VarcharVector *>(vector2)->GetValue(offset2);
        bool isSame = data1.compare(data2) == 0;
        return isSame;
    }
    if (isInputNull1 != isInputNull2) {
        return false;
    }
    return true;
}

static void PrintResultByHashAggEqual(BaseVector *vector1, BaseVector *vector2, Timer &timer)
{
    double sum = 0.0f;
    bool isEqual;

    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            isEqual = hashagg_equal(vector1, i, vector2, i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "isEqual: " << isEqual << std::endl;
}

static bool join_equal(VarcharVector *leftVector, int32_t leftIndex, VarcharVector *rightVector, int32_t rightIndex)
{
    auto leftValue = leftVector->GetValue(leftIndex);
    auto rightValue = rightVector->GetValue(rightIndex);
    if (leftValue.size() != rightValue.size()) {
        return false;
    }
    if (leftValue.compare(rightValue) == 0) {
        return true;
    } else {
        return false;
    }
}

static void PrintResultByJoinEqual(VarcharVector *vector1, VarcharVector *vector2, Timer &timer)
{
    double sum = 0.0f;
    bool isEqual;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            isEqual = join_equal(vector1, i, vector2, i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "isEqual: " << isEqual << std::endl;
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
}

TEST(varcharType, VarcharValueEqualsValueIgnoreNullsPerf)
{
    VarcharVector *vector1 = new VarcharVector(ROW_SIZE);
    VarcharVector *vector2 = new VarcharVector(ROW_SIZE);

    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 10, VAR_LEN);
        std::string_view value(str);
        vector1->SetValue(i, value);
        vector2->SetValue(i, value);
    }

    std::cout << "Test times: " << ROW_SIZE << std::endl;
    std::cout << "varchar length: " << VAR_LEN << std::endl;

    Timer timer;
    timer.SetStart();

    std::cout << "Compare same varchar: " << std::endl;

    PrintResultByJoinEqual(vector1, vector2, timer);

    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 20, VAR_LEN);
        std::string_view value(str);
        vector3->SetValue(i, value);
    }

    std::cout << "Compare different varchar: " << std::endl;

    PrintResultByJoinEqual(vector1, vector3, timer);

    delete vector1;
    delete vector2;
    delete vector3;
}

TEST(varcharType, IsSameNodeFuncVarcharImplPerf)
{
    VarcharVector *vector1 = new VarcharVector(ROW_SIZE);
    VarcharVector *vector2 = new VarcharVector(ROW_SIZE);

    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 10, VAR_LEN);
        std::string_view value(str);
        vector1->SetValue(i, value);
        vector2->SetValue(i, value);
        vector1->SetNotNull(i);
        vector2->SetNotNull(i);
    }

    // Test perf
    std::cout << "Test times: " << ROW_SIZE << std::endl;
    std::cout << "varchar length: " << VAR_LEN << std::endl;

    Timer timer;
    timer.SetStart();

    std::cout << "Compare same varchar: " << std::endl;

    PrintResultByHashAggEqual(vector1, vector2, timer);

    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 20, VAR_LEN);
        std::string_view value(str);
        vector3->SetValue(i, value);
        vector3->SetNotNull(i);
    }

    std::cout << "Compare different varchar: " << std::endl;

    PrintResultByHashAggEqual(vector1, vector3, timer);

    delete vector1;
    delete vector2;
    delete vector3;
}
} // namespace omniruntime
