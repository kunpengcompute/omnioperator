/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: ...
 */

#include <memory>
#include <unordered_map>
#include <vector>
#include "gtest/gtest.h"
#include "vector/vector_helper.h"
#include "operator/pages_index.h"
#include "operator/join/sortmergejoin/dynamic_pages_index.h"
#include "operator/join/sortmergejoin/sort_merge_join_resultBuilder.h"
#include "operator/join/sortmergejoin/sort_merge_join.h"
#include "util/test_util.h"
#include "expression/jsonparser/jsonparser.h"
#include "type/data_type.h"
#include "util/config/QueryConfig.h"
#include "vector/row_vector.h"

using namespace omniruntime::op;
using namespace std;
using namespace TestUtil;

namespace SortMergeJoinTest {
TEST(NativeSortMergeJoinTest, TestMultiAddVecBatches)
{
    std::vector<DataTypePtr> types = { IntType(), DoubleType(), ShortType() };
    DataTypes sourceTypes(types);
    int32_t computeCols[] = {0};
    auto *dynamicPagesIndex = new DynamicPagesIndex(sourceTypes, computeCols, 1);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), 0);

    // construct data
    const int32_t dataSize1 = 6;
    // table1
    int32_t data1[dataSize1] = {0, 1, 2, 0, 1, 2};
    double data2[dataSize1] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize1] = {6, 5, 4, 3, 2, 1};
    // table2
    const int32_t dataSize2 = 7;
    int32_t data4[dataSize2] = {10, 11, 12, 10, 11, 12, 15};
    double data5[dataSize2] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1, 11.3};
    int16_t data6[dataSize2] = {16, 15, 14, 13, 12, 11, 10};

    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize1, data1, data2, data3);
    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, dataSize2, data4, data5, data6);

    dynamicPagesIndex->AddVecBatch(vecBatch1);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), dataSize1);
    ASSERT_EQ(dynamicPagesIndex->IsDataFinish(), false);

    dynamicPagesIndex->AddVecBatch(vecBatch2);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), dataSize1 + dataSize2);
    ASSERT_EQ(dynamicPagesIndex->IsDataFinish(), false);

    VectorBatch *emptyVectorBatch = CreateEmptyVectorBatch(sourceTypes);
    dynamicPagesIndex->AddVecBatch(emptyVectorBatch);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), dataSize1 + dataSize2);
    ASSERT_EQ(dynamicPagesIndex->IsDataFinish(), true);

    dynamicPagesIndex->FreeAllRemainingVecBatch();
    delete dynamicPagesIndex;
}

TEST(NativeSortMergeJoinTest, TestDataValue)
{
    std::vector<DataTypePtr> types = { IntType(), DoubleType() };
    DataTypes sourceTypes(types);
    int32_t computeCols[] = {0};
    auto *dynamicPagesIndex = new DynamicPagesIndex(sourceTypes, computeCols, 1);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), 0);

    // construct data
    const int32_t dataSize1 = 6;
    // table1
    int32_t data1[dataSize1] = {0, 1, 2, 3, 4, 5};
    double data2[dataSize1] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    // table2
    const int32_t dataSize2 = 7;
    int32_t data3[dataSize2] = {6, 7, 8, 9, 10, 11, 12};
    double data4[dataSize2] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1, 11.3};

    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize1, data1, data2);
    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, dataSize2, data3, data4);
    vecBatch2->Get(0)->SetNull(5);

    dynamicPagesIndex->AddVecBatch(vecBatch1);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), dataSize1);
    ASSERT_EQ(dynamicPagesIndex->IsDataFinish(), false);
    // fetch logical row of 6
    int64_t row6ValueAddress = dynamicPagesIndex->GetValueAddresses(5);
    int32_t row6vecBatchIndex = DecodeSliceIndex(row6ValueAddress);
    int32_t row6IndexInVecBatch = DecodePosition(row6ValueAddress);
    ASSERT_EQ(row6vecBatchIndex, 0);
    ASSERT_EQ(row6IndexInVecBatch, 5);
    auto *row6DdoubleVector = reinterpret_cast<Vector<double> *>(dynamicPagesIndex->GetColumn(row6vecBatchIndex, 1));
    ASSERT_EQ(row6DdoubleVector->GetValue(5), 1.1);

    dynamicPagesIndex->AddVecBatch(vecBatch2);
    ASSERT_EQ(dynamicPagesIndex->GetPositionCount(), dataSize1 + dataSize2);
    ASSERT_EQ(dynamicPagesIndex->IsDataFinish(), false);

    // fetch logical row of 8
    int64_t row8ValueAddress = dynamicPagesIndex->GetValueAddresses(7);
    int32_t row8vecBatchIndex = DecodeSliceIndex(row8ValueAddress);
    int32_t row8IndexInVecBatch = DecodePosition(row8ValueAddress);
    ASSERT_EQ(row8vecBatchIndex, 1);
    ASSERT_EQ(row8IndexInVecBatch, 1);
    auto *row8DoubleVector = reinterpret_cast<Vector<double> *>(dynamicPagesIndex->GetColumn(row8vecBatchIndex, 1));
    ASSERT_EQ(row8DoubleVector->GetValue(1), 15.5);

    // fetch logical row of 12
    int64_t row12ValueAddress = dynamicPagesIndex->GetValueAddresses(11);
    int32_t row12vecBatchIndex = DecodeSliceIndex(row12ValueAddress);
    int32_t row12IndexInVecBatch = DecodePosition(row12ValueAddress);
    ASSERT_EQ(row12vecBatchIndex, 1);
    ASSERT_EQ(row12IndexInVecBatch, 5);
    auto *row12IntVector = reinterpret_cast<Vector<int32_t> *>(dynamicPagesIndex->GetColumn(row12vecBatchIndex, 0));
    ASSERT_EQ(row12IntVector->IsNull(4), false);
    ASSERT_EQ(row12IntVector->IsNull(5), true);
    ASSERT_EQ(row12IntVector->IsNull(6), false);

    dynamicPagesIndex->FreeAllRemainingVecBatch();
    delete dynamicPagesIndex;
}

TEST(NativeSortMergeJoinTest, TestSmjOneTimeEqualCondition)
{
    // select t1.b, t2.c from t1, t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank = "";
    auto *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);

    std::vector<DataTypePtr> streamTypesVector = { IntType(), LongType() };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols;
    streamedKeysCols.emplace_back(0);
    std::vector<int32_t> streamedOutputCols;
    streamedOutputCols.emplace_back(1);
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { DoubleType(), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols;
    bufferedKeysCols.emplace_back(1);
    std::vector<int32_t> bufferedOutputCols;
    bufferedOutputCols.emplace_back(0);
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add buffered table data
    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // need add buffered table data
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatchEof);
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    VectorBatch *result;
    smjOp->GetOutput(&result);

    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatchEof1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));

    // check the join result
    int32_t index = 0;
    ASSERT_EQ(result->GetVectorCount(), 2);
    for (int32_t j = 0; j < result->GetRowCount(); j++) {
        long longValue = (reinterpret_cast<Vector<int64_t> *>(result->Get(0)))->GetValue(j);
        ASSERT_EQ(longValue, streamedTblDataCol2[index]);

        double doubleValue = (reinterpret_cast<Vector<double> *>(result->Get(1)))->GetValue(j);
        ASSERT_EQ(doubleValue, bufferedTblDataCol1[index]);
        index++;
    }
    VectorHelper::FreeVecBatch(result);

    omniruntime::op::Operator::DeleteOperator(smjOp);
}

void ExpectVectorEqual(std::vector<int64_t> expected, std::vector<int64_t> actual)
{
    EXPECT_EQ(expected.size(), actual.size());
    for (uint32_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], actual[i]);
    }
}

