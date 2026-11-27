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
static const int32_t SIZE_OF_LONG = 8;
string GetString(int32_t index, int32_t offset, int32_t width)
{
    std::string str;
    for (int j = 0; j < width; j++) {
        char c = STRING.at((index + offset + j) % STRING.length());
        str.push_back(c);
    }
    return str;
}

int32_t CompareVarchar(BaseVector *leftColumn, int32_t leftColumnPosition, BaseVector *rightColumn,
                       int32_t rightColumnPosition)
{
    auto leftVarCharColumn = static_cast<VarcharVector *>(leftColumn);
    auto rightVarCharColumn = static_cast<VarcharVector *>(rightColumn);

    auto leftValue = leftVarCharColumn->GetValue(leftColumnPosition);
    auto rightValue = rightVarCharColumn->GetValue(rightColumnPosition);
    int32_t result = leftValue.compare(rightValue);
    if (result != 0) {
        return (result > 0) ? 1 : -1;
    } else if (leftValue.size() == rightValue.size()) {
        return 0;
    } else {
        return (leftValue.size() > rightValue.size()) ? 1 : -1;
    }
}

void PrintCompareVarcharResult(BaseVector *vector1, BaseVector *vector2, Timer &timer)
{
    double sum = 0.0f;
    int comp = 0;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            comp = CompareVarchar(vector1, i, vector2, i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "comp: " << comp << std::endl;
}

int64_t ReverseBytes(int64_t var0)
{
    auto result = static_cast<uint64_t>(var0);
    result = ((result & 71777214294589695L) << 8) | ((result >> 8) & 71777214294589695L);
    return (result << 48) | ((result & 4294901760L) << 16) | ((result >> 16) & 4294901760L) | (result >> 48);
}

int64_t LongBytesToLong(int64_t bytes) { return ReverseBytes(bytes) ^ LONG_MIN; }

int32_t CmpByLong(char *var1, char *var2, int32_t compareLength)
{
    auto *left = reinterpret_cast<int64_t *>(var1);
    auto *right = reinterpret_cast<int64_t *>(var2);
    while (compareLength >= 8) {
        int64_t leftVal = *left;
        int64_t rightVal = *right;
        if (leftVal != rightVal) {
            return (LongBytesToLong(leftVal) < LongBytesToLong(rightVal)) ? -1 : 1;
        }
        left++;
        right++;
        compareLength -= 8;
        var1 += 8;
        var2 += SIZE_OF_LONG;
    }
    while (compareLength > 0) {
        int8_t value1 = *(var1);
        int8_t value2 = *(var2);
        if (value1 != value2) {
            return value1 - value2;
        }
        var1++;
        var2++;
        compareLength--;
    }
    return 0;
}

int32_t CompareVarcharByLong(BaseVector *leftColumn, int32_t leftColumnPosition, BaseVector *rightColumn,
                             int32_t rightColumnPosition)
{
    auto leftVarCharColumn = static_cast<VarcharVector *>(leftColumn);
    auto rightVarCharColumn = static_cast<VarcharVector *>(rightColumn);
    auto leftValue = leftVarCharColumn->GetValue(leftColumnPosition);
    auto rightValue = rightVarCharColumn->GetValue(rightColumnPosition);
    int32_t result =
        CmpByLong((char *)leftValue.data(), (char *)rightValue.data(), std::min(leftValue.size(), rightValue.size()));
    if (result != 0) {
        return (result > 0) ? 1 : -1;
    } else if (leftValue.size() == rightValue.size()) {
        return 0;
    } else {
        return (leftValue.size() > rightValue.size()) ? 1 : -1;
    }
}

void PrintCompareVarcharResultByLong(BaseVector *vector1, BaseVector *vector2, Timer &timer)
{
    double sum = 0.0f;
    int comp;
    for (int j = 0; j < ROUNDS; j++) {
        timer.Reset();

        for (int i = 0; i < ROW_SIZE; i++) {
            comp = CompareVarcharByLong(vector1, i, vector2, i);
        }

        timer.CalculateElapse();
        double wallElapsed = timer.GetWallElapse() * 1000;
        double cpuElapsed = timer.GetCpuElapse() * 1000;
        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
        sum += wallElapsed;
    }
    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
    std::cout << "comp: " << comp << std::endl;
}

TEST(varcharType, CompareVarcharPerf)
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

    PrintCompareVarcharResult(vector1, vector2, timer);

    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 20, VAR_LEN);
        std::string_view value(str);
        vector3->SetValue(i, value);
    }
    std::cout << "Compare different varchar:" << std::endl;

    PrintCompareVarcharResult(vector1, vector3, timer);

    delete vector1;
    delete vector2;
    delete vector3;
}

TEST(varcharType, CompareVarcharByLongPerf)
{
    VarcharVector *vector1 = new VarcharVector(ROW_SIZE);
    VarcharVector *vector2 = new VarcharVector(ROW_SIZE);

    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 10, VAR_LEN);
        std::string_view value(str);
        vector1->SetValue(i, value);
        vector2->SetValue(i, value);
    }

    // Test perf
    std::cout << "Test times: " << ROW_SIZE << std::endl;
    std::cout << "varchar length: " << VAR_LEN << std::endl;

    Timer timer;
    timer.SetStart();

    std::cout << "Compare equal varchar" << std::endl;

    PrintCompareVarcharResultByLong(vector1, vector2, timer);

    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
    for (int i = 0; i < ROW_SIZE; i++) {
        std::string str = GetString(i, 20, VAR_LEN);
        std::string_view value(str);
        vector3->SetValue(i, value);
    }

    std::cout << "Compare not equal varchar" << std::endl;

    PrintCompareVarcharResultByLong(vector1, vector3, timer);

    delete vector1;
    delete vector2;
    delete vector3;
}
} // namespace omniruntime
