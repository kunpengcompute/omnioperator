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
#include "gtest/gtest.h"
#include "operator/union/union.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace std;
using namespace omniruntime::TestUtil;

namespace UnionTest {
TEST(NativeOmniUnionOperator, TestUnionByThreeColumn)
{
    // construct input data
    const int32_t dataSize = 6;
    // table1
    int32_t data1[dataSize] = {0, 1, 2, 0, 1, 2};
    double data2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[dataSize] = {6, 5, 4, 3, 2, 1};
    // table2
    int32_t data4[dataSize] = {10, 11, 12, 10, 11, 12};
    double data5[dataSize] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1};
    int16_t data6[dataSize] = {16, 15, 14, 13, 12, 11};
    std::vector<DataTypePtr> types = {IntType(), DoubleType(), ShortType()};
    DataTypes sourceTypes(types);
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);
    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, dataSize, data4, data5, data6);

    UnionOperatorFactory *operatorFactory =
        UnionOperatorFactory::CreateUnionOperatorFactory(sourceTypes, sourceTypes.GetSize(), false);
    UnionOperator *unionOperator = dynamic_cast<UnionOperator *>(CreateTestOperator(operatorFactory));
    unionOperator->AddInput(vecBatch1);
    unionOperator->AddInput(vecBatch2);
    std::vector<VectorBatch *> outputVecBatches;
    while (unionOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        unionOperator->GetOutput(&outputVecBatch);
        outputVecBatches.push_back(outputVecBatch);
    }

    int32_t expData1[dataSize] = {0, 1, 2, 0, 1, 2};
    double expData2[dataSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t expData3[dataSize] = {6, 5, 4, 3, 2, 1};
    int32_t expData4[dataSize] = {10, 11, 12, 10, 11, 12};
    double expData5[dataSize] = {16.6, 15.5, 14.4, 13.3, 12.2, 11.1};
    int16_t expData6[dataSize] = {16, 15, 14, 13, 12, 11};
    VectorBatch *expVecBatch1 = CreateVectorBatch(sourceTypes, dataSize, expData1, expData2, expData3);
    VectorBatch *expVecBatch2 = CreateVectorBatch(sourceTypes, dataSize, expData4, expData5, expData6);

    EXPECT_EQ(outputVecBatches.size(), 2);
    EXPECT_TRUE(VecBatchMatch(outputVecBatches[0], expVecBatch1));
    EXPECT_TRUE(VecBatchMatch(outputVecBatches[1], expVecBatch2));

    VectorHelper::FreeVecBatch(expVecBatch1);
    VectorHelper::FreeVecBatch(expVecBatch2);
    VectorHelper::FreeVecBatches(outputVecBatches);
    omniruntime::op::Operator::DeleteOperator(unionOperator);
    delete operatorFactory;
}
} // namespace UnionTest
