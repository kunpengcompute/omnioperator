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
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.topn.OmniTopNOperatorFactory;
import nova.hetu.omniruntime.operator.topn.OmniTopNOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.util.TestUtils;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

/**
 * The type Omni TopN operator test.
 *
 * @since 2021-7-31
 */
public class OmniTopNOperatorTest {
    /**
     * The Total page count.
     */
    int totalPageCount = 10;

    /**
     * The Page distinct count.
     */
    int pageDistinctCount = 4;

    /**
     * The Page distinct value repeat count.
     */
    int pageDistinctValueRepeatCount = 2500;

    /**
     * test topN performance whether with jit or not.
     */
    @Test
    public void testTopNComparePref() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int limitN = 10;
        int[] outputCols = {0, 1};
        String[] sortCols = {"#0", "#1"};
        int[] sotrAscendings = {1, 1};
        int[] sortNullFirsts = {0, 0};

        OmniTopNOperatorFactory topNOperatorFactoryWithoutJit = new OmniTopNOperatorFactory(sourceTypes, limitN,
                sortCols, sotrAscendings, sortNullFirsts, new OperatorConfig());
        OmniOperator topNOperatorWithoutJit = topNOperatorFactoryWithoutJit.createOperator();
        ImmutableList<VecBatch> vecsWithoutJit = buildVecs();

        long start = System.currentTimeMillis();
        for (VecBatch vec : vecsWithoutJit) {
            topNOperatorWithoutJit.addInput(vec);
        }
        Iterator<VecBatch> outputWithoutJit = topNOperatorWithoutJit.getOutput();
        long end = System.currentTimeMillis();
        System.out.println("TopN without jit use " + (end - start) + " ms.");

        OmniTopNOperatorFactory topNOperatorFactoryWithJit = new OmniTopNOperatorFactory(sourceTypes, limitN, sortCols,
                sotrAscendings, sortNullFirsts, new OperatorConfig());
        OmniOperator topNOperatorWithJit = topNOperatorFactoryWithJit.createOperator();
        ImmutableList<VecBatch> vecsWithJit = buildVecs();

        start = System.currentTimeMillis();
        for (VecBatch vec : vecsWithJit) {
            topNOperatorWithJit.addInput(vec);
        }
        Iterator<VecBatch> outputWithJit = topNOperatorWithJit.getOutput();
        end = System.currentTimeMillis();
        System.out.println("TopN with jit use " + (end - start) + " ms.");

        while (outputWithoutJit.hasNext()) {
            VecBatch resultWithoutJit = outputWithoutJit.next();
            VecBatch resultWithJit = outputWithJit.next();
            assertVecBatchEquals(resultWithoutJit, resultWithJit);
            freeVecBatch(resultWithoutJit);
            freeVecBatch(resultWithJit);
        }

