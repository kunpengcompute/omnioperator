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
import static nova.hetu.omniruntime.util.TestUtils.createDictionaryVec;
import static nova.hetu.omniruntime.util.TestUtils.createLongVec;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.union.OmniUnionOperatorFactory;
import nova.hetu.omniruntime.operator.union.OmniUnionOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * The type Omni union operator test.
 *
 * @since 2021-8-11
 */
public class OmniUnionOperatorTest {
    /**
     * Test the correctness of Omni union operator.
     */
    @Test
    public void testUnionByTwoCols() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        Object[][] sourceDatas1 = {{5, 3, 2, 6, 1, 4, 7, 8}, {5.0, 3.0, 2.0, 6.0, 1.0, 4.0, 7.0, 8.0}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);

        Object[][] sourceDatas2 = {{15, 13, 12, 16, 11, 14, 17, 18}, {15.0, 13.0, 12.0, 16.0, 11.0, 14.0, 17.0, 18.0}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas2);

        OmniUnionOperatorFactory unionOperatorFactory = new OmniUnionOperatorFactory(sourceTypes, false);
        OmniOperator unionOperator = unionOperatorFactory.createOperator();
        unionOperator.addInput(vecBatch1);
        unionOperator.addInput(vecBatch2);
        Iterator<VecBatch> results = unionOperator.getOutput();

        Object[][] expectedDatas1 = {{5, 3, 2, 6, 1, 4, 7, 8}, {5.0, 3.0, 2.0, 6.0, 1.0, 4.0, 7.0, 8.0}};
        Object[][] expectedDatas2 = {{15, 13, 12, 16, 11, 14, 17, 18},
                {15.0, 13.0, 12.0, 16.0, 11.0, 14.0, 17.0, 18.0}};

        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        assertEquals(resultList.size(), 2);
        assertVecBatchEquals(resultList.get(0), expectedDatas1);
        assertVecBatchEquals(resultList.get(1), expectedDatas2);

        for (int i = 0; i < resultList.size(); i++) {
            freeVecBatch(resultList.get(i));
        }

        unionOperator.close();
        unionOperatorFactory.close();
    }

    /**
     * Test the correctness of Omni union operator when the data has null value.
     */
    @Test
    public void testUnionByTwoColsWithNulls() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        Object[][] sourceDatas1 = {{null, 3, 2, 6, 1, 4, 7, 8}, {5.0, 3.0, 2.0, 6.0, 1.0, 4.0, null, 8.0}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);

        Object[][] sourceDatas2 = {{15, 13, null, 16, 11, 14, 17, 18},
                {15.0, null, 12.0, 16.0, 11.0, 14.0, 17.0, 18.0}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas2);

        OmniUnionOperatorFactory unionOperatorFactory = new OmniUnionOperatorFactory(sourceTypes, false);
        OmniOperator unionOperator = unionOperatorFactory.createOperator();
        unionOperator.addInput(vecBatch1);
        unionOperator.addInput(vecBatch2);
        Iterator<VecBatch> results = unionOperator.getOutput();

        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        Object[][] expectedDatas1 = {{null, 3, 2, 6, 1, 4, 7, 8}, {5.0, 3.0, 2.0, 6.0, 1.0, 4.0, null, 8.0}};
        Object[][] expectedDatas2 = {{15, 13, null, 16, 11, 14, 17, 18},
                {15.0, null, 12.0, 16.0, 11.0, 14.0, 17.0, 18.0}};

        assertEquals(resultList.size(), 2);
        assertVecBatchEquals(resultList.get(0), expectedDatas1);
        assertVecBatchEquals(resultList.get(1), expectedDatas2);

        for (int i = 0; i < resultList.size(); i++) {
            freeVecBatch(resultList.get(i));
        }
        unionOperator.close();
        unionOperatorFactory.close();
    }

    /**
     * Test the correctness of Omni union operator when the data has dictionary
     * type.
     */
    @Test
    public void testUnionWithDictionaryType() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] ids = {0, 1, 2, 3};

        Object[][] sourceDatas1 = {{1L, null, 3L, null}, {111L, 11L, 333L, 33L}};
        Vec[] vecs1 = new Vec[2];
        vecs1[0] = createLongVec(sourceDatas1[0]);
        vecs1[1] = createDictionaryVec(sourceTypes[1], sourceDatas1[1], ids);
        VecBatch vecBatch1 = new VecBatch(vecs1);

        Object[][] sourceDatas2 = {{null, 2L, null, 4L}, {11L, 22L, 33L, 44L}};
        Vec[] vecs2 = new Vec[2];
        vecs2[0] = createLongVec(sourceDatas2[0]);
        vecs2[1] = createDictionaryVec(sourceTypes[1], sourceDatas2[1], ids);
        VecBatch vecBatch2 = new VecBatch(vecs2);

        OmniUnionOperatorFactory unionOperatorFactory = new OmniUnionOperatorFactory(sourceTypes, false);
        OmniOperator unionOperator = unionOperatorFactory.createOperator();
        unionOperator.addInput(vecBatch1);
        unionOperator.addInput(vecBatch2);
        Iterator<VecBatch> results = unionOperator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (results.hasNext()) {
            resultList.add(results.next());
        }

        Object[][] expectedDatas1 = {{1L, null, 3L, null}, {111L, 11L, 333L, 33L}};
        Object[][] expectedDatas2 = {{null, 2L, null, 4L}, {11L, 22L, 33L, 44L}};

        assertEquals(resultList.size(), 2);
        assertVecBatchEquals(resultList.get(0), expectedDatas1);
        assertVecBatchEquals(resultList.get(1), expectedDatas2);

        for (int i = 0; i < resultList.size(); i++) {
            freeVecBatch(resultList.get(i));
        }
        unionOperator.close();
        unionOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        FactoryContext factory1 = new FactoryContext(sourceTypes, false, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, false, new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }
}