TEST(NativeSortMergeJoinTest, TestJoinScanner1)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 5};
    int64_t bufferData0[] = {1, 5, 6, 7};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 0, 3 });
    std::vector<int64_t> expectedBufferedAddr({ 0, 1 });
    std::vector<int8_t> isMatchPre;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    isMatchPre.reserve(8);
    streamedAddr.reserve(8);
    bufferedAddr.reserve(8);
    isSameBufferedKeyMatched.reserve(8);
    auto *oldIsMatchPreData = isMatchPre.data();
    auto *oldStreamedAddrData = streamedAddr.data();
    auto *oldBufferedAddrData = bufferedAddr.data();
    scan->GetMatchedValueAddresses(isMatchPre, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // Empty destinations take ownership of the scanner buffers instead of copying
    // into their reserved storage. Their old capacity is returned to the scanner.
    EXPECT_NE(isMatchPre.data(), oldIsMatchPreData);
    EXPECT_NE(streamedAddr.data(), oldStreamedAddrData);
    EXPECT_NE(bufferedAddr.data(), oldBufferedAddrData);
    EXPECT_EQ(isSameBufferedKeyMatched.capacity(), 0);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    streamedPageIndex->FreeAllRemainingVecBatch();
    bufferedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScannerStreamedPinIncludesAddressZero)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes bufferedTypes(std::vector<DataTypePtr>{IntType()});
    int32_t keyColumns[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, keyColumns, 1);
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, keyColumns, 1);
    int32_t key[] = {7};
    auto *streamedBatch = new VectorBatch(1);
    streamedBatch->Append(CreateVector<int32_t>(1, key));
    streamedPageIndex->AddVecBatch(streamedBatch);
    auto *bufferedBatch = new VectorBatch(1);
    bufferedBatch->Append(CreateVector<int32_t>(1, key));
    bufferedPageIndex->AddVecBatch(bufferedBatch);
    streamedPageIndex->AddVecBatch(CreateEmptyVectorBatch(streamedTypes));
    bufferedPageIndex->AddVecBatch(CreateEmptyVectorBatch(bufferedTypes));

    auto *scanner = new SortMergeJoinScanner(streamedTypes, keyColumns, 1, streamedPageIndex,
        bufferedTypes, keyColumns, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    EXPECT_EQ(scanner->GetStreamedPinBatchId(), INT32_MAX);
    scanner->FindNextJoinRows();
    // Synthetic address 0 is batch 0 / row 0, not the unset sentinel. The scanner
    // must publish batch 0 so the result builder cannot reclaim it prematurely.
    EXPECT_EQ(scanner->GetStreamedPinBatchId(), 0);

    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddresses;
    std::vector<int64_t> bufferedAddresses;
    std::vector<int8_t> sameBufferedKeyMatched;
    scanner->GetMatchedValueAddresses(isPreMatched, streamedAddresses, bufferedAddresses,
        sameBufferedKeyMatched);
    EXPECT_EQ(streamedAddresses.size(), 1);
    EXPECT_EQ(bufferedAddresses.size(), 1);
    if (!streamedAddresses.empty()) {
        EXPECT_EQ(streamedAddresses[0], 0);
    }
    if (!bufferedAddresses.empty()) {
        EXPECT_EQ(bufferedAddresses[0], 0);
    }

    delete scanner;
    streamedPageIndex->FreeAllRemainingVecBatch();
    bufferedPageIndex->FreeAllRemainingVecBatch();
    delete streamedPageIndex;
    delete bufferedPageIndex;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner2)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 5, 6, 7};
    int64_t bufferData0[] = {1, 2, 3, 5};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    auto ret = scan->FindNextJoinRows();
    EXPECT_NE(ret, -1);

    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    ret = scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 0, 1 });
    std::vector<int64_t> expectedBufferedAddr({ 0, 3 });
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    streamedPageIndex->FreeAllRemainingVecBatch();
    bufferedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner3)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 5};
    int64_t bufferData0[] = {0, 3, 5, 6};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 2, 3 });
    std::vector<int64_t> expectedBufferedAddr({ 1, 2 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner4)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {0, 3, 5, 6};
    int64_t bufferData0[] = {1, 2, 3, 5};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 2, 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner5)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 5};
    int64_t bufferData0[] = {0, 5, 6, 7};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 3 });
    std::vector<int64_t> expectedBufferedAddr({ 1 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner6)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {0, 5, 6, 7};
    int64_t bufferData0[] = {1, 2, 3, 5};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1 });
    std::vector<int64_t> expectedBufferedAddr({ 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestRepeatBufferedTableKeys1)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {-1, 0, 2, 7};
    int64_t bufferData0[] = {0, 1, 2, 2};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1, 2, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 0, 2, 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestRepeatBufferedTableKeys2)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {-1, 0, 2, 7};
    int64_t bufferData0[] = {1, 2, 2, 5};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 2, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 1, 2 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestRepeatStreamedTableKeys1)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {0, 1, 2, 2};
    int64_t bufferData0[] = {-1, 0, 2, 7};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 0, 2, 3 });
    std::vector<int64_t> expectedBufferedAddr({ 1, 2, 2 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestRepeatStreamedTableKeys2)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 2, 5};
    int64_t bufferData0[] = {-1, 0, 2, 7};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 2, 2 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestMultipleTableKeys)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t streamedCols[] = {1, 2};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 2);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t bufferedCols[] = {0, 1};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 2);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 4};
    int64_t streamData1[] = {-1, 0, 2, 7};
    int64_t streamData2[] = {-1, 0, 2, 7};
    int64_t streamData3[] = {11, 22, 33, 44};

    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData1));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData2));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData3));
    streamedPageIndex->AddVecBatch(streamedVecBatch);
    int64_t bufferData0[] = {0, 1, 2, 2};
    int64_t bufferData1[] = {0, 1, 1, 2};
    int64_t bufferData2[] = {9, 8, 7, 6};
    int64_t bufferData3[] = {111, 11, 1, 0};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData2));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData3));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedTypes, streamedCols, streamedKeysTypes.GetSize(), streamedPageIndex,
        bufferedTypes, bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 0, 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestNullKeys)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    int32_t streamedCols[] = {1, 2};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 2);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType() }));
    int32_t bufferedCols[] = {0, 1};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 2);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 4};
    int64_t streamData1[] = {-1, 0, 2, 7};
    int64_t streamData2[] = {-1, 0, 2, 7};
    int64_t streamData3[] = {11, 22, 33, 44};

    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData1));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData2));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData3));
    streamedPageIndex->AddVecBatch(streamedVecBatch);
    int64_t bufferData0[] = {0, 1, 2, 2};
    int64_t bufferData1[] = {0, 1, 2, 2};
    int64_t bufferData2[] = {9, 8, 7, 6};
    int64_t bufferData3[] = {111, 11, 1, 0};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    auto bufferVector0 = CreateVector<int64_t>(dataSize, bufferData0);
    auto bufferVector1 = CreateVector<int64_t>(dataSize, bufferData1);
    bufferVector1->SetNull(2);
    bufferedVecBatch->Append(bufferVector0);
    bufferedVecBatch->Append(bufferVector1);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData2));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData3));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedTypes, streamedCols, 2, streamedPageIndex, bufferedTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 1, 2 });
    std::vector<int64_t> expectedBufferedAddr({ 0, 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestDateTypes)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType(), VarcharType(5), BooleanType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(5), BooleanType() }));
    int32_t streamedCols[] = {1, 2, 3};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 3);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(5), BooleanType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(5), BooleanType() }));
    int32_t bufferedCols[] = {0, 1, 2};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 3);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 4};
    double streamData1[] = {-1.2, 0.2, 2.2, 7.2};
    std::string streamData2[] = {"ab", "cd", "ef", "gh"};
    bool streamData3[] = {false, false, true, true};

    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<double>(dataSize, streamData1));
    streamedVecBatch->Append(CreateVarcharVector(streamData2, dataSize));
    streamedVecBatch->Append(CreateVector<bool>(dataSize, streamData3));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    double bufferData0[] = {-1.3, 0.2, 2.2, 7.2};
    std::string bufferData1[] = {"ab", "di", "ef", "gh"};
    bool bufferData2[] = {false, false, false, true};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<double>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVarcharVector(bufferData1, dataSize));
    bufferedVecBatch->Append(CreateVector<bool>(dataSize, bufferData2));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedTypes, streamedCols, streamedKeysTypes.GetSize(), streamedPageIndex,
        bufferedTypes, bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int64_t> expectedStreamedAddr({ 3 });
    std::vector<int64_t> expectedBufferedAddr({ 3 });
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestMultipleVecBatch)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    long streamData0[] = {1, 2, 3, 4, 4, 5, 6, 7, 10, 13, 13, 15, 18, 26};
    int streamedSize0 = 14;
    auto *streamedVecBatch = new VectorBatch(streamedSize0);
    streamedVecBatch->Append(CreateVector<int64_t>(streamedSize0, streamData0));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    long bufferData0[] = {2, 4, 4, 4, 4, 5, 6, 10};
    int bufferSize0 = 8;
    auto *bufferedVecBatch = new VectorBatch(bufferSize0);
    bufferedVecBatch->Append(CreateVector<int64_t>(bufferSize0, bufferData0));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);
    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, streamedKeysTypes.GetSize(),
        streamedPageIndex, bufferedKeysTypes, bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    scan->FindNextJoinRows();
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    long bufferData1[] = {10, 13, 13, 17, 17, 18, 18, 19};
    int bufferSize1 = 8;
    auto *bufferedVecBatch1 = new VectorBatch(bufferSize1);
    bufferedVecBatch1->Append(CreateVector<int64_t>(bufferSize1, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch1);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    long bufferData2[] = {20, 21, 23, 24, 25, 25, 25, 25};
    int bufferSize2 = 8;
    auto *bufferedVecBatch2 = new VectorBatch(bufferSize2);
    bufferedVecBatch2->Append(CreateVector<int64_t>(bufferSize2, bufferData2));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch2);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    long bufferData3[] = {30, 31, 42, 43, 44, 45, 46, 47};
    int bufferSize3 = 8;
    auto *bufferedVecBatch3 = new VectorBatch(bufferSize3);
    bufferedVecBatch3->Append(CreateVector<int64_t>(bufferSize3, bufferData3));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch3);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    long streamData1[] = {28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};
    int streamedSize1 = 14;
    auto *streamedVecBatch1 = new VectorBatch(streamedSize1);
    streamedVecBatch1->Append(CreateVector<int64_t>(streamedSize1, streamData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch1);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    long streamData2[] = {43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 47};
    int streamedSize2 = 14;
    auto *streamedVecBatch2 = new VectorBatch(streamedSize2);
    streamedVecBatch2->Append(CreateVector<int64_t>(streamedSize2, streamData2));
    streamedPageIndex->AddVecBatch(streamedVecBatch2);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    auto *eofStreamedVecBatch = CreateEmptyVectorBatch(streamedTypes);
    streamedPageIndex->AddVecBatch(eofStreamedVecBatch);
    scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    std::vector<int64_t> expectedStreamedAddr(
        { 1,          3,          3,          3,          3,          4,          4,          4,          4,
        5,          6,          8,          8,          9,          9,          10,         10,         12,
        12,         4294967298, 4294967299, 8589934592, 8589934593, 8589934594, 8589934595, 8589934596, 8589934597,
        8589934598, 8589934599, 8589934600, 8589934601, 8589934602, 8589934603, 8589934604, 8589934605 });
    std::vector<int64_t> expectedBufferedAddr(
        { 0,           1,           2,           3,           4,           1,           2,
        3,           4,           5,           6,           7,           4294967296,  4294967297,
        4294967298,  4294967297,  4294967298,  4294967301,  4294967302,  12884901888, 12884901889,
        12884901891, 12884901891, 12884901891, 12884901891, 12884901891, 12884901891, 12884901891,
        12884901891, 12884901891, 12884901891, 12884901891, 12884901891, 12884901891, 12884901895 });
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestReturnCode)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3, 4};
    int64_t bufferData0[] = {1, 2, 3, 4};
    int64_t streamedData1[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t streamData2[] = {4, 6, 7};
    auto *streamedVecBatch2 = new VectorBatch(dataSize - 1);
    streamedVecBatch2->Append(CreateVector<int64_t>(dataSize - 1, streamData2));
    streamedVecBatch2->Append(CreateVector<int64_t>(dataSize - 1, streamedData1));

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);

    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    auto ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 0);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 1);
    ASSERT_EQ(DecodeJoinResult(ret), 1);
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 1);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 0);
    ASSERT_EQ(DecodeJoinResult(ret), 1);
    streamedPageIndex->AddVecBatch(streamedVecBatch2);
    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 0);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 1);
    ASSERT_EQ(DecodeJoinResult(ret), 1);

    auto *eofBufferedVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofBufferedVecBatch);
    // add will skip since the process is finished
    VectorHelper::FreeVecBatch(eofBufferedVecBatch);

    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 1);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 2);
    ASSERT_EQ(DecodeJoinResult(ret), 0);

    auto *eofStreamedVecBatch = CreateEmptyVectorBatch(streamedTypes);
    streamedPageIndex->AddVecBatch(eofStreamedVecBatch);
    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 2);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 2);
    ASSERT_EQ(DecodeJoinResult(ret), 1);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestReturnCode2)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 4;
    int64_t streamData0[] = {1, 2, 3};
    int64_t bufferData0[] = {1, 2, 3, 4};
    int64_t streamedData1[] = {111, 11, 1};
    auto *streamedVecBatch = new VectorBatch(dataSize - 1);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize - 1, streamData0));
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize - 1, streamedData1));
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    int64_t bufferData1[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData1));
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);

    int64_t bufferData2[] = {11, 22, 33, 44};
    auto *bufferedVecBatch2 = new VectorBatch(dataSize);
    bufferedVecBatch2->Append(CreateVector<int64_t>(dataSize, bufferData0));
    bufferedVecBatch2->Append(CreateVector<int64_t>(dataSize, bufferData2));

    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    auto ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 1);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 0);
    ASSERT_EQ(DecodeJoinResult(ret), 1);
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(streamedTypes);
    streamedPageIndex->AddVecBatch(eofVecBatch);
    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 2);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 1);
    ASSERT_EQ(DecodeJoinResult(ret), 0);
    bufferedPageIndex->AddVecBatch(bufferedVecBatch2);
    ret = scan->FindNextJoinRows();
    ASSERT_EQ(DecodeStreamedTblResult(ret), 2);
    ASSERT_EQ(DecodeBufferedTblResult(ret), 2);
    ASSERT_EQ(DecodeJoinResult(ret), 1);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

TEST(NativeSortMergeJoinTest, TestJoinScanner7)
{
    DataTypes streamedTypes(std::vector<DataTypePtr>({ LongType() }));
    DataTypes streamedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t streamedCols[] = {0};
    auto *streamedPageIndex = new DynamicPagesIndex(streamedTypes, streamedCols, 1);
    DataTypes bufferedTypes(std::vector<DataTypePtr>({ LongType() }));
    DataTypes bufferedKeysTypes(std::vector<DataTypePtr>({ LongType() }));
    int32_t bufferedCols[] = {0};
    auto *bufferedPageIndex = new DynamicPagesIndex(bufferedTypes, bufferedCols, 1);

    const int32_t dataSize = 6;
    // stream data0
    int64_t streamData0[] = {0, 1, 2, 3, 4, 5};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(CreateVector<int64_t>(dataSize, streamData0));

    // buffer data0
    int64_t bufferData0[] = {0, 1, 2, 3, 4, 5};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(CreateVector<int64_t>(dataSize, bufferData0));

    // buffer data1
    int64_t bufferData1[] = {5, 5, 5, 5, 5, 5};
    auto *bufferedVecBatch1 = new VectorBatch(dataSize);
    bufferedVecBatch1->Append(CreateVector<int64_t>(dataSize, bufferData1));

    // stream data1
    int64_t streamData1[] = {5, 5, 5, 5, 5, 5};
    auto *streamedVecBatch1 = new VectorBatch(dataSize);
    streamedVecBatch1->Append(CreateVector<int64_t>(dataSize, streamData1));

    // stream data2
    int64_t streamData2[] = {5, 6, 6, 7, 8, 9};
    auto *streamedVecBatch2 = new VectorBatch(dataSize);
    streamedVecBatch2->Append(CreateVector<int64_t>(dataSize, streamData2));

    // buffer data2
    int64_t bufferData2[] = {5, 6, 7, 7, 7, 7};
    auto *bufferedVecBatch2 = new VectorBatch(dataSize);
    bufferedVecBatch2->Append(CreateVector<int64_t>(dataSize, bufferData2));

    // add stream0
    streamedPageIndex->AddVecBatch(streamedVecBatch);

    // add buffer0
    bufferedPageIndex->AddVecBatch(bufferedVecBatch);

    auto *scan = new SortMergeJoinScanner(streamedKeysTypes, streamedCols, 1, streamedPageIndex, bufferedKeysTypes,
        bufferedCols, bufferedPageIndex, JoinType::OMNI_JOIN_TYPE_INNER, false);
    auto ret = scan->FindNextJoinRows();
    EXPECT_NE(ret, -1);

    // get output
    std::vector<int8_t> isPreMatched;
    std::vector<int64_t> streamedAddr;
    std::vector<int64_t> bufferedAddr;
    std::vector<int8_t> isSameBufferedKeyMatched;
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add buffer1
    bufferedPageIndex->AddVecBatch(bufferedVecBatch1);
    ret = scan->FindNextJoinRows();
    // get output
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add buffer2
    bufferedPageIndex->AddVecBatch(bufferedVecBatch2);
    ret = scan->FindNextJoinRows();
    // get output
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add stream1
    streamedPageIndex->AddVecBatch(streamedVecBatch1);
    ret = scan->FindNextJoinRows();

    // get output
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add stream2
    streamedPageIndex->AddVecBatch(streamedVecBatch2);
    ret = scan->FindNextJoinRows();
    // get output
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add buffer eof
    VectorBatch *eofVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofVecBatch);
    ret = scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add buffer eof
    auto *eofBufferedVecBatch = CreateEmptyVectorBatch(bufferedTypes);
    bufferedPageIndex->AddVecBatch(eofBufferedVecBatch);
    // add will skip since the process is finished
    VectorHelper::FreeVecBatch(eofBufferedVecBatch);

    ret = scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    // add stream eof
    auto *eofStreamedVecBatch = CreateEmptyVectorBatch(streamedTypes);
    streamedPageIndex->AddVecBatch(eofStreamedVecBatch);
    ret = scan->FindNextJoinRows();
    scan->GetMatchedValueAddresses(isPreMatched, streamedAddr, bufferedAddr, isSameBufferedKeyMatched);

    std::vector<int64_t> expectedStreamedAddr(
        { 0,          1,          2,          3,          4,          5,          5,          5,          5,
        5,          5,          5,          5,          4294967296, 4294967296, 4294967296, 4294967296, 4294967296,
        4294967296, 4294967296, 4294967296, 4294967297, 4294967297, 4294967297, 4294967297, 4294967297, 4294967297,
        4294967297, 4294967297, 4294967298, 4294967298, 4294967298, 4294967298, 4294967298, 4294967298, 4294967298,
        4294967298, 4294967299, 4294967299, 4294967299, 4294967299, 4294967299, 4294967299, 4294967299, 4294967299,
        4294967300, 4294967300, 4294967300, 4294967300, 4294967300, 4294967300, 4294967300, 4294967300, 4294967301,
        4294967301, 4294967301, 4294967301, 4294967301, 4294967301, 4294967301, 4294967301, 8589934592, 8589934592,
        8589934592, 8589934592, 8589934592, 8589934592, 8589934592, 8589934592, 8589934593, 8589934594, 8589934595,
        8589934595, 8589934595, 8589934595 });
    std::vector<int64_t> expectedBufferedAddr(
        { 0,          1,          2,          3,          4,          5,          4294967296, 4294967297, 4294967298,
        4294967299, 4294967300, 4294967301, 8589934592, 5,          4294967296, 4294967297, 4294967298, 4294967299,
        4294967300, 4294967301, 8589934592, 5,          4294967296, 4294967297, 4294967298, 4294967299, 4294967300,
        4294967301, 8589934592, 5,          4294967296, 4294967297, 4294967298, 4294967299, 4294967300, 4294967301,
        8589934592, 5,          4294967296, 4294967297, 4294967298, 4294967299, 4294967300, 4294967301, 8589934592,
        5,          4294967296, 4294967297, 4294967298, 4294967299, 4294967300, 4294967301, 8589934592, 5,
        4294967296, 4294967297, 4294967298, 4294967299, 4294967300, 4294967301, 8589934592, 5,          4294967296,
        4294967297, 4294967298, 4294967299, 4294967300, 4294967301, 8589934592, 8589934593, 8589934593, 8589934594,
        8589934595, 8589934596, 8589934597 });
    ExpectVectorEqual(expectedStreamedAddr, streamedAddr);
    ExpectVectorEqual(expectedBufferedAddr, bufferedAddr);

    bufferedPageIndex->FreeAllRemainingVecBatch();
    streamedPageIndex->FreeAllRemainingVecBatch();
    delete bufferedPageIndex;
    delete streamedPageIndex;
    delete scan;
}

