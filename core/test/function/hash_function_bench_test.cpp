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
#include "operator/hash_util.h"
#include "util/test_util.h"
namespace omniruntime {
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace std;
using namespace TestUtil;

const int32_t ROW_SIZE = 1000000;
const int32_t VAR_LEN = 10;
const int32_t ROUNDS = 10;
const int64_t START = 0;
const int64_t END = 1000000;

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

TEST(varcharType, VarcharHashPerf)
{
    std::vector<string> vec;
    for (int i = 0; i < ROW_SIZE; i++) {
        string str = GetString(i, 10, VAR_LEN);
        vec.emplace_back(str);
    }

    // Test perf
    std::cout << "Test times: " << ROW_SIZE << std::endl;
    std::cout << "varchar length: " << VAR_LEN << std::endl;

    Timer timer;
    timer.SetStart();

    std::hash<std::string> hashVarChar;
    std::cout << "std::hash()" << std::endl;

    double sum = 0;
    int hashVal;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            hashVal = hashVarChar(vec[i]);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << j + 1 << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "hashVal: " << hashVal << std::endl;

    std::cout << "HashUtil::HashValue()" << std::endl;
    sum = 0;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            hashVal = HashUtil::HashValue((int8_t *)vec[i].c_str(), VAR_LEN);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << j + 1 << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "hashVal: " << hashVal << std::endl;
}

TEST(LongType, LongHashPerf)
{
    std::cout << "Test times: " << ROW_SIZE << std::endl;
    std::cout << "long scope: [" << START << ", " << START + ROW_SIZE << "]" << std::endl;

    Timer timer;
    timer.SetStart();

    std::hash<long> hashLong;
    std::cout << "std::hash()" << std::endl;

    double sum = 0;
    long hashVal;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (long i = START; i < END; i++) {
            hashVal = hashLong(i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << j + 1 << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "hashVal: " << hashVal << std::endl;

    std::cout << "HashUtil::HashValue()" << std::endl;
    sum = 0;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (long i = START; i < END; i++) {
            hashVal = HashUtil::HashValue(i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << j + 1 << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "hashVal: " << hashVal << std::endl;
}
} // namespace omniruntime
