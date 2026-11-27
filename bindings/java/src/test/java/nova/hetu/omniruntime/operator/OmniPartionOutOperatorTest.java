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

package nova.hetu.omniruntime.operator;

import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.partitionedoutput.OmniPartitionedOutPutOperatorFactory;
import nova.hetu.omniruntime.operator.partitionedoutput.OmniPartitionedOutPutOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.CharDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.util.TestUtils;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.OptionalInt;

/**
 * The type Omni partition out operators test.
 *
 * @since 2021-6-30
 */
public class OmniPartionOutOperatorTest {
    @Test(enabled = false)
    public void testPartionOut() {
        OptionalInt nullChannel = OptionalInt.empty();

        int[] partitionChannels = {0};
        int partitionCount = 1;
        int[] bucketToPartition = {0};
        DataType[] hashChannelTypes = {VarcharDataType.VARCHAR};
        int[] hashChannels = {0};

        DataType[] buildTypes = {new VarcharDataType(3), new VarcharDataType(3)};
        Object[][] buildDatas = {{"abc", "de", "f"}, {"def", "bc", "a"}};
        VecBatch vecBatch = createVecBatch(buildTypes, buildDatas);
        DataType[] sourceTypes = {VarcharDataType.VARCHAR};

        OmniPartitionedOutPutOperatorFactory omniPartitionedOutPutOperatorFactory = new OmniPartitionedOutPutOperatorFactory(
                sourceTypes, false, nullChannel, partitionChannels, partitionCount, bucketToPartition, false,
                hashChannelTypes, hashChannels);
        OmniOperator omniOperator = omniPartitionedOutPutOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        assertEquals(resultList.get(0).getRowCount(), 3);

        Object[][] expectedDatas = {{"abc", "de", "f"}};
        assertVecBatchEquals(resultList.get(0), expectedDatas);
        TestUtils.freeVecBatch(resultList.get(0));
        omniOperator.close();
        omniPartitionedOutPutOperatorFactory.close();
    }

    @Test(enabled = false)
    public void testPartionOutCache() {
        OptionalInt nullChannel = OptionalInt.empty();
        int[] partitionChannels = {0};
        int partitionCount = 1;
        int[] bucketToPartition = {0};
        DataType[] hashChannelTypes = {VarcharDataType.VARCHAR};
        int[] hashChannels = {0};

        DataType[] buildTypes = {new VarcharDataType(3), new VarcharDataType(3)};
        Object[][] buildDatas = {{"abc", "de", null}, {"abc", "de", null}};
        VecBatch vecBatch = createVecBatch(buildTypes, buildDatas);
        DataType[] sourceTypes = {VarcharDataType.VARCHAR};

        OmniPartitionedOutPutOperatorFactory omniPartitionedOutPutOperatorFactory = new OmniPartitionedOutPutOperatorFactory(
                sourceTypes, false, nullChannel, partitionChannels, partitionCount, bucketToPartition, false,
                hashChannelTypes, hashChannels);
        OmniOperator omniOperator = omniPartitionedOutPutOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        assertEquals(resultList.get(0).getRowCount(), 3);

        Object[][] expectedDatas = {{"abc", "de", null}};
        assertVecBatchEquals(resultList.get(0), expectedDatas);
        TestUtils.freeVecBatch(resultList.get(0));
        omniOperator.close();
        omniPartitionedOutPutOperatorFactory.close();
    }

    @Test(enabled = false)
    public void testPartionOutChar() {
        OptionalInt nullChannel = OptionalInt.empty();
        int[] partitionChannels = {0};
        int partitionCount = 1;
        int[] bucketToPartition = {0};
        DataType[] hashChannelTypes = {CharDataType.CHAR};
        int[] hashChannels = {0};

        DataType[] buildTypes = {new CharDataType(3), new CharDataType(3)};
        Object[][] buildDatas = {{"abc", "de", "f"}, {"def", "bc", "a"}};
        VecBatch vecBatch = createVecBatch(buildTypes, buildDatas);
        DataType[] sourceTypes = {CharDataType.CHAR};

        OmniPartitionedOutPutOperatorFactory omniPartitionedOutPutOperatorFactory = new OmniPartitionedOutPutOperatorFactory(
                sourceTypes, false, nullChannel, partitionChannels, partitionCount, bucketToPartition, false,
                hashChannelTypes, hashChannels);
        OmniOperator omniOperator = omniPartitionedOutPutOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        assertEquals(resultList.get(0).getRowCount(), 3);

        Object[][] expectedDatas = {{"abc", "de", "f"}};
        assertVecBatchEquals(resultList.get(0), expectedDatas);
        TestUtils.freeVecBatch(resultList.get(0));
        omniOperator.close();
        omniPartitionedOutPutOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {CharDataType.CHAR};
        OptionalInt nullChannel = OptionalInt.empty();
        int[] partitionChannels = {0};
        int partitionCount = 1;
        int[] bucketToPartition = {0};
        DataType[] hashChannelTypes = {CharDataType.CHAR};
        int[] hashChannels = {0};

        FactoryContext factory1 = new FactoryContext(sourceTypes, false, nullChannel, partitionChannels, partitionCount,
                bucketToPartition, false, hashChannelTypes, hashChannels, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, false, nullChannel, partitionChannels, partitionCount,
                bucketToPartition, false, hashChannelTypes, hashChannels, new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }
}