namespace {
using SmjTestVarcharVector = vec::Vector<vec::LargeStringContainer<std::string_view>>;

class SmjDestructionCountingIntVector : public Vector<int32_t> {
public:
    SmjDestructionCountingIntVector(int32_t value, int32_t &destructionCount)
        : Vector<int32_t>(1), destructionCount(destructionCount)
    {
        SetValue(0, value);
    }

    ~SmjDestructionCountingIntVector() override
    {
        ++destructionCount;
    }

private:
    int32_t &destructionCount;
};

VectorBatch *CreateSmjCountingBatch(int32_t value, int32_t &destructionCount)
{
    auto *batch = new VectorBatch(1);
    batch->Append(new SmjDestructionCountingIntVector(value, destructionCount));
    return batch;
}

SmjTestVarcharVector *CreateSmjTestVarcharVector(const std::vector<std::string> &values,
    const std::vector<uint8_t> &nulls = {})
{
    auto *vector = new SmjTestVarcharVector(static_cast<int32_t>(values.size()));
    for (int32_t row = 0; row < static_cast<int32_t>(values.size()); ++row) {
        if (!nulls.empty() && nulls[row] != 0) {
            vector->SetNull(row);
        } else {
            vector->SetValue(row, std::string_view(values[row]));
        }
    }
    return vector;
}

void AppendSmjTestAddresses(JoinResultBuilder *builder, const std::vector<int32_t> &leftBatchIds,
    const std::vector<int32_t> &leftRowIds, const std::vector<int32_t> &rightBatchIds,
    const std::vector<int32_t> &rightRowIds)
{
    ASSERT_EQ(leftBatchIds.size(), leftRowIds.size());
    ASSERT_EQ(leftRowIds.size(), rightBatchIds.size());
    ASSERT_EQ(rightBatchIds.size(), rightRowIds.size());

    auto &preMatched = builder->GetPreKeyMatched();
    auto &leftAddresses = builder->GetStreamedTableValueAddresses();
    auto &rightAddresses = builder->GetBufferedTableValueAddresses();
    for (size_t row = 0; row < leftRowIds.size(); ++row) {
        preMatched.push_back(0);
        leftAddresses.push_back(static_cast<int64_t>(EncodeSyntheticAddress(leftBatchIds[row], leftRowIds[row])));
        rightAddresses.push_back(
            static_cast<int64_t>(EncodeSyntheticAddress(rightBatchIds[row], rightRowIds[row])));
    }
}

void AssertSmjTestVarcharValues(vec::BaseVector *baseVector, const std::vector<std::string> &expected,
    const std::vector<uint8_t> &expectedNulls)
{
    ASSERT_EQ(expected.size(), expectedNulls.size());
    ASSERT_EQ(baseVector->GetSize(), static_cast<int32_t>(expected.size()));
    auto *vector = static_cast<SmjTestVarcharVector *>(baseVector);
    for (int32_t row = 0; row < static_cast<int32_t>(expected.size()); ++row) {
        ASSERT_EQ(vector->IsNull(row), expectedNulls[row] != 0) << "row " << row;
        const std::string_view actual = vector->GetValue(row);
        if (expectedNulls[row] != 0) {
            ASSERT_TRUE(actual.empty()) << "row " << row;
        } else {
            ASSERT_EQ(actual, std::string_view(expected[row])) << "row " << row;
        }
    }
}
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilder)
{
    std::vector<DataTypePtr> leftTypes = { IntType(), DoubleType() };
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = { IntType(), DoubleType(), VarcharType(3) };
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    const int32_t dataSize = 6;
    int32_t leftData1_1[dataSize] = {0, 1, 2, 3, 4, 5};
    auto leftVector1 = CreateVector<int32_t>(dataSize, leftData1_1);
    // build dictionary vector for test
    double leftData1_2[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};
    auto dataVector1 = CreateVector<double>(dataSize, leftData1_2);

    auto *leftVecBatch1 = new VectorBatch(dataSize);
    leftVecBatch1->Append(leftVector1);
    leftVecBatch1->Append(dataVector1);

    int32_t leftData21[dataSize] = {6, 7, 8, 9, 10, 11};
    auto leftVector2 = CreateVector<int32_t>(dataSize, leftData21);
    double leftData22[dataSize] = {6.6, 7.7, 8.8, 9.9, 10.1, 11.1};
    auto dataVector2 = CreateVector<double>(dataSize, leftData22);

    auto *leftVecBatch2 = new VectorBatch(dataSize);
    leftVecBatch2->Append(leftVector2);
    leftVecBatch2->Append(dataVector2);
    leftPagesIndex->AddVecBatch(leftVecBatch1);
    leftPagesIndex->AddVecBatch(leftVecBatch2);

    std::vector<DataTypePtr> leftTableOutputTypes { IntType(), DoubleType() };
    int32_t leftTableOutputCols[2] = {0, 1};
    int32_t leftTableOutputColsCount = 2;

    int32_t rightData11[dataSize] = {5, 4, 3, 2, 1, 0};
    double rightData12[dataSize] = {5.5, 4.4, 3.3, 2.2, 1.1, 0.0};
    std::string rightData13[dataSize] = {"555", "444", "33", "22", "1", "0"};
    int32_t rightData21[dataSize] = {11, 10, 9, 8, 7, 6};
    double rightData22[dataSize] = {11.1, 10.1, 9.9, 8.8, 7.7, 6.6};
    std::string rightData23[dataSize] = {"111", "101", "99", "88", "7", "6"};

    VectorBatch *rightVecBatch1 = CreateVectorBatch(rightSourceTypes, dataSize, rightData11, rightData12, rightData13);
    VectorBatch *rightVecBatch2 = CreateVectorBatch(rightSourceTypes, dataSize, rightData21, rightData22, rightData23);
    rightPagesIndex->AddVecBatch(rightVecBatch1);
    rightPagesIndex->AddVecBatch(rightVecBatch2);

    std::vector<DataTypePtr> rightTableOutputTypes { DoubleType(), VarcharType(3) };
    int32_t rightTableOutputCols[2] = {1, 2};
    int32_t rightTableOutputColsCount = 2;
    string filter;

    auto *resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, leftTableOutputColsCount,
        leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
        rightTableOutputColsCount, rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    std::vector<int8_t> isPreMatched;
    isPreMatched.insert(isPreMatched.end(), 6, 0);
    std::vector<int64_t> leftAddress1 = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 1)), static_cast<int64_t>(EncodeSyntheticAddress(0, 3)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 5)), static_cast<int64_t>(EncodeSyntheticAddress(1, 1)),
        static_cast<int64_t>(EncodeSyntheticAddress(1, 3)), static_cast<int64_t>(EncodeSyntheticAddress(1, 5))
    };
    std::vector<int64_t> rightAddress1 = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 0)), static_cast<int64_t>(EncodeSyntheticAddress(0, 2)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 4)), static_cast<int64_t>(EncodeSyntheticAddress(1, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(1, 2)), static_cast<int64_t>(EncodeSyntheticAddress(1, 4))
    };

    std::vector<int64_t> &builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t> &builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t> &builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress1.begin(), rightAddress1.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress1.begin(), leftAddress1.end());
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *outputVecBatch;

    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    int32_t expectedData1[6] = {1, 3, 5, 7, 9, 11};
    double expectedData2[6] = {1.1, 3.3, 5.5, 7.7, 9.9, 11.1};
    double expectedData3[6] = {5.5, 3.3, 1.1, 11.1, 9.9, 7.7};
    string expectedData4[6] = {"555", "33", "1", "111", "99", "7"};

    AssertVecBatchEquals(outputVecBatch, 4, 6, expectedData1, expectedData2, expectedData3, expectedData4);

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderBatchedVarcharGather)
{
    const int32_t sourceRowCount = 12;
    std::vector<DataTypePtr> leftTypes = {IntType(), VarcharType(64)};
    std::vector<DataTypePtr> rightTypes = {IntType(), VarcharType(64)};
    DataTypes leftSourceTypes(leftTypes);
    DataTypes rightSourceTypes(rightTypes);
    int32_t computeCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, computeCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, computeCols, 1);

    int32_t leftKeys[sourceRowCount] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    std::vector<std::string> leftValues = {
        "left-0", "", "left-2", "ignored-left-null", "left-4", "left-value-five-is-long",
        "left-6", "left-7", "left-8", "left-9", "left-10", "left-11"
    };
    std::vector<uint8_t> leftNulls(sourceRowCount, 0);
    leftNulls[3] = 1;
    auto *leftBatch = new VectorBatch(sourceRowCount);
    leftBatch->Append(CreateVector<int32_t>(sourceRowCount, leftKeys));
    leftBatch->Append(CreateSmjTestVarcharVector(leftValues, leftNulls));
    leftPagesIndex->AddVecBatch(leftBatch);

    int32_t rightKeys[sourceRowCount] = {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111};
    std::vector<std::string> rightValues = {
        "right-0", "right-1", "right-2", "right-3", "right-4", "right-5",
        "ignored-right-null", "right-7", "right-8", "right-9", "", "right-11-long-value"
    };
    std::vector<uint8_t> rightNulls(sourceRowCount, 0);
    rightNulls[6] = 1;
    auto *rightBatch = new VectorBatch(sourceRowCount);
    rightBatch->Append(CreateVector<int32_t>(sourceRowCount, rightKeys));
    rightBatch->Append(CreateSmjTestVarcharVector(rightValues, rightNulls));
    rightPagesIndex->AddVecBatch(rightBatch);

    std::vector<DataTypePtr> leftOutputTypes = {VarcharType(64)};
    std::vector<DataTypePtr> rightOutputTypes = {VarcharType(64)};
    int32_t leftOutputCols[] = {1};
    int32_t rightOutputCols[] = {1};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(leftOutputTypes, leftOutputCols, 1,
        leftSourceTypes.GetSize(), leftPagesIndex, rightOutputTypes, rightOutputCols, 1,
        rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    // Both sides form one 12-row segment (above the 8-row cutover). The selections are
    // deliberately non-monotonic and repeat source rows.
    std::vector<int32_t> leftRows = {9, 0, 9, 3, 1, 7, 2, 11, 5, 4, 6, 8};
    std::vector<int32_t> rightRows = {2, 10, 2, 0, 6, 3, 11, 1, 4, 8, 7, 5};
    std::vector<int32_t> batchIds(leftRows.size(), 0);
    AppendSmjTestAddresses(resultBuilder, batchIds, leftRows, batchIds, rightRows);
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetVectorCount(), 2);
    ASSERT_EQ(output->GetRowCount(), static_cast<int32_t>(leftRows.size()));

    std::vector<std::string> expectedLeft;
    std::vector<std::string> expectedRight;
    std::vector<uint8_t> expectedLeftNulls;
    std::vector<uint8_t> expectedRightNulls;
    for (size_t row = 0; row < leftRows.size(); ++row) {
        expectedLeft.push_back(leftValues[leftRows[row]]);
        expectedLeftNulls.push_back(leftNulls[leftRows[row]]);
        expectedRight.push_back(rightValues[rightRows[row]]);
        expectedRightNulls.push_back(rightNulls[rightRows[row]]);
    }
    AssertSmjTestVarcharValues(output->Get(0), expectedLeft, expectedLeftNulls);
    AssertSmjTestVarcharValues(output->Get(1), expectedRight, expectedRightNulls);

    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(output);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderLeftJoinBatchedVarcharAndNullSegments)
{
    const int32_t leftRowCount = 17;
    std::vector<DataTypePtr> leftTypes = {IntType()};
    std::vector<DataTypePtr> rightTypes = {IntType(), VarcharType(64)};
    DataTypes leftSourceTypes(leftTypes);
    DataTypes rightSourceTypes(rightTypes);
    int32_t computeCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, computeCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, computeCols, 1);

    int32_t leftValues[leftRowCount] = {
        100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116
    };
    auto *leftBatch = new VectorBatch(leftRowCount);
    leftBatch->Append(CreateVector<int32_t>(leftRowCount, leftValues));
    leftPagesIndex->AddVecBatch(leftBatch);

    const int32_t rightRowCount = 11;
    int32_t rightKeys[rightRowCount] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<std::string> rightValues = {
        "zero", "", "two", "ignored-source-null", "four", "right-value-five-is-long",
        "", "seven", "eight", "repeat-nine", "tail"
    };
    std::vector<uint8_t> rightNulls(rightRowCount, 0);
    rightNulls[3] = 1;
    auto *rightBatch = new VectorBatch(rightRowCount);
    rightBatch->Append(CreateVector<int32_t>(rightRowCount, rightKeys));
    rightBatch->Append(CreateSmjTestVarcharVector(rightValues, rightNulls));
    rightPagesIndex->AddVecBatch(rightBatch);

    std::vector<DataTypePtr> leftOutputTypes = {IntType()};
    std::vector<DataTypePtr> rightOutputTypes = {VarcharType(64)};
    int32_t leftOutputCols[] = {0};
    int32_t rightOutputCols[] = {1};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(leftOutputTypes, leftOutputCols, 1,
        leftSourceTypes.GetSize(), leftPagesIndex, rightOutputTypes, rightOutputCols, 1,
        rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    // First materialization creates a five-row output vector with actual/null/actual segments.
    AppendSmjTestAddresses(resultBuilder, {0, 0, 0, 0, 0}, {0, 1, 2, 3, 4},
        {0, 0, JOIN_NULL_FLAG, JOIN_NULL_FLAG, 0}, {0, 1, 0, 0, 2});
    resultBuilder->AddJoinValueAddresses();

    // The next call expands that vector. Its first right-side run has exactly eight rows,
    // exercising batched SetValues at a non-zero output offset, followed by a null run.
    AppendSmjTestAddresses(resultBuilder,
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
        {0, 0, 0, 0, 0, 0, 0, 0, JOIN_NULL_FLAG, JOIN_NULL_FLAG, JOIN_NULL_FLAG, 0},
        {9, 3, 9, 4, 6, 5, 7, 8, 0, 0, 0, 10});
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetVectorCount(), 2);
    ASSERT_EQ(output->GetRowCount(), leftRowCount);

    auto *leftOutput = static_cast<Vector<int32_t> *>(output->Get(0));
    for (int32_t row = 0; row < leftRowCount; ++row) {
        ASSERT_FALSE(leftOutput->IsNull(row));
        ASSERT_EQ(leftOutput->GetValue(row), leftValues[row]);
    }
    std::vector<std::string> expectedRight = {
        "zero", "", "", "", "two",
        "repeat-nine", "", "repeat-nine", "four", "", "right-value-five-is-long", "seven", "eight",
        "", "", "", "tail"
    };
    std::vector<uint8_t> expectedRightNulls = {
        0, 0, 1, 1, 0,
        0, 1, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 0
    };
    AssertSmjTestVarcharValues(output->Get(1), expectedRight, expectedRightNulls);

    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(output);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderDictionaryVarcharFallback)
{
    const int32_t sourceRowCount = 12;
    std::vector<DataTypePtr> leftTypes = {IntType(), VarcharType(64)};
    std::vector<DataTypePtr> rightTypes = {IntType()};
    DataTypes leftSourceTypes(leftTypes);
    DataTypes rightSourceTypes(rightTypes);
    int32_t computeCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, computeCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, computeCols, 1);

    std::vector<std::string> dictionaryValues = {
        "alpha", "", "gamma", "ignored-dictionary-null", "epsilon", "dictionary-long-value"
    };
    std::vector<uint8_t> dictionaryNulls = {0, 0, 0, 1, 0, 0};
    auto *dictionaryBase = CreateSmjTestVarcharVector(dictionaryValues, dictionaryNulls);
    int32_t dictionaryIds[sourceRowCount] = {5, 0, 3, 1, 4, 2, 0, 5, 1, 3, 4, 2};
    auto *dictionaryVector = VectorHelper::CreateDictionaryVector(dictionaryIds, sourceRowCount,
        dictionaryBase, omniruntime::type::OMNI_VARCHAR);
    delete dictionaryBase;
    ASSERT_EQ(dictionaryVector->GetEncoding(), vec::OMNI_DICTIONARY);

    int32_t leftKeys[sourceRowCount] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto *leftBatch = new VectorBatch(sourceRowCount);
    leftBatch->Append(CreateVector<int32_t>(sourceRowCount, leftKeys));
    leftBatch->Append(dictionaryVector);
    leftPagesIndex->AddVecBatch(leftBatch);

    int32_t rightKey[] = {0};
    auto *rightBatch = new VectorBatch(1);
    rightBatch->Append(CreateVector<int32_t>(1, rightKey));
    rightPagesIndex->AddVecBatch(rightBatch);

    std::vector<DataTypePtr> leftOutputTypes = {VarcharType(64)};
    std::vector<DataTypePtr> rightOutputTypes;
    int32_t leftOutputCols[] = {1};
    int32_t unusedRightOutputCols[] = {0};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(leftOutputTypes, leftOutputCols, 1,
        leftSourceTypes.GetSize(), leftPagesIndex, rightOutputTypes, unusedRightOutputCols, 0,
        rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    // Ten rows make CopyOutputSegment attempt the batched path. Dictionary encoding must
    // reject that path without mutating output, then resolve the same arbitrary selection scalarly.
    std::vector<int32_t> leftRows = {11, 2, 7, 0, 4, 2, 9, 1, 8, 5};
    std::vector<int32_t> leftBatchIds(leftRows.size(), 0);
    std::vector<int32_t> rightBatchIds(leftRows.size(), 0);
    std::vector<int32_t> rightRows(leftRows.size(), 0);
    AppendSmjTestAddresses(resultBuilder, leftBatchIds, leftRows, rightBatchIds, rightRows);
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetVectorCount(), 1);
    ASSERT_EQ(output->GetRowCount(), static_cast<int32_t>(leftRows.size()));

    std::vector<std::string> expected;
    std::vector<uint8_t> expectedNulls;
    for (int32_t sourceRow : leftRows) {
        const int32_t dictionaryRow = dictionaryIds[sourceRow];
        expected.push_back(dictionaryValues[dictionaryRow]);
        expectedNulls.push_back(dictionaryNulls[dictionaryRow]);
    }
    AssertSmjTestVarcharValues(output->Get(0), expected, expectedNulls);

    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(output);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderComplexSelectionRegression)
{
    using namespace omniruntime::type;
    const int32_t sourceRowCount = 5;
    auto arrayType = ArrayDataType(IntType());
    std::vector<DataTypePtr> rowChildTypes = {IntType(), VarcharType(16)};
    auto rowType = std::make_shared<RowType>(rowChildTypes, std::vector<std::string>{"id", "name"});
    std::vector<DataTypePtr> leftTypes = {IntType(), arrayType, rowType};
    std::vector<DataTypePtr> rightTypes = {IntType()};
    DataTypes leftSourceTypes(leftTypes);
    DataTypes rightSourceTypes(rightTypes);
    int32_t computeCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, computeCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, computeCols, 1);

    int32_t leftKeys[sourceRowCount] = {0, 1, 2, 3, 4};
    int32_t arrayElements[] = {10, 20, 21, 30, 40, 41, 42};
    auto *arrayVector = new vec::ArrayVector(sourceRowCount,
        std::shared_ptr<vec::BaseVector>(CreateVector<int32_t>(7, arrayElements)));
    arrayVector->SetOffset(0, 0);
    arrayVector->SetSize(0, 1);
    arrayVector->SetOffset(1, 1);
    arrayVector->SetSize(1, 2);
    arrayVector->SetOffset(2, 3);
    arrayVector->SetSize(2, 0);
    arrayVector->SetOffset(3, 3);
    arrayVector->SetSize(3, 1);
    arrayVector->SetNull(3);
    arrayVector->SetOffset(4, 4);
    arrayVector->SetSize(4, 3);

    int32_t rowIds[sourceRowCount] = {100, 101, 102, 103, 104};
    std::vector<std::string> rowNames = {"row-0", "", "ignored-row-null", "row-3", "ignored-child-null"};
    std::vector<uint8_t> rowNameNulls = {0, 0, 0, 0, 1};
    auto *rowVector = new vec::RowVector(sourceRowCount);
    rowVector->AddChild(std::shared_ptr<vec::BaseVector>(CreateVector<int32_t>(sourceRowCount, rowIds)));
    rowVector->AddChild(std::shared_ptr<vec::BaseVector>(CreateSmjTestVarcharVector(rowNames, rowNameNulls)));
    // Mark only the ROW itself null. RowVector::SetNull also rewrites every child; doing
    // that after the VARCHAR child is populated would rewind its append-only offsets.
    static_cast<vec::BaseVector *>(rowVector)->SetNull(2);

    auto *leftBatch = new VectorBatch(sourceRowCount);
    leftBatch->Append(CreateVector<int32_t>(sourceRowCount, leftKeys));
    leftBatch->Append(arrayVector);
    leftBatch->Append(rowVector);
    leftPagesIndex->AddVecBatch(leftBatch);

    int32_t rightKey[] = {0};
    auto *rightBatch = new VectorBatch(1);
    rightBatch->Append(CreateVector<int32_t>(1, rightKey));
    rightPagesIndex->AddVecBatch(rightBatch);

    std::vector<DataTypePtr> leftOutputTypes = {arrayType, rowType};
    std::vector<DataTypePtr> rightOutputTypes;
    int32_t leftOutputCols[] = {1, 2};
    int32_t unusedRightOutputCols[] = {0};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(leftOutputTypes, leftOutputCols, 2,
        leftSourceTypes.GetSize(), leftPagesIndex, rightOutputTypes, unusedRightOutputCols, 0,
        rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    // This segment is also above the VARCHAR cutover, but ARRAY and ROW must retain
    // their recursive semantic copy path for repeated, out-of-order, null and empty values.
    std::vector<int32_t> leftRows = {4, 1, 3, 0, 4, 2, 1, 0, 3, 4};
    std::vector<int32_t> batchIds(leftRows.size(), 0);
    std::vector<int32_t> rightRows(leftRows.size(), 0);
    AppendSmjTestAddresses(resultBuilder, batchIds, leftRows, batchIds, rightRows);
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(output->GetVectorCount(), 2);
    ASSERT_EQ(output->GetRowCount(), static_cast<int32_t>(leftRows.size()));

    std::vector<std::vector<int32_t>> sourceArrays = {{10}, {20, 21}, {}, {}, {40, 41, 42}};
    std::vector<uint8_t> sourceArrayNulls = {0, 0, 0, 1, 0};
    auto *outputArray = static_cast<vec::ArrayVector *>(output->Get(0));
    for (int32_t outputRow = 0; outputRow < static_cast<int32_t>(leftRows.size()); ++outputRow) {
        const int32_t sourceRow = leftRows[outputRow];
        ASSERT_EQ(outputArray->IsNull(outputRow), sourceArrayNulls[sourceRow] != 0) << "row " << outputRow;
        if (sourceArrayNulls[sourceRow] != 0) {
            continue;
        }
        ASSERT_EQ(outputArray->GetSize(outputRow), static_cast<int32_t>(sourceArrays[sourceRow].size()));
        std::unique_ptr<vec::BaseVector> elements(outputArray->GetValue(outputRow));
        auto *intElements = static_cast<Vector<int32_t> *>(elements.get());
        for (int32_t element = 0; element < static_cast<int32_t>(sourceArrays[sourceRow].size()); ++element) {
            ASSERT_EQ(intElements->GetValue(element), sourceArrays[sourceRow][element]);
        }
    }

    auto *outputRow = static_cast<vec::RowVector *>(output->Get(1));
    auto *outputRowIds = static_cast<Vector<int32_t> *>(outputRow->ChildAt(0).get());
    auto *outputRowNames = static_cast<SmjTestVarcharVector *>(outputRow->ChildAt(1).get());
    std::vector<uint8_t> sourceRowNulls = {0, 0, 1, 0, 0};
    for (int32_t outputIndex = 0; outputIndex < static_cast<int32_t>(leftRows.size()); ++outputIndex) {
        const int32_t sourceRow = leftRows[outputIndex];
        ASSERT_EQ(outputRow->IsNull(outputIndex), sourceRowNulls[sourceRow] != 0) << "row " << outputIndex;
        if (sourceRowNulls[sourceRow] != 0) {
            continue;
        }
        ASSERT_EQ(outputRowIds->GetValue(outputIndex), rowIds[sourceRow]);
        ASSERT_EQ(outputRowNames->IsNull(outputIndex), rowNameNulls[sourceRow] != 0);
        if (rowNameNulls[sourceRow] != 0) {
            ASSERT_TRUE(outputRowNames->GetValue(outputIndex).empty());
        } else {
            ASSERT_EQ(outputRowNames->GetValue(outputIndex), std::string_view(rowNames[sourceRow]));
        }
    }

    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(output);
}

