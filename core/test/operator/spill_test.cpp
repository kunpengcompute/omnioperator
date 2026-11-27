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
#include <vector>
#include <ctime>
#include "gtest/gtest.h"
#include "operator/spill/spiller.h"
#include "operator/spill/spill_merger.h"
#include "vector/vector_helper.h"
#include "type/data_types.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace omniruntime::SpillTest {
TEST(SpillTest, TestWriteRead)
{
    const int32_t dataSize = 5;
    int32_t data1[dataSize] = {3, 5, 7, 9, 1};
    int64_t data2[dataSize] = {8, 4, 0, 2, 6};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1};

    std::vector<DataTypePtr> types = {IntType(), LongType(), ShortType()};
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    ChildSpillTracker spillTracker(&(GetRootSpillTracker()), UINT64_MAX);

    SpillWriter writer(sourceTypes, path);
    writer.WriteVecBatch(vecBatch, 100);
    writer.Close();
    VectorHelper::FreeVecBatch(vecBatch);

    auto spillFile = writer.GetSpillFileInfo();
    SpillReader reader(sourceTypes, spillFile.filePath, spillFile.fileLength, spillFile.totalRowCount);
    std::unique_ptr<VectorBatch> outputVecBatch = nullptr;
    bool isEnd = false;
    reader.ReadVecBatch(outputVecBatch, isEnd);
    VectorHelper::PrintVecBatch(outputVecBatch.get());
    TestUtil::AssertVecBatchEquals(outputVecBatch.get(), sourceTypes.GetSize(), dataSize, data1, data2, data3);
    ASSERT_FALSE(isEnd);

    reader.ReadVecBatch(outputVecBatch, isEnd);
    ASSERT_TRUE(isEnd);
    outputVecBatch = nullptr;

    rmdir(path.c_str());
}

TEST(SpillTest, TestSpillNoneSinceExceededLimit)
{
    const int32_t dataSize = 5;
    int32_t data1[dataSize] = {-7, -3, 1, 5, 9};
    int64_t data2[dataSize] = {6, 8, 4, 0, 2};
    int16_t data3[dataSize] = {-5, -3, 0, 2, 4};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), ShortType()}));
    VectorBatch *vecBatch = TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<int32_t> sortCols = {0, 1, 2};
    std::vector<int32_t> sortAscendings = {1, 1, 1};
    std::vector<int32_t> sortNullFirsts = {0, 0, 0};
    PagesIndex pagesIndex(sourceTypes);
    pagesIndex.AddVecBatch(vecBatch);
    pagesIndex.Prepare();
    pagesIndex.Sort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), 3, 0, dataSize);

    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    InitRootSpillTracker(50);

    std::vector<omniruntime::op::SortOrder> sortOrders;
    for (size_t i = 0; i < sortCols.size(); i++) {
        omniruntime::op::SortOrder sortOrder{sortAscendings[i] == 1, sortNullFirsts[i] == 1};
        sortOrders.emplace_back(sortOrder);
    }

    Spiller spiller(sourceTypes, sortCols, sortOrders, path, 50);
    auto status = spiller.Spill(&pagesIndex, false, false);
    ASSERT_EQ(status, ErrorCode::EXCEED_SPILL_THRESHOLD);
    ASSERT_EQ(spiller.GetSpilledBytes(), 0);
    auto spillTracker = spiller.GetSpillTracker();
    delete spillTracker;
    rmdir(path.c_str());
}
} // namespace omniruntime::SpillTest
