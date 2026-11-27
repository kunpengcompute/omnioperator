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

#include <regex>
#include "gtest/gtest.h"
#include "util/global_log.h"
#include "util/debug.h"

TEST(NativeLog, TestError)
{
    char logBuf[GLOBAL_LOG_BUF_SIZE];
    LogsInfoVargMacro(logBuf, "int is %d; float is %f", 10, 10.10f);
    std::string logString(logBuf);
    std::regex replaceRegex("\\[.*\\]");
    std::string newStr = std::regex_replace(logString, replaceRegex, " ");
    std::string genLog = " :int is 10; float is 10.100000";
    Log(logString, LogType::LOG_ERROR);
    ASSERT_EQ(genLog == newStr, true);
}

TEST(NativeLog, TestDebug)
{
    char logBuf[GLOBAL_LOG_BUF_SIZE];
    LogsInfoVargMacro(logBuf, "arg %s", "test string");
    std::string debugLog(logBuf);
    std::regex replaceRegex("\\[.*\\]");
    std::string newStr = std::regex_replace(debugLog, replaceRegex, " ");
    std::string genLog = " :arg test string";
    Log(debugLog, LogType::LOG_DEBUG);
    ASSERT_EQ(genLog == newStr, true);
}

TEST(NativeLog, TestInfo)
{
    std::ostringstream stringStream;
    LogsInfoMacro(stringStream, "Type info: Info");
    std::string infoLog = stringStream.str();
    std::regex replaceRegex("\\[.*\\]");
    std::string newStr = std::regex_replace(infoLog, replaceRegex, " ");
    std::string genLog = " Type info: Info";
    Log(infoLog, LogType::LOG_INFO);
    ASSERT_EQ(genLog == newStr, true);
    stringStream.str("");
}

TEST(NativeLog, TestWarn)
{
    std::ostringstream stringStream;
    LogsInfoMacro(stringStream, "Type info: WARN");
    std::string warnLog = stringStream.str();
    std::regex replaceRegex("\\[.*\\]");
    std::string newStr = std::regex_replace(warnLog, replaceRegex, " ");
    std::string genLog = " Type info: WARN";
    Log(warnLog, LogType::LOG_WARN);
    ASSERT_EQ(genLog == newStr, true);
    stringStream.str("");
}

TEST(NativeLog, AllLog)
{
    LogDebug("denig log %d,%f,%s\n", 123, 123.0f, "hello");
    LogWarn("warn log %d,%f,%s\n", 123, 123.0f, "hello");
    LogError("error log %d,%f,%s\n", 123, 123.0f, "hello");
    LogInfo("info log %d,%f,%s\n", 123, 123.0f, "hello");
}