TEST(NativeSortMergeJoinTest, TestLeftJoinPageReleaseHonorsStreamedPin)
{
    DataTypes leftTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes rightTypes(std::vector<DataTypePtr>{IntType()});
    int32_t keyColumns[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftTypes, keyColumns, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightTypes, keyColumns, 1);
    std::vector<int32_t> leftDestroyed(4, 0);
    std::vector<int32_t> rightDestroyed(4, 0);
    for (int32_t batch = 0; batch < 4; ++batch) {
        leftPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, leftDestroyed[batch]));
        rightPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, rightDestroyed[batch]));
    }

    std::vector<DataTypePtr> noOutputTypes;
    int32_t unusedOutputColumns[] = {0};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(noOutputTypes, unusedOutputColumns, 0,
        leftTypes.GetSize(), leftPagesIndex, noOutputTypes, unusedOutputColumns, 0,
        rightTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    // Batch 2 has been materialized, but the scanner still compares a pending row in
    // streamed batch 1. Only batch 0 may be reclaimed on the streamed side. The
    // buffered side has no such pin and can release both batches below batch 2.
    AppendSmjTestAddresses(resultBuilder, {2}, {0}, {2}, {0});
    resultBuilder->SetStreamedPinBatchId(1);
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 0);
    EXPECT_EQ(leftDestroyed[0], 1);
    EXPECT_EQ(leftDestroyed[1], 0);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(leftDestroyed[3], 0);
    EXPECT_EQ(rightDestroyed[0], 1);
    EXPECT_EQ(rightDestroyed[1], 1);
    EXPECT_EQ(rightDestroyed[2], 0);
    EXPECT_EQ(rightDestroyed[3], 0);

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    if (output != nullptr) {
        VectorHelper::FreeVecBatch(output);
    }
    for (int32_t batch = 0; batch < 4; ++batch) {
        EXPECT_EQ(leftDestroyed[batch], 1) << "left batch " << batch;
        EXPECT_EQ(rightDestroyed[batch], 1) << "right batch " << batch;
    }
}

