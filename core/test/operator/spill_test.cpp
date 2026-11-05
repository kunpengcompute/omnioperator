/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * @Description: spill test implementations
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
static uint64_t ScalarBatchSize(int32_t rowCount)
{
    // row count + one null bitmap and values for int/long/short.
    return sizeof(int32_t) + 3 * BitUtil::Nbytes(rowCount) +
        rowCount * (sizeof(int32_t) + sizeof(int64_t) + sizeof(int16_t));
}

TEST(SpillTest, TestWriteRead)
{
    const int32_t dataSize = 5;
    int32_t data1[dataSize] = {3, 5, 7, 9, 1};
    int64_t data2[dataSize] = {8, 4, 0, 2, 6};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1};

    std::vector<DataTypePtr> types = { IntType(), LongType(), ShortType() };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    ChildSpillTracker spillTracker(&(GetRootSpillTracker()), UINT64_MAX);

    SpillWriter writer(sourceTypes, path);
    ASSERT_EQ(writer.WriteVecBatch(vecBatch, ScalarBatchSize(dataSize)), ErrorCode::SUCCESS);
    ASSERT_EQ(writer.Close(), ErrorCode::SUCCESS);
    VectorHelper::FreeVecBatch(vecBatch);

    auto spillFile = writer.GetSpillFileInfo();
    SpillReader reader(sourceTypes, spillFile.filePath, spillFile.fileLength, spillFile.totalRowCount, false);
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

    DataTypes sourceTypes(std::vector<DataTypePtr>({ IntType(), LongType(), ShortType() }));
    VectorBatch *vecBatch = TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<int32_t> sortCols = { 0, 1, 2 };
    std::vector<int32_t> sortAscendings = { 1, 1, 1 };
    std::vector<int32_t> sortNullFirsts = { 0, 0, 0 };
    PagesIndex pagesIndex(sourceTypes);
    pagesIndex.AddVecBatch(vecBatch);
    pagesIndex.Prepare();
    pagesIndex.Sort(sortCols.data(), sortAscendings.data(), sortNullFirsts.data(), 3, 0, dataSize);

    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    InitRootSpillTracker(50);

    std::vector<omniruntime::op::SortOrder> sortOrders;
    for (size_t i = 0; i < sortCols.size(); i++) {
        omniruntime::op::SortOrder sortOrder { sortAscendings[i] == 1, sortNullFirsts[i] == 1 };
        sortOrders.emplace_back(sortOrder);
    }

    Spiller spiller(sourceTypes, sortCols, sortOrders, path, 50);
    auto status = spiller.Spill(&pagesIndex, false, false, nullptr);
    ASSERT_EQ(status, ErrorCode::EXCEED_SPILL_THRESHOLD);
    ASSERT_EQ(spiller.GetSpilledBytes(), 0);
    auto spillTracker = spiller.GetSpillTracker();
    delete spillTracker;
    rmdir(path.c_str());
}

TEST(SpillTest, TestWriteReadWithCompress)
{
    const int32_t dataSize = 5;
    int32_t data1[dataSize] = {3, 5, 7, 9, 1};
    int64_t data2[dataSize] = {8, 4, 0, 2, 6};
    int16_t data3[dataSize] = {5, 4, 3, 2, 1};

    std::vector<DataTypePtr> types = { IntType(), LongType(), ShortType() };
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch = TestUtil::CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);
    ChildSpillTracker spillTracker(&(GetRootSpillTracker()), UINT64_MAX);

    SpillWriter writer(sourceTypes, path, 0, true);
    ASSERT_EQ(writer.WriteVecBatch(vecBatch, ScalarBatchSize(dataSize)), ErrorCode::SUCCESS);
    ASSERT_EQ(writer.Close(), ErrorCode::SUCCESS);
    VectorHelper::FreeVecBatch(vecBatch);

    auto spillFile = writer.GetSpillFileInfo();
    SpillReader reader(sourceTypes, spillFile.filePath, spillFile.fileLength, spillFile.totalRowCount, true);
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