        topNOperatorWithoutJit.close();
        topNOperatorWithJit.close();
        topNOperatorFactoryWithoutJit.close();
        topNOperatorFactoryWithJit.close();
    }

    @Test
    public void testOneColumn() {
        int rowSize = 6;
        int expectedRowSize = 5;
        long[] rawData = {0, 1, 2, 0, 1, 2};
        LongVec longVec = new LongVec(6);
        longVec.put(rawData, 0, 0, rowSize);
        ArrayList<Vec> longVecs = new ArrayList<>();
        longVecs.add(longVec);

        DataType[] sourceTypes = {LongDataType.LONG};
        String[] sortCols = {"#0"};
        int[] sortAsc = {0};
        int[] nullFirst = {0};
        OmniTopNOperatorFactory omniTopNOperatorFactory = new OmniTopNOperatorFactory(sourceTypes, expectedRowSize,
                sortCols, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();
        operator.addInput(new VecBatch(longVecs));
        Iterator<VecBatch> output = operator.getOutput();
        VecBatch result = output.next();
        assertEquals(result.getRowCount(), expectedRowSize);
        Vec vector = result.getVectors()[0];
        long[] resultArray = new long[expectedRowSize];
        for (int i = 0; i < vector.getSize(); i++) {
            resultArray[i] = ((LongVec) vector).get(i);
        }
        long[] expectedArray = {2, 2, 1, 1, 0};
        assertEquals(resultArray, expectedArray);

        TestUtils.freeVecBatch(result);

        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testMultipleColumns() {
        int rowSize = 6;
        int[] rawData1 = {0, 1, 2, 0, 1, 2};
        long[] rawData2 = {0, 1, 2, 3, 4, 5};
        double[] rawData3 = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
        IntVec vec1 = new IntVec(6);
        LongVec vec2 = new LongVec(6);
        DoubleVec vec3 = new DoubleVec(6);
        vec1.put(rawData1, 0, 0, rowSize);
        vec2.put(rawData2, 0, 0, rowSize);
        vec3.put(rawData3, 0, 0, rowSize);
        ArrayList<Vec> longVecs = new ArrayList<>();
        longVecs.add(vec1);
        longVecs.add(vec2);
        longVecs.add(vec3);

        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        String[] sortCols = {"#0", "#1"};
        int[] sortAsc = {1, 1};
        int[] nullFirst = {0, 0};
        int expectedRowSize = 5;
        OmniTopNOperatorFactory omniTopNOperatorFactory = new OmniTopNOperatorFactory(sourceTypes, expectedRowSize,
                sortCols, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();
        operator.addInput(new VecBatch(longVecs));
        Iterator<VecBatch> output = operator.getOutput();
        VecBatch result = output.next();
        assertEquals(result.getRowCount(), expectedRowSize);
        Vec[] vector = result.getVectors();
        int[] resultArray1 = new int[expectedRowSize];
        long[] resultArray2 = new long[expectedRowSize];
        double[] resultArray3 = new double[expectedRowSize];
        for (int i = 0; i < vector[0].getSize(); i++) {
            resultArray1[i] = ((IntVec) vector[0]).get(i);
        }
        for (int i = 0; i < vector[1].getSize(); i++) {
            resultArray2[i] = ((LongVec) vector[1]).get(i);
        }
        for (int i = 0; i < vector[2].getSize(); i++) {
            resultArray3[i] = ((DoubleVec) vector[2]).get(i);
        }
        int[] expectedArray1 = {0, 0, 1, 1, 2};
        long[] expectedArray2 = {0, 3, 1, 4, 2};
        double[] expectedArray3 = {6.6, 3.3, 5.5, 2.2, 4.4};
        assertEquals(resultArray1, expectedArray1);
        assertEquals(resultArray2, expectedArray2);
        assertEquals(resultArray3, expectedArray3);

        TestUtils.freeVecBatch(result);

        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testTopNWithOffset() {
        int rowSize = 6;
        int[] rawData1 = {0, 1, 2, 0, 1, 2};
        double[] rawData2 = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
        IntVec vec1 = new IntVec(6);
        DoubleVec vec2 = new DoubleVec(6);
        vec1.put(rawData1, 0, 0, rowSize);
        vec2.put(rawData2, 0, 0, rowSize);
        ArrayList<Vec> longVecs = new ArrayList<>();
        longVecs.add(vec1);
        longVecs.add(vec2);

        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        String[] sortCols = {"#0", "#1"};
        int[] sortAsc = {1, 0};
        int[] nullFirst = {0, 0};
        int limitSize = 5;
        int expectedRowSize = limitSize - 2;
        OmniTopNOperatorFactory omniTopNOperatorFactory = new OmniTopNOperatorFactory(sourceTypes, limitSize, 2,
                sortCols, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();
        operator.addInput(new VecBatch(longVecs));
        Iterator<VecBatch> output = operator.getOutput();
        VecBatch result = output.next();
        assertEquals(result.getRowCount(), expectedRowSize);
        Vec[] vector = result.getVectors();
        int[] resultArray1 = new int[expectedRowSize];
        double[] resultArray2 = new double[expectedRowSize];
        for (int i = 0; i < vector[0].getSize(); i++) {
            if (vector[0] instanceof IntVec) {
                resultArray1[i] = ((IntVec) vector[0]).get(i);
            }
        }
        for (int i = 0; i < vector[1].getSize(); i++) {
            if (vector[1] instanceof DoubleVec) {
                resultArray2[i] = ((DoubleVec) vector[1]).get(i);
            }
        }

        int[] expectedArray1 = {1, 1, 2};
        double[] expectedArray2 = {5.5, 2.2, 4.4};
        assertEquals(resultArray1, expectedArray1);
        assertEquals(resultArray2, expectedArray2);

        TestUtils.freeVecBatch(result);

        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testTopNDescMultiColumnSortColumn1() {
        int rowSize = 6;
        int[] rawData1 = {0, 1, 2, 0, 1, 2};
        long[] rawData2 = {0, 1, 2, 3, 4, 5};
        double[] rawData3 = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
        IntVec vec1 = new IntVec(6);
        LongVec vec2 = new LongVec(6);
        DoubleVec vec3 = new DoubleVec(6);
        vec1.put(rawData1, 0, 0, rowSize);
        vec2.put(rawData2, 0, 0, rowSize);
        vec3.put(rawData3, 0, 0, rowSize);
        ArrayList<Vec> longVecs = new ArrayList<>();
        longVecs.add(vec1);
        longVecs.add(vec2);
        longVecs.add(vec3);

        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        String[] sortCols = {"#1"};
        int[] sortAsc = {0};
        int[] nullFirst = {0};
        int expectedRowSize = 5;
        OmniTopNOperatorFactory omniTopNOperatorFactory = new OmniTopNOperatorFactory(sourceTypes, expectedRowSize,
                sortCols, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();
        operator.addInput(new VecBatch(longVecs));
        Iterator<VecBatch> output = operator.getOutput();
        VecBatch result = output.next();
        assertEquals(result.getRowCount(), expectedRowSize);
        Vec[] vector = result.getVectors();
        List<Integer> resultList1 = new ArrayList<>();
        List<Long> resultList2 = new ArrayList<>();
        List<Double> resultList3 = new ArrayList<>();

        for (int i = 0; i < vector[0].getSize(); i++) {
            resultList1.add(((IntVec) vector[0]).get(i));
        }
        for (int i = 0; i < vector[1].getSize(); i++) {
            resultList2.add(((LongVec) vector[1]).get(i));
        }
        for (int i = 0; i < vector[2].getSize(); i++) {
            resultList3.add(((DoubleVec) vector[2]).get(i));
        }

        ArrayList<Integer> expectList1 = new ArrayList<>(Arrays.asList(2, 1, 0, 2, 1));
        ArrayList<Long> expectList2 = new ArrayList<>(Arrays.asList(5L, 4L, 3L, 2L, 1L));
        ArrayList<Double> expectList3 = new ArrayList<>(Arrays.asList(1.1, 2.2, 3.3, 4.4, 5.5));
        assertEquals(expectList1, resultList1);
        assertEquals(expectList2, resultList2);
        assertEquals(expectList3, resultList3);

        TestUtils.freeVecBatch(result);

        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        String[] sortCols = {"#1"};
        int[] sortAsc = {0};
        int[] nullFirst = {0};
        int expectedRowSize = 5;
        FactoryContext factory1 = new FactoryContext(sourceTypes, expectedRowSize, 0, sortCols, sortAsc, nullFirst,
                new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, expectedRowSize, 0, sortCols, sortAsc, nullFirst,
                new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    private ImmutableList<VecBatch> buildVecs() {
        ImmutableList.Builder<VecBatch> vecBatchList = ImmutableList.builder();
        int positionCount = pageDistinctCount * pageDistinctValueRepeatCount;
        List<Vec> vecs = new ArrayList<>();
        for (int i = 0; i < totalPageCount; i++) {
            LongVec longVec1 = new LongVec(positionCount);
            LongVec longVec2 = new LongVec(positionCount);
            int idx = 0;
            for (int j = 0; j < pageDistinctCount; j++) {
                for (int k = 0; k < pageDistinctValueRepeatCount; k++) {
                    longVec1.set(idx, j);
                    longVec2.set(idx, j);
                    idx++;
                }
            }
            vecs.add(longVec1);
            vecs.add(longVec2);
            VecBatch vecBatch = new VecBatch(new Vec[]{longVec1, longVec2});
            vecBatchList.add(vecBatch);
        }
        return vecBatchList.build();
    }
}