TEST(NativeSortMergeJoinTest, TestLeftJoinTracksWindowMinimumSeparatelyFromReleaseLimit)
{
    DataTypes leftTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes rightTypes(std::vector<DataTypePtr>{IntType()});
    int32_t keyColumns[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftTypes, keyColumns, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightTypes, keyColumns, 1);
    std::vector<int32_t> leftDestroyed(4, 0);
    std::vector<int32_t> rightDestroyed(4, 0);
    for (int32_t batch = 0; batch < 4; ++batch) {
        leftPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, leftDestroyed[batch]));
        rightPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, rightDestroyed[batch]));
    }

    std::vector<DataTypePtr> noOutputTypes;
    int32_t unusedOutputColumns[] = {0};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(noOutputTypes, unusedOutputColumns, 0,
        leftTypes.GetSize(), leftPagesIndex, noOutputTypes, unusedOutputColumns, 0,
        rightTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    // Both addresses are consumed in one output window. The persistent lease must
    // record the window minimum (2), while the temporary scanner pin (1) controls
    // only this call's release limit.
    AppendSmjTestAddresses(resultBuilder, {2, 3}, {0, 0}, {2, 3}, {0, 0});
    resultBuilder->SetStreamedPinBatchId(1);
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 0);
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), 2);
    EXPECT_EQ(leftDestroyed[0], 1);
    EXPECT_EQ(leftDestroyed[1], 0);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(leftDestroyed[3], 0);

    VectorBatch *firstOutput = nullptr;
    resultBuilder->GetOutput(&firstOutput);
    if (firstOutput != nullptr) {
        VectorHelper::FreeVecBatch(firstOutput);
    }

    // Removing the temporary pin lets the old window lease advance the release
    // limit to 2. A mistakenly persisted pin would keep batch 1 alive; tracking
    // only the previous window's last address would incorrectly release batch 2.
    resultBuilder->SetStreamedPinBatchId(INT32_MAX);
    AppendSmjTestAddresses(resultBuilder, {3}, {0}, {3}, {0});
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 0);
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), 2);
    EXPECT_EQ(leftDestroyed[1], 1);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(leftDestroyed[3], 0);

    VectorBatch *secondOutput = nullptr;
    resultBuilder->GetOutput(&secondOutput);
    resultBuilder->Finish();
    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    if (secondOutput != nullptr) {
        VectorHelper::FreeVecBatch(secondOutput);
    }
    for (int32_t batch = 0; batch < 4; ++batch) {
        EXPECT_EQ(leftDestroyed[batch], 1) << "left batch " << batch;
        EXPECT_EQ(rightDestroyed[batch], 1) << "right batch " << batch;
    }
}

TEST(NativeSortMergeJoinTest, TestLeftJoinConsumedLeaseAcrossOutputWindowsAndClear)
{
    DataTypes leftTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes rightTypes(std::vector<DataTypePtr>{IntType()});
    int32_t keyColumns[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftTypes, keyColumns, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightTypes, keyColumns, 1);
    std::vector<int32_t> leftDestroyed(4, 0);
    std::vector<int32_t> rightDestroyed(4, 0);
    for (int32_t batch = 0; batch < 4; ++batch) {
        leftPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, leftDestroyed[batch]));
        rightPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, rightDestroyed[batch]));
    }

    std::vector<DataTypePtr> noOutputTypes;
    int32_t unusedOutputColumns[] = {0};
    std::string filter;
    const omniruntime::config::QueryConfig oneRowConfig(
        std::unordered_map<std::string, std::string>{
            {omniruntime::config::QueryConfig::KMaxBatchRowCount, "1"}});
    auto *resultBuilder = new JoinResultBuilder(noOutputTypes, unusedOutputColumns, 0,
        leftTypes.GetSize(), leftPagesIndex, noOutputTypes, unusedOutputColumns, 0,
        rightTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr, oneRowConfig);

    // Force two output windows from one duplicate-key group. The first window consumes
    // batch 2 and establishes the lease floor; the second consumes batch 3 but must not
    // reclaim batch 2 while the same scanner/result window is still active.
    AppendSmjTestAddresses(resultBuilder, {2, 3}, {0, 0}, {2, 3}, {0, 0});
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), INT32_MAX);
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 1);
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), 2);
    EXPECT_EQ(leftDestroyed[0], 1);
    EXPECT_EQ(leftDestroyed[1], 1);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(rightDestroyed[0], 1);
    EXPECT_EQ(rightDestroyed[1], 1);

    VectorBatch *firstOutput = nullptr;
    resultBuilder->GetOutput(&firstOutput);
    EXPECT_NE(firstOutput, nullptr);
    if (firstOutput != nullptr) {
        VectorHelper::FreeVecBatch(firstOutput);
    }

    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 1);
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), 2);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(leftDestroyed[3], 0);
    EXPECT_EQ(rightDestroyed[2], 1);
    EXPECT_EQ(rightDestroyed[3], 0);

    VectorBatch *secondOutput = nullptr;
    resultBuilder->GetOutput(&secondOutput);
    EXPECT_NE(secondOutput, nullptr);
    if (secondOutput != nullptr) {
        VectorHelper::FreeVecBatch(secondOutput);
    }

    // Clear starts a new scanner/result window. It must drop the old lease so a new
    // batch-3 address can advance the streamed watermark and release batch 2.
    resultBuilder->Clear();
    EXPECT_EQ(resultBuilder->GetMinConsumedStreamedBatchId(), INT32_MAX);
    AppendSmjTestAddresses(resultBuilder, {3}, {0}, {3}, {0});
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 1);
    EXPECT_EQ(leftDestroyed[2], 1);
    EXPECT_EQ(leftDestroyed[3], 0);

    VectorBatch *thirdOutput = nullptr;
    resultBuilder->GetOutput(&thirdOutput);
    if (thirdOutput != nullptr) {
        VectorHelper::FreeVecBatch(thirdOutput);
    }
    resultBuilder->Finish();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    for (int32_t batch = 0; batch < 4; ++batch) {
        EXPECT_EQ(leftDestroyed[batch], 1) << "left batch " << batch;
        EXPECT_EQ(rightDestroyed[batch], 1) << "right batch " << batch;
    }
}