TEST(SpillTest, TestArrayBatchSizeAndReaderReuse)
{
    auto arrayType = std::make_shared<ArrayType>(IntType());
    DataTypes sourceTypes(std::vector<DataTypePtr>({arrayType}));
    std::string path = TestUtil::GenerateSpillPath();
    mkdir(path.c_str(), 0750);

    // Keep the buffer smaller than the two batches combined so the first run
    // is flushed before the second one. This exercises both buffered and
    // direct spill-format paths.
    SpillWriter writer(sourceTypes, path, 64);

    auto firstElements = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *firstElementVector = static_cast<Vector<int32_t> *>(firstElements.get());
    firstElementVector->SetValue(0, 10);
    firstElementVector->SetValue(1, 20);
    firstElementVector->SetValue(2, 30);
    auto *firstArray = new ArrayVector(2, firstElements);
    firstArray->SetOffset(0, 0);
    firstArray->SetOffset(1, 2);
    firstArray->SetOffset(2, 3);
    auto *firstBatch = new VectorBatch(2);
    firstBatch->Append(firstArray);

    auto secondElements = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    static_cast<Vector<int32_t> *>(secondElements.get())->SetValue(0, 40);
    auto *secondArray = new ArrayVector(1, secondElements);
    secondArray->SetOffset(0, 0);
    secondArray->SetOffset(1, 1);
    auto *secondBatch = new VectorBatch(1);
    secondBatch->Append(secondArray);

    // array batch size = row count + nulls + offsets + child nulls + values.
    constexpr uint64_t firstSize = sizeof(int32_t) + BitUtil::Nbytes(2) + 3 * sizeof(int64_t) +
        BitUtil::Nbytes(3) + 3 * sizeof(int32_t);
    constexpr uint64_t secondSize = sizeof(int32_t) + BitUtil::Nbytes(1) + 2 * sizeof(int64_t) +
        BitUtil::Nbytes(1) + sizeof(int32_t);
    ASSERT_EQ(writer.WriteVecBatch(firstBatch, firstSize), ErrorCode::SUCCESS);
    ASSERT_EQ(writer.WriteVecBatch(secondBatch, secondSize), ErrorCode::SUCCESS);
    ASSERT_EQ(writer.Close(), ErrorCode::SUCCESS);
    delete firstBatch;
    delete secondBatch;

    auto spillFile = writer.GetSpillFileInfo();
    SpillReader reader(sourceTypes, spillFile.filePath, spillFile.fileLength, spillFile.totalRowCount, false);
    std::unique_ptr<VectorBatch> output;
    bool isEnd = false;
    ASSERT_EQ(reader.ReadVecBatch(output, isEnd), ErrorCode::SUCCESS);
    ASSERT_FALSE(isEnd);
    ASSERT_EQ(output->GetRowCount(), 2);
    auto *firstOutput = static_cast<ArrayVector *>(output->Get(0));
    ASSERT_EQ(firstOutput->GetOffset(0), 0);
    ASSERT_EQ(firstOutput->GetOffset(1), 2);
    ASSERT_EQ(firstOutput->GetOffset(2), 3);
    auto *firstOutputElements = static_cast<Vector<int32_t> *>(firstOutput->GetElementVector().get());
    ASSERT_EQ(firstOutputElements->GetValue(0), 10);
    ASSERT_EQ(firstOutputElements->GetValue(2), 30);

    ASSERT_EQ(reader.ReadVecBatch(output, isEnd), ErrorCode::SUCCESS);
    ASSERT_FALSE(isEnd);
    ASSERT_EQ(output->GetRowCount(), 1);
    auto *secondOutput = static_cast<ArrayVector *>(output->Get(0));
    ASSERT_EQ(secondOutput->GetOffset(0), 0);
    ASSERT_EQ(secondOutput->GetOffset(1), 1);
    auto *secondOutputElements = static_cast<Vector<int32_t> *>(secondOutput->GetElementVector().get());
    ASSERT_EQ(secondOutputElements->GetValue(0), 40);

    ASSERT_EQ(reader.ReadVecBatch(output, isEnd), ErrorCode::SUCCESS);
    ASSERT_TRUE(isEnd);
    rmdir(path.c_str());
}
}