TEST(NativeSortMergeJoinTest, TestLeftJoinUnmatchedRowsAdvanceOnlyStreamedRelease)
{
    DataTypes leftTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes rightTypes(std::vector<DataTypePtr>{IntType()});
    int32_t keyColumns[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftTypes, keyColumns, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightTypes, keyColumns, 1);
    std::vector<int32_t> leftDestroyed(4, 0);
    std::vector<int32_t> rightDestroyed(3, 0);
    for (int32_t batch = 0; batch < 4; ++batch) {
        leftPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, leftDestroyed[batch]));
    }
    for (int32_t batch = 0; batch < 3; ++batch) {
        rightPagesIndex->AddVecBatch(CreateSmjCountingBatch(batch, rightDestroyed[batch]));
    }

    std::vector<DataTypePtr> noOutputTypes;
    int32_t unusedOutputColumns[] = {0};
    std::string filter;
    auto *resultBuilder = new JoinResultBuilder(noOutputTypes, unusedOutputColumns, 0,
        leftTypes.GetSize(), leftPagesIndex, noOutputTypes, unusedOutputColumns, 0,
        rightTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    // A right-null-padded row still consumes a real streamed row. It must advance the
    // streamed watermark while leaving every buffered page untouched.
    AppendSmjTestAddresses(resultBuilder, {2}, {0}, {JOIN_NULL_FLAG}, {JOIN_NULL_FLAG});
    EXPECT_EQ(resultBuilder->AddJoinValueAddresses(), 0);
    EXPECT_EQ(leftDestroyed[0], 1);
    EXPECT_EQ(leftDestroyed[1], 1);
    EXPECT_EQ(leftDestroyed[2], 0);
    EXPECT_EQ(leftDestroyed[3], 0);
    for (int32_t batch = 0; batch < 3; ++batch) {
        EXPECT_EQ(rightDestroyed[batch], 0) << "right batch " << batch;
    }

    VectorBatch *output = nullptr;
    resultBuilder->GetOutput(&output);
    resultBuilder->Finish();
    delete resultBuilder;
    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete leftPagesIndex;
    delete rightPagesIndex;
    if (output != nullptr) {
        VectorHelper::FreeVecBatch(output);
    }
    for (int32_t batch = 0; batch < 4; ++batch) {
        EXPECT_EQ(leftDestroyed[batch], 1) << "left batch " << batch;
    }
    for (int32_t batch = 0; batch < 3; ++batch) {
        EXPECT_EQ(rightDestroyed[batch], 1) << "right batch " << batch;
    }
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderLeftJoin)
{
    std::vector<DataTypePtr> leftTypes = { IntType(), DoubleType() };
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = { IntType(), DoubleType(), VarcharType(3) };
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    const int32_t dataSize = 6;
    int32_t leftData1_1[dataSize] = {0, 1, 2, 3, 4, 5};
    double leftData1_2[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};
    auto *leftVecBatch1 = new VectorBatch(dataSize);
    leftVecBatch1->Append(CreateVector<int32_t>(dataSize, leftData1_1));
    leftVecBatch1->Append(CreateVector<double>(dataSize, leftData1_2));

    int32_t leftData21[dataSize] = {6, 7, 8, 9, 10, 11};
    double leftData22[dataSize] = {6.6, 7.7, 8.8, 9.9, 10.1, 11.1};
    auto *leftVecBatch2 = new VectorBatch(dataSize);
    leftVecBatch2->Append(CreateVector<int32_t>(dataSize, leftData21));
    leftVecBatch2->Append(CreateVector<double>(dataSize, leftData22));
    leftPagesIndex->AddVecBatch(leftVecBatch1);
    leftPagesIndex->AddVecBatch(leftVecBatch2);

    std::vector<DataTypePtr> leftTableOutputTypes { IntType(), DoubleType() };
    int32_t leftTableOutputCols[2] = {0, 1};
    int32_t leftTableOutputColsCount = 2;

    int32_t rightData11[dataSize] = {5, 4, 3, 2, 1, 0};
    double rightData12[dataSize] = {5.5, 4.4, 3.3, 2.2, 1.1, 0.0};
    std::string rightData13[dataSize] = {"555", "444", "33", "22", "1", "0"};
    int32_t rightData21[dataSize] = {11, 10, 9, 8, 7, 6};
    double rightData22[dataSize] = {11.1, 10.1, 9.9, 8.8, 7.7, 6.6};
    std::string rightData23[dataSize] = {"111", "101", "99", "88", "7", "6"};
    VectorBatch *rightVecBatch1 = CreateVectorBatch(rightSourceTypes, dataSize, rightData11, rightData12, rightData13);
    VectorBatch *rightVecBatch2 = CreateVectorBatch(rightSourceTypes, dataSize, rightData21, rightData22, rightData23);
    rightPagesIndex->AddVecBatch(rightVecBatch1);
    rightPagesIndex->AddVecBatch(rightVecBatch2);

    std::vector<DataTypePtr> rightTableOutputTypes { DoubleType(), VarcharType(3) };
    int32_t rightTableOutputCols[2] = {1, 2};
    int32_t rightTableOutputColsCount = 2;
    string filter;

    auto *resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, leftTableOutputColsCount,
        leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
        rightTableOutputColsCount, rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    std::vector<int8_t> isPreMatched(6, 0);
    std::vector<int64_t> leftAddress1 = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 1)), static_cast<int64_t>(EncodeSyntheticAddress(0, 3)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 5)), static_cast<int64_t>(EncodeSyntheticAddress(1, 1)),
        static_cast<int64_t>(EncodeSyntheticAddress(1, 3)), static_cast<int64_t>(EncodeSyntheticAddress(1, 2))
    };
    std::vector<int64_t> rightAddress1 = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 0)), static_cast<int64_t>(EncodeSyntheticAddress(0, 2)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 4)), static_cast<int64_t>(EncodeSyntheticAddress(1, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(1, 2)), static_cast<int64_t>(EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG))
    };

    std::vector<int64_t> &builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t> &builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t> &builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress1.begin(), rightAddress1.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress1.begin(), leftAddress1.end());
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *outputVecBatch;
    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    int32_t expectedLeft1[6] = {1, 3, 5, 7, 9, 8};
    double expectedLeft2[6] = {1.1, 3.3, 5.5, 7.7, 9.9, 8.8};
    double expectedRight1[5] = {5.5, 3.3, 1.1, 11.1, 9.9};
    std::string expectedRight2[5] = {"555", "33", "1", "111", "99"};

    ASSERT_EQ(outputVecBatch->GetVectorCount(), 4);
    ASSERT_EQ(outputVecBatch->GetRowCount(), 6);
    for (int32_t i = 0; i < 6; i++) {
        ASSERT_EQ((static_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i), expectedLeft1[i]);
        ASSERT_EQ((static_cast<Vector<double> *>(outputVecBatch->Get(1)))->GetValue(i), expectedLeft2[i]);
        if (i < 5) {
            ASSERT_EQ((static_cast<Vector<double> *>(outputVecBatch->Get(2)))->GetValue(i), expectedRight1[i]);
            ASSERT_FALSE(outputVecBatch->Get(2)->IsNull(i));
            ASSERT_EQ((static_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(3)))->GetValue(i),
                std::string_view(expectedRight2[i]));
            ASSERT_FALSE(outputVecBatch->Get(3)->IsNull(i));
        } else {
            ASSERT_TRUE(outputVecBatch->Get(2)->IsNull(i));
            ASSERT_TRUE(outputVecBatch->Get(3)->IsNull(i));
        }
    }

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

// Regression test for NegativeArraySizeException: Left Join with VARCHAR on right and pad-null row
// must set LargeStringContainer offsets (via SetNull per row) so reading the batch does not yield negative length.
TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderLeftJoinVarcharPadNullOnly)
{
    std::vector<DataTypePtr> leftTypes = { IntType() };
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = { IntType(), VarcharType(10) };
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    int32_t leftData[2] = {1, 2};
    auto *leftVecBatch = new VectorBatch(2);
    leftVecBatch->Append(CreateVector<int32_t>(2, leftData));
    leftPagesIndex->AddVecBatch(leftVecBatch);

    int32_t rightData1[1] = {1};
    std::string rightData2[1] = {"matched"};
    VectorBatch *rightVecBatch = CreateVectorBatch(rightSourceTypes, 1, rightData1, rightData2);
    rightPagesIndex->AddVecBatch(rightVecBatch);

    std::vector<DataTypePtr> leftTableOutputTypes = { IntType() };
    int32_t leftTableOutputCols[1] = {0};
    std::vector<DataTypePtr> rightTableOutputTypes = { VarcharType(10) };
    int32_t rightTableOutputCols[1] = {1};
    std::string filter;

    auto *resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, 1,
        leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
        1, rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_LEFT, nullptr);

    std::vector<int8_t> isPreMatched(2, 0);
    std::vector<int64_t> leftAddress = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 1))
    };
    std::vector<int64_t> rightAddress = {
        static_cast<int64_t>(EncodeSyntheticAddress(0, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(JOIN_NULL_FLAG, JOIN_NULL_FLAG))
    };

    std::vector<int64_t> &builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t> &builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t> &builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress.begin(), rightAddress.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress.begin(), leftAddress.end());
    resultBuilder->AddJoinValueAddresses();

    VectorBatch *outputVecBatch = nullptr;
    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    ASSERT_NE(outputVecBatch, nullptr);
    ASSERT_EQ(outputVecBatch->GetVectorCount(), 2);
    ASSERT_EQ(outputVecBatch->GetRowCount(), 2);

    ASSERT_FALSE(outputVecBatch->Get(1)->IsNull(0));
    ASSERT_EQ((static_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(1)))->GetValue(0),
        std::string_view("matched"));

    ASSERT_TRUE(outputVecBatch->Get(1)->IsNull(1));
    std::string_view padNullVal =
        (static_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(1)))->GetValue(1);
    ASSERT_EQ(padNullVal.size(), 0u);

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderWithFilter)
{
    std::vector<DataTypePtr> leftTypes = { IntType(), DoubleType() };
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = { IntType(), DoubleType(), VarcharType(3) };
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    const int32_t dataSize = 6;
    int32_t leftData11[dataSize] = {0, 1, 2, 3, 4, 5};
    double leftData12[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};

    VectorBatch *leftVecBatch = CreateVectorBatch(leftSourceTypes, dataSize, leftData11, leftData12);
    leftPagesIndex->AddVecBatch(leftVecBatch);
    std::vector<DataTypePtr> leftTableOutputTypes { IntType(), DoubleType() };
    int32_t leftTableOutputCols[2] = {0, 1};
    int32_t leftTableOutputColsCount = 2;

    int32_t rightData11[dataSize] = {5, 4, 3, 2, 1, 0};
    double rightData12[dataSize] = {5.5, 4.4, 3.3, 2.2, 1.1, 0.0};
    std::string rightData1_3[dataSize] = {"555", "444", "33", "22", "1", "0"};

    VectorBatch *rightVecBatch = CreateVectorBatch(rightSourceTypes, dataSize, rightData11, rightData12, rightData1_3);
    rightPagesIndex->AddVecBatch(rightVecBatch);
    std::vector<DataTypePtr> rightTableOutputTypes { DoubleType(), VarcharType(3) };
    int32_t rightTableOutputCols[2] = {1, 2};
    int32_t rightTableOutputColsCount = 2;
    string filter = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"GREATER_THAN\",\"left\":{\"exprType\":"
        "\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":"
        "1,\"isNull\":false,\"value\":1}}";

    auto *resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, leftTableOutputColsCount,
        leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
        rightTableOutputColsCount, rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    std::vector<int8_t> isPreMatched;
    isPreMatched.insert(isPreMatched.end(), 3, 0);
    std::vector<int64_t> leftAddress1 = { static_cast<int64_t>(EncodeSyntheticAddress(0, 1)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 3)), static_cast<int64_t>(EncodeSyntheticAddress(0, 5)) };
    std::vector<int64_t> rightAddress1 = { static_cast<int64_t>(EncodeSyntheticAddress(0, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 2)), static_cast<int64_t>(EncodeSyntheticAddress(0, 4)) };

    std::vector<int8_t> isSameKey;
    std::vector<int64_t> &builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t> &builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t> &builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress1.begin(), rightAddress1.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress1.begin(), leftAddress1.end());
    ASSERT_EQ(resultBuilder->AddJoinValueAddresses(), 0);

    VectorBatch *outputVecBatch;

    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    int32_t expectedData1[2] = {3, 5};
    double expectedData2[2] = {3.3, 5.5};
    double expectedData3[2] = {3.3, 1.1};
    string expectedData4[2] = {"33", "1"};

    AssertVecBatchEquals(outputVecBatch, 4, 2, expectedData1, expectedData2, expectedData3, expectedData4);

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderWithFilterDataType)
{
    std::vector<DataTypePtr> leftTypes = { IntType(), DoubleType(), LongType(), FloatType()};
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = { IntType(), DoubleType(), VarcharType(3) };
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto *leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto *rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    const int32_t dataSize = 6;
    int32_t leftData11[dataSize] = {0, 1, 2, 3, 4, 5};
    double leftData12[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};
    int64_t leftData33[dataSize] = {0, 1, 2, 3, 4, 5};
    float leftData13[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};

    VectorBatch *leftVecBatch = CreateVectorBatch(leftSourceTypes, dataSize, leftData11, leftData12, leftData33, leftData13);
    leftPagesIndex->AddVecBatch(leftVecBatch);
    std::vector<DataTypePtr> leftTableOutputTypes { IntType(), DoubleType(), LongType(), FloatType() };
    int32_t leftTableOutputCols[4] = {0, 1, 2, 3};
    int32_t leftTableOutputColsCount = 4;

    int32_t rightData11[dataSize] = {5, 4, 3, 2, 1, 0};
    double rightData12[dataSize] = {5.5, 4.4, 3.3, 2.2, 1.1, 0.0};
    std::string rightData1_3[dataSize] = {"555", "444", "33", "22", "1", "0"};

    VectorBatch *rightVecBatch = CreateVectorBatch(rightSourceTypes, dataSize, rightData11, rightData12, rightData1_3);
    rightPagesIndex->AddVecBatch(rightVecBatch);
    std::vector<DataTypePtr> rightTableOutputTypes { DoubleType(), VarcharType(3) };
    int32_t rightTableOutputCols[2] = {1, 2};
    int32_t rightTableOutputColsCount = 2;
    string filter = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"GREATER_THAN\",\"left\":{\"exprType\":"
                    "\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":"
                    "1,\"isNull\":false,\"value\":1}}";

    auto *resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, leftTableOutputColsCount,
                                                leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
                                                rightTableOutputColsCount, rightSourceTypes.GetSize(), rightPagesIndex, filter, OMNI_JOIN_TYPE_INNER, nullptr);

    std::vector<int8_t> isPreMatched;
    isPreMatched.insert(isPreMatched.end(), 3, 0);
    std::vector<int64_t> leftAddress1 = { static_cast<int64_t>(EncodeSyntheticAddress(0, 1)),
                                          static_cast<int64_t>(EncodeSyntheticAddress(0, 3)), static_cast<int64_t>(EncodeSyntheticAddress(0, 5)) };
    std::vector<int64_t> rightAddress1 = { static_cast<int64_t>(EncodeSyntheticAddress(0, 0)),
                                           static_cast<int64_t>(EncodeSyntheticAddress(0, 2)), static_cast<int64_t>(EncodeSyntheticAddress(0, 4)) };

    std::vector<int8_t> isSameKey;
    std::vector<int64_t> &builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t> &builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t> &builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress1.begin(), rightAddress1.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress1.begin(), leftAddress1.end());
    ASSERT_EQ(resultBuilder->AddJoinValueAddresses(), 0);

    VectorBatch *outputVecBatch;

    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    int32_t expectedData1[2] = {3, 5};
    double expectedData2[2] = {3.3, 5.5};
    long expectedData5[2] = {3, 5};
    float expectedData6[2] = {3.3, 5.5};
    double expectedData3[2] = {3.3, 1.1};
    string expectedData4[2] = {"33", "1"};

    AssertVecBatchEquals(outputVecBatch, 6, 2, expectedData1, expectedData2, expectedData5, expectedData6,
                         expectedData3, expectedData4);

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeSortMergeJoinTest, TestSortMergeJoinResultBuilderWithFilterExpr)
{
    std::vector<DataTypePtr> leftTypes = {IntType(), DoubleType()};
    DataTypes leftSourceTypes(leftTypes);
    std::vector<DataTypePtr> rightTypes = {IntType(), DoubleType(), VarcharType(3)};
    DataTypes rightSourceTypes(rightTypes);

    int32_t leftCols[] = {0};
    int32_t rightCols[] = {0};
    auto* leftPagesIndex = new DynamicPagesIndex(leftSourceTypes, leftCols, 1);
    auto* rightPagesIndex = new DynamicPagesIndex(rightSourceTypes, rightCols, 1);

    const int32_t dataSize = 6;
    int32_t leftData11[dataSize] = {0, 1, 2, 3, 4, 5};
    double leftData12[dataSize] = {0.0, 1.1, 2.2, 3.3, 4.4, 5.5};

    VectorBatch* leftVecBatch = CreateVectorBatch(leftSourceTypes, dataSize, leftData11, leftData12);
    leftPagesIndex->AddVecBatch(leftVecBatch);
    std::vector<DataTypePtr> leftTableOutputTypes{IntType(), DoubleType()};
    int32_t leftTableOutputCols[2] = {0, 1};
    int32_t leftTableOutputColsCount = 2;

    int32_t rightData11[dataSize] = {5, 4, 3, 2, 1, 0};
    double rightData12[dataSize] = {5.5, 4.4, 3.3, 2.2, 1.1, 0.0};
    std::string rightData1_3[dataSize] = {"555", "444", "33", "22", "1", "0"};

    VectorBatch* rightVecBatch = CreateVectorBatch(rightSourceTypes, dataSize, rightData11, rightData12, rightData1_3);
    rightPagesIndex->AddVecBatch(rightVecBatch);
    std::vector<DataTypePtr> rightTableOutputTypes{DoubleType(), VarcharType(3)};
    int32_t rightTableOutputCols[2] = {1, 2};
    int32_t rightTableOutputColsCount = 2;
    string filter = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"GREATER_THAN\",\"left\":{\"exprType\":"
                    "\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},\"right\":{\"exprType\":\"LITERAL\",\"dataType\":"
                    "1,\"isNull\":false,\"value\":1}}";
    omniruntime::expressions::Expr *filterExpr = JSONParser::ParseJSON(filter);

    auto* resultBuilder = new JoinResultBuilder(leftTableOutputTypes, leftTableOutputCols, leftTableOutputColsCount,
        leftSourceTypes.GetSize(), leftPagesIndex, rightTableOutputTypes, rightTableOutputCols,
        rightTableOutputColsCount, rightSourceTypes.GetSize(), rightPagesIndex, filterExpr, OMNI_JOIN_TYPE_INNER, nullptr);

    std::vector<int8_t> isPreMatched;
    isPreMatched.insert(isPreMatched.end(), 3, 0);
    std::vector<int64_t> leftAddress1 = {static_cast<int64_t>(EncodeSyntheticAddress(0, 1)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 3)), static_cast<int64_t>(EncodeSyntheticAddress(0, 5))};
    std::vector<int64_t> rightAddress1 = {static_cast<int64_t>(EncodeSyntheticAddress(0, 0)),
        static_cast<int64_t>(EncodeSyntheticAddress(0, 2)), static_cast<int64_t>(EncodeSyntheticAddress(0, 4))};

    std::vector<int8_t> isSameKey;
    std::vector<int64_t>& builderBufferedAddress = resultBuilder->GetBufferedTableValueAddresses();
    std::vector<int64_t>& builderStreamedAddress = resultBuilder->GetStreamedTableValueAddresses();
    std::vector<int8_t>& builderIsPreMatched = resultBuilder->GetPreKeyMatched();
    builderIsPreMatched.insert(builderIsPreMatched.end(), isPreMatched.begin(), isPreMatched.end());
    builderBufferedAddress.insert(builderBufferedAddress.end(), rightAddress1.begin(), rightAddress1.end());
    builderStreamedAddress.insert(builderStreamedAddress.end(), leftAddress1.begin(), leftAddress1.end());
    ASSERT_EQ(resultBuilder->AddJoinValueAddresses(), 0);

    VectorBatch* outputVecBatch;

    resultBuilder->GetOutput(&outputVecBatch);
    resultBuilder->Finish();

    int32_t expectedData1[2] = {3, 5};
    double expectedData2[2] = {3.3, 5.5};
    double expectedData3[2] = {3.3, 1.1};
    string expectedData4[2] = {"33", "1"};

    AssertVecBatchEquals(outputVecBatch, 4, 2, expectedData1, expectedData2, expectedData3, expectedData4);

    leftPagesIndex->FreeAllRemainingVecBatch();
    rightPagesIndex->FreeAllRemainingVecBatch();
    delete resultBuilder;
    delete leftPagesIndex;
    delete rightPagesIndex;
    delete filterExpr;
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeSortMergeJoinTest, TestSmjStreamingGetOutput)
{
    // select t1.b, t2.c from t1, t2 where t1.a = t2.d
    // streamedTbl t1:  int a, Long b;
    // bufferedTbl t2: long c, int d;
    std::string blank = "";
    SortMergeJoinOperator *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);

    std::vector<DataTypePtr> streamTypesVector = { IntType(), VarcharType(2000) };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols;
    streamedKeysCols.push_back(0);
    std::vector<int32_t> streamedOutputCols;
    streamedOutputCols.push_back(1);
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { VarcharType(2000), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols;
    bufferedKeysCols.push_back(1);
    std::vector<int32_t> bufferedOutputCols;
    bufferedOutputCols.push_back(0);
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    // construct data
    const int32_t streamedTblDataSize = 270;
    int32_t streamedTblDataCol1[streamedTblDataSize];
    std::string streamedTblDataCol2[streamedTblDataSize];
    for (uint32_t i = 0; i < streamedTblDataSize; i++) {
        streamedTblDataCol1[i] = i;
        streamedTblDataCol2[i] = std::to_string(i + 1);
    }
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);

    const int32_t bufferedTblSize = 270;
    std::string bufferedTblDataCol1[bufferedTblSize];
    int32_t bufferedTblDataCol2[bufferedTblSize];
    for (int32_t i = 0; i < streamedTblDataSize; i++) {
        bufferedTblDataCol1[i] = std::to_string(i + 3);
        bufferedTblDataCol2[i] = i;
    }
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add buffered table data
    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // need add buffered table data
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    std::vector<VectorBatch *> result;

    VectorBatch *result1;
    smjOp->GetOutput(&result1);
    result.emplace_back(result1);
    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_NORMAL);

    VectorBatch *result2;
    smjOp->GetOutput(&result2);
    result.emplace_back(result2);
    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_FINISHED);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    VectorBatch *result3;
    smjOp->GetOutput(&result3);
    result.push_back(result3);

    // check the join result
    int32_t resultCount = 0;
    int32_t index = 0;
    for (auto &res : result) {
        ASSERT_EQ(res->GetVectorCount(), 2);
        resultCount += res->GetRowCount();
        for (int32_t rowId = 0; rowId < res->GetRowCount(); ++rowId) {
            auto value1 =
                (reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(res->Get(0)))->GetValue(rowId);
            ASSERT_EQ(value1, streamedTblDataCol2[index]);

            auto value2 =
                (reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(res->Get(1)))->GetValue(rowId);
            ASSERT_EQ(value2, bufferedTblDataCol1[index]);
            index++;
        }
    }
    ASSERT_EQ(resultCount, streamedTblDataSize);

    VectorHelper::FreeVecBatches(result);
    omniruntime::op::Operator::DeleteOperator(smjOp);
}

TEST(NativeSortMergeJoinTest, TestSmjIterativeGetOutput)
{
    // select t1.b, t2.c from t1, t2 where t1.a = t2.d
    // streamedTbl t1:  int a, Long b;
    // bufferedTbl t2: long c, int d;
    std::string blank = "";
    SortMergeJoinOperator *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);

    std::vector<DataTypePtr> streamTypesVector = { IntType(), VarcharType(2000) };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols { 0 };
    std::vector<int32_t> streamedOutputCols { 1 };
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { VarcharType(2000), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols { 1 };
    std::vector<int32_t> bufferedOutputCols { 0 };
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    // construct data
    const int32_t streamedTblDataSize = 270;
    int32_t streamedTblCol1Data[streamedTblDataSize];
    std::string streamedTblCol2Data[streamedTblDataSize];
    for (int32_t i = 0; i < streamedTblDataSize; i++) {
        streamedTblCol1Data[i] = i;
        streamedTblCol2Data[i] = std::to_string(i + 1);
    }
    VectorBatch *streamedTblVecBatch =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblCol1Data, streamedTblCol2Data);

    const int32_t bufferedTblSize = 270;
    std::string bufferedTblCol1Data[bufferedTblSize];
    int32_t bufferedTblCol2Data[bufferedTblSize];
    for (uint32_t i = 0; i < streamedTblDataSize; i++) {
        bufferedTblCol1Data[i] = std::to_string(i + 3);
        bufferedTblCol2Data[i] = i;
    }
    VectorBatch *bufferedTblVecBatch =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblCol1Data, bufferedTblCol2Data);

    // need add buffered table data
    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // need add buffered table data
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    std::vector<VectorBatch *> result;

    VectorBatch *result1;
    smjOp->GetOutput(&result1);
    result.emplace_back(result1);
    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_NORMAL);

    VectorBatch *result2;
    smjOp->GetOutput(&result2);
    result.emplace_back(result2);
    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_FINISHED);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));


    VectorBatch *result3;
    smjOp->GetOutput(&result3);
    result.emplace_back(result3);
    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_FINISHED);

    // check the join result
    int32_t resultCount = 0;
    int32_t index = 0;
    for (auto &res : result) {
        ASSERT_EQ(res->GetVectorCount(), 2);
        resultCount += res->GetRowCount();
        for (int32_t rowId = 0; rowId < res->GetRowCount(); ++rowId) {
            auto value1 =
                (reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(res->Get(0)))->GetValue(rowId);
            ASSERT_EQ(value1, streamedTblCol2Data[index]);

            auto value2 =
                (reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(res->Get(1)))->GetValue(rowId);
            ASSERT_EQ(value2, bufferedTblCol1Data[index]);
            index++;
        }
    }
    ASSERT_EQ(resultCount, streamedTblDataSize);

    VectorHelper::FreeVecBatches(result);
    omniruntime::op::Operator::DeleteOperator(smjOp);
}


TEST(NativeSortMergeJoinTest, TestSmjOneLeftEqualMultiRight)
{
    // select t1.b, t2.c from t1, t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: long c, int d;
    string filter = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"EQUAL\",\"left\":{\"exprType\":"
        "\"FIELD_REFERENCE\",\"dataType\":2,\"colVal\":1},\"right\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":2,"
        "\"colVal\":2}}";
    auto *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, filter);

    std::vector<DataTypePtr> streamTypesVector = { IntType(), LongType() };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols;
    streamedKeysCols.emplace_back(0);
    std::vector<int32_t> streamedOutputCols{0, 1};
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { LongType(), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols;
    bufferedKeysCols.emplace_back(1);
    std::vector<int32_t> bufferedOutputCols{0, 1};
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    // construct data
    const int32_t streamedTblDataSize = 3;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 1};
    int64_t streamedTblDataCol2[streamedTblDataSize] = {6666, 4444, 4444};
    VectorBatch *streamedTblVecBatch1 =
            CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);

    int32_t streamedTblDataCol3[2] = {1, 5};
    int64_t streamedTblDataCol4[2] = {8888, 8888};
    VectorBatch *streamedTblVecBatch2 =
            CreateVectorBatch(streamedTblTypes, 2, streamedTblDataCol3, streamedTblDataCol4);

    const int32_t bufferedTblSize = 2;
    int64_t bufferedTblDataCol1[bufferedTblSize] = {1111, 8888};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 1};
    VectorBatch *bufferedTblVecBatch1 =
            CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    int64_t bufferedTblDataCol3[bufferedTblSize] = {6666, 5555};
    int32_t bufferedTblDataCol4[bufferedTblSize] = {1, 2};
    VectorBatch *bufferedTblVecBatch2 =
            CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol3, bufferedTblDataCol4);
    // need add buffered table data
    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
              static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // need add buffered table data
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatch1);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
              static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // table2 input
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatch2);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
              static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    // table2 input
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatch2);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
              static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
              static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedTblVecBatchEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));

    VectorBatch *result;
    smjOp->GetOutput(&result);

    ASSERT_EQ(smjOp->GetStatus(), OMNI_STATUS_FINISHED);
    // check the join result
    int32_t index = 0;
    ASSERT_EQ(result->GetVectorCount(), 4);
    int64_t expectInt64[2] = {6666, 8888};
    int32_t expectInt32[2] = {1, 1};
    for (int32_t j = 0; j < result->GetRowCount(); j++) {
        int64_t longValue = (reinterpret_cast<Vector<int64_t> *>(result->Get(1)))->GetValue(j);
        ASSERT_EQ(longValue, expectInt64[index]);

        int32_t intValue = (reinterpret_cast<Vector<int32_t> *>(result->Get(0)))->GetValue(j);
        ASSERT_EQ(intValue, expectInt32[index]);

        longValue = (reinterpret_cast<Vector<int64_t> *>(result->Get(2)))->GetValue(j);
        ASSERT_EQ(longValue, expectInt64[index]);
        intValue = (reinterpret_cast<Vector<int32_t> *>(result->Get(3)))->GetValue(j);
        ASSERT_EQ(intValue, expectInt32[index]);
        index++;
    }
    VectorHelper::FreeVecBatch(result);

    omniruntime::op::Operator::DeleteOperator(smjOp);
}

// Full SMJ operator test with array as join key and output column.
// Streamed keys [1], [1,2], [2], [3]; buffered keys [1], [2], [4], [5]. Inner match: [1] and [2] -> 2 rows.
TEST(NativeSortMergeJoinTest, TestSmjFullOperatorArrayKey)
{
    std::string blank = "";
    auto *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);

    std::vector<DataTypePtr> streamTypesVector = { ArrayDataType(IntType()), IntType() };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols = {0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { ArrayDataType(IntType()), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols = {0};
    std::vector<int32_t> bufferedOutputCols = {0, 1};
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    const int32_t dataSize = 4;
    // Streamed: key [1], [1,2], [2], [3]; payload 111, 11, 1, 0
    int32_t streamKeyElements[] = {1, 1, 2, 2, 3};
    auto *streamKeyElemVec = CreateVector<int32_t>(5, streamKeyElements);
    auto *streamKeyArrayVec = new vec::ArrayVector(dataSize, std::shared_ptr<vec::BaseVector>(streamKeyElemVec));
    streamKeyArrayVec->SetOffset(0, 0);
    streamKeyArrayVec->SetSize(0, 1);
    streamKeyArrayVec->SetOffset(1, 1);
    streamKeyArrayVec->SetSize(1, 2);
    streamKeyArrayVec->SetOffset(2, 3);
    streamKeyArrayVec->SetSize(2, 1);
    streamKeyArrayVec->SetOffset(3, 4);
    streamKeyArrayVec->SetSize(3, 1);
    int32_t streamedPayload[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(streamKeyArrayVec);
    streamedVecBatch->Append(CreateVector<int32_t>(dataSize, streamedPayload));

    // Buffered: key [1], [2], [4], [5]; payload 11, 22, 33, 44
    int32_t bufferKeyElements[] = {1, 2, 4, 5};
    auto *bufferKeyElemVec = CreateVector<int32_t>(4, bufferKeyElements);
    auto *bufferKeyArrayVec = new vec::ArrayVector(dataSize, std::shared_ptr<vec::BaseVector>(bufferKeyElemVec));
    bufferKeyArrayVec->SetOffset(0, 0);
    bufferKeyArrayVec->SetSize(0, 1);
    bufferKeyArrayVec->SetOffset(1, 1);
    bufferKeyArrayVec->SetSize(1, 1);
    bufferKeyArrayVec->SetOffset(2, 2);
    bufferKeyArrayVec->SetSize(2, 1);
    bufferKeyArrayVec->SetOffset(3, 3);
    bufferKeyArrayVec->SetSize(3, 1);
    int32_t bufferPayload[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(bufferKeyArrayVec);
    bufferedVecBatch->Append(CreateVector<int32_t>(dataSize, bufferPayload));

    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    addInputRetCode = smjOp->AddBufferedTableInput(bufferedVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    VectorBatch *streamedEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    VectorBatch *bufferedEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    VectorBatch *result = nullptr;
    smjOp->GetOutput(&result);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->GetVectorCount(), 4);
    ASSERT_EQ(result->GetRowCount(), 2);

    // Result columns: left array, left int, right array, right int. Row 0: [1], 111, [1], 11; Row 1: [2], 1, [2], 22
    auto *leftArrCol = reinterpret_cast<vec::ArrayVector *>(result->Get(0));
    auto *leftPayloadCol = reinterpret_cast<vec::Vector<int32_t> *>(result->Get(1));
    auto *rightArrCol = reinterpret_cast<vec::ArrayVector *>(result->Get(2));
    auto *rightPayloadCol = reinterpret_cast<vec::Vector<int32_t> *>(result->Get(3));

    ASSERT_FALSE(leftArrCol->IsNull(0));
    ASSERT_EQ(leftArrCol->GetSize(0), 1);
    std::unique_ptr<vec::BaseVector> slice0(leftArrCol->GetValue(0));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(slice0.get())->GetValue(0), 1);
    ASSERT_EQ(leftPayloadCol->GetValue(0), 111);
    ASSERT_FALSE(rightArrCol->IsNull(0));
    ASSERT_EQ(rightArrCol->GetSize(0), 1);
    std::unique_ptr<vec::BaseVector> rightSlice0(rightArrCol->GetValue(0));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightSlice0.get())->GetValue(0), 1);
    ASSERT_EQ(rightPayloadCol->GetValue(0), 11);

    ASSERT_FALSE(leftArrCol->IsNull(1));
    ASSERT_EQ(leftArrCol->GetSize(1), 1);
    std::unique_ptr<vec::BaseVector> slice1(leftArrCol->GetValue(1));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(slice1.get())->GetValue(0), 2);
    ASSERT_EQ(leftPayloadCol->GetValue(1), 1);
    ASSERT_FALSE(rightArrCol->IsNull(1));
    ASSERT_EQ(rightArrCol->GetSize(1), 1);
    std::unique_ptr<vec::BaseVector> rightSlice1(rightArrCol->GetValue(1));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightSlice1.get())->GetValue(0), 2);
    ASSERT_EQ(rightPayloadCol->GetValue(1), 22);

    VectorHelper::FreeVecBatch(result);
    omniruntime::op::Operator::DeleteOperator(smjOp);
}

// Full SMJ operator test with struct (ROW<INT, INT>) as join key and output column.
// Streamed keys (1,1), (1,2), (2,0), (3,0); buffered keys (1,1), (2,0), (4,0), (5,0). Inner match: (1,1) and (2,0) -> 2 rows.
TEST(NativeSortMergeJoinTest, TestSmjFullOperatorStructKey)
{
    using namespace omniruntime::type;
    std::string blank = "";
    auto *smjOp = new SortMergeJoinOperator(JoinType::OMNI_JOIN_TYPE_INNER, blank);

    std::vector<DataTypePtr> fieldTypes = { IntType(), IntType() };
    auto structType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0", "c1"});
    std::vector<DataTypePtr> streamTypesVector = { structType, IntType() };
    DataTypes streamedTblTypes(streamTypesVector);
    std::vector<int32_t> streamedKeysCols = {0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    smjOp->ConfigStreamedTblInfo(streamedTblTypes, streamedKeysCols, streamedOutputCols, streamedTblTypes.GetSize());

    std::vector<DataTypePtr> bufferTypesVector = { structType, IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    std::vector<int32_t> bufferedKeysCols = {0};
    std::vector<int32_t> bufferedOutputCols = {0, 1};
    smjOp->ConfigBufferedTblInfo(bufferedTblTypes, bufferedKeysCols, bufferedOutputCols, bufferedTblTypes.GetSize());
    smjOp->InitScannerAndResultBuilder(nullptr);

    const int32_t dataSize = 4;
    // Streamed: key (1,1), (1,2), (2,0), (3,0); payload 111, 11, 1, 0
    int32_t streamKeyCol0[] = {1, 1, 2, 3};
    int32_t streamKeyCol1[] = {1, 2, 0, 0};
    auto *streamKeyChild0 = CreateVector<int32_t>(dataSize, streamKeyCol0);
    auto *streamKeyChild1 = CreateVector<int32_t>(dataSize, streamKeyCol1);
    auto *streamKeyRowVec = new vec::RowVector(dataSize);
    streamKeyRowVec->AddChild(std::shared_ptr<vec::BaseVector>(streamKeyChild0));
    streamKeyRowVec->AddChild(std::shared_ptr<vec::BaseVector>(streamKeyChild1));
    int32_t streamedPayload[] = {111, 11, 1, 0};
    auto *streamedVecBatch = new VectorBatch(dataSize);
    streamedVecBatch->Append(streamKeyRowVec);
    streamedVecBatch->Append(CreateVector<int32_t>(dataSize, streamedPayload));

    // Buffered: key (1,1), (2,0), (4,0), (5,0); payload 11, 22, 33, 44
    int32_t bufferKeyCol0[] = {1, 2, 4, 5};
    int32_t bufferKeyCol1[] = {1, 0, 0, 0};
    auto *bufferKeyChild0 = CreateVector<int32_t>(dataSize, bufferKeyCol0);
    auto *bufferKeyChild1 = CreateVector<int32_t>(dataSize, bufferKeyCol1);
    auto *bufferKeyRowVec = new vec::RowVector(dataSize);
    bufferKeyRowVec->AddChild(std::shared_ptr<vec::BaseVector>(bufferKeyChild0));
    bufferKeyRowVec->AddChild(std::shared_ptr<vec::BaseVector>(bufferKeyChild1));
    int32_t bufferPayload[] = {11, 22, 33, 44};
    auto *bufferedVecBatch = new VectorBatch(dataSize);
    bufferedVecBatch->Append(bufferKeyRowVec);
    bufferedVecBatch->Append(CreateVector<int32_t>(dataSize, bufferPayload));

    int32_t addInputRetCode = smjOp->AddStreamedTableInput(streamedVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    addInputRetCode = smjOp->AddBufferedTableInput(bufferedVecBatch);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_STREAM_TBL_DATA));

    VectorBatch *streamedEof = CreateEmptyVectorBatch(streamedTblTypes);
    addInputRetCode = smjOp->AddStreamedTableInput(streamedEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode),
        static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_NEED_ADD_BUFFER_TBL_DATA));

    VectorBatch *bufferedEof = CreateEmptyVectorBatch(bufferedTblTypes);
    addInputRetCode = smjOp->AddBufferedTableInput(bufferedEof);
    ASSERT_EQ(DecodeAddFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_SCAN_FINISH));
    ASSERT_EQ(DecodeFetchFlag(addInputRetCode), static_cast<int32_t>(SortMergeJoinAddInputCode::SMJ_FETCH_JOIN_DATA));

    VectorBatch *result = nullptr;
    smjOp->GetOutput(&result);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->GetVectorCount(), 4);
    ASSERT_EQ(result->GetRowCount(), 2);

    // Result columns: left struct, left int, right struct, right int. Row 0: (1,1), 111, (1,1), 11; Row 1: (2,0), 1, (2,0), 22
    auto *leftStructCol = reinterpret_cast<vec::RowVector *>(result->Get(0));
    auto *leftPayloadCol = reinterpret_cast<vec::Vector<int32_t> *>(result->Get(1));
    auto *rightStructCol = reinterpret_cast<vec::RowVector *>(result->Get(2));
    auto *rightPayloadCol = reinterpret_cast<vec::Vector<int32_t> *>(result->Get(3));

    ASSERT_FALSE(leftStructCol->IsNull(0));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(leftStructCol->ChildAt(0).get())->GetValue(0), 1);
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(leftStructCol->ChildAt(1).get())->GetValue(0), 1);
    ASSERT_EQ(leftPayloadCol->GetValue(0), 111);
    ASSERT_FALSE(rightStructCol->IsNull(0));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightStructCol->ChildAt(0).get())->GetValue(0), 1);
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightStructCol->ChildAt(1).get())->GetValue(0), 1);
    ASSERT_EQ(rightPayloadCol->GetValue(0), 11);

    ASSERT_FALSE(leftStructCol->IsNull(1));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(leftStructCol->ChildAt(0).get())->GetValue(1), 2);
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(leftStructCol->ChildAt(1).get())->GetValue(1), 0);
    ASSERT_EQ(leftPayloadCol->GetValue(1), 1);
    ASSERT_FALSE(rightStructCol->IsNull(1));
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightStructCol->ChildAt(0).get())->GetValue(1), 2);
    ASSERT_EQ(reinterpret_cast<vec::Vector<int32_t> *>(rightStructCol->ChildAt(1).get())->GetValue(1), 0);
    ASSERT_EQ(rightPayloadCol->GetValue(1), 22);

    VectorHelper::FreeVecBatch(result);
    omniruntime::op::Operator::DeleteOperator(smjOp);
}
}
