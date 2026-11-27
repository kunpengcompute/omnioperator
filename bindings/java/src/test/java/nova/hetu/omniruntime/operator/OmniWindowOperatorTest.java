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

import static nova.hetu.omniruntime.constants.OmniWindowFrameBoundType.OMNI_FRAME_BOUND_CURRENT_ROW;
import static nova.hetu.omniruntime.constants.OmniWindowFrameBoundType.OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING;
import static nova.hetu.omniruntime.constants.OmniWindowFrameType.OMNI_FRAME_TYPE_RANGE;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.constants.OmniWindowFrameBoundType;
import nova.hetu.omniruntime.constants.OmniWindowFrameType;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.window.OmniWindowOperatorFactory;
import nova.hetu.omniruntime.operator.window.OmniWindowOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * The type Omni window operator test.
 *
 * @since 2021-6-4
 */
public class OmniWindowOperatorTest {
    /**
     * The Total page count.
     */
    int totalPageCount = 1;

    /**
     * The Page distinct count.
     */
    int pageDistinctCount = 4;

    /**
     * The Page distinct value repeat count.
     */
    int pageDistinctValueRepeatCount = 5000;

    /**
     * test window performance whether with jit or not.
     */
    @Test
    public void testWindowComparePref() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] outputChannels = {0, 1};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1, -1, -1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1, -1, -1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {1};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        int expectedPositions = 10000;
        int[] argumentChannels = {1, -1};
        DataType[] windowFunctionReturnType = {LongDataType.LONG, LongDataType.LONG};

        OmniWindowOperatorFactory windowOperatorFactoryWithoutJit = new OmniWindowOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, expectedPositions, argumentChannels, windowFunctionReturnType,
                windowFrameTypes, windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes,
                winddowFrameEndChannels, new OperatorConfig());
        OmniOperator windowOperatorWithoutJit = windowOperatorFactoryWithoutJit.createOperator();
        ImmutableList<VecBatch> vecsWithoutJit = buildVecs();

        long start = System.currentTimeMillis();
        for (VecBatch vec : vecsWithoutJit) {
            windowOperatorWithoutJit.addInput(vec);
        }
        Iterator<VecBatch> outputWithoutJit = windowOperatorWithoutJit.getOutput();
        long end = System.currentTimeMillis();
        System.out.println("Window without jit use " + (end - start) + " ms.");

        OmniWindowOperatorFactory windowOperatorFactoryWithJit = new OmniWindowOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, expectedPositions, argumentChannels, windowFunctionReturnType,
                windowFrameTypes, windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes,
                winddowFrameEndChannels, new OperatorConfig());
        OmniOperator windowOperatorWithJit = windowOperatorFactoryWithJit.createOperator();
        ImmutableList<VecBatch> vecsWithJit = buildVecs();

        start = System.currentTimeMillis();
        for (VecBatch vec : vecsWithJit) {
            windowOperatorWithJit.addInput(vec);
        }
        Iterator<VecBatch> outputWithJit = windowOperatorWithJit.getOutput();
        end = System.currentTimeMillis();
        System.out.println("Window with jit use " + (end - start) + " ms.");

        while (outputWithoutJit.hasNext() && outputWithJit.hasNext()) {
            VecBatch resultWithoutJit = outputWithoutJit.next();
            VecBatch resultWithJit = outputWithJit.next();
            assertVecBatchEquals(resultWithoutJit, resultWithJit);
            freeVecBatch(resultWithoutJit);
            freeVecBatch(resultWithJit);
        }

        windowOperatorWithoutJit.close();
        windowOperatorWithJit.close();
        windowOperatorFactoryWithoutJit.close();
        windowOperatorFactoryWithJit.close();
    }

    /**
     * Test rank.
     */
    @Test
    public void testRank() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] outputChannels = {0, 1};
        FunctionType[] windowFunction = {FunctionType.OMNI_WINDOW_TYPE_RANK};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {1};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        int expectedPositions = 10000;
        int[] argumentChannels = {};
        DataType[] windowFunctionReturnType = {LongDataType.LONG};
        OmniWindowOperatorFactory omniWindowOperatorFactory = new OmniWindowOperatorFactory(sourceTypes, outputChannels,
                windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder, sortNullFirsts,
                preSortedChannelPrefix, expectedPositions, argumentChannels, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
        OmniOperator omniOperator = omniWindowOperatorFactory.createOperator();

        VecBatch vecBatch = buildData();

        omniOperator.addInput(vecBatch);
        Iterator<VecBatch> output = omniOperator.getOutput();
        if (output.hasNext()) {
            VecBatch outputVecBatch = output.next();
            Vec[] vectors = outputVecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 1);
            assertEquals(((LongVec) vectors[0]).get(1), 1);
            assertEquals(((LongVec) vectors[0]).get(2), 1);
            assertEquals(((LongVec) vectors[0]).get(3), 2);
            assertEquals(((LongVec) vectors[0]).get(4), 2);
            assertEquals(((LongVec) vectors[1]).get(0), 2);
            assertEquals(((LongVec) vectors[1]).get(1), 4);
            assertEquals(((LongVec) vectors[1]).get(2), 6);
            assertEquals(((LongVec) vectors[1]).get(3), -1);
            assertEquals(((LongVec) vectors[1]).get(4), 5);
            assertEquals(((LongVec) vectors[2]).get(0), 1);
            assertEquals(((LongVec) vectors[2]).get(1), 2);
            assertEquals(((LongVec) vectors[2]).get(2), 3);
            assertEquals(((LongVec) vectors[2]).get(3), 1);
            assertEquals(((LongVec) vectors[2]).get(4), 2);
            freeVecBatch(outputVecBatch);
        }

        omniOperator.close();
        omniWindowOperatorFactory.close();
    }

    /**
     * Test count.
     */
    @Test
    public void testCount() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] outputChannels = {0, 1};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1, -1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1, -1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {1};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        int expectedPositions = 10000;
        int[] argumentChannels = {1, -1};
        DataType[] windowFunctionReturnType = {LongDataType.LONG, LongDataType.LONG};
        OmniWindowOperatorFactory omniWindowOperatorFactory = new OmniWindowOperatorFactory(sourceTypes, outputChannels,
                windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder, sortNullFirsts,
                preSortedChannelPrefix, expectedPositions, argumentChannels, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
        OmniOperator omniOperator = omniWindowOperatorFactory.createOperator();

        VecBatch vecBatch = buildData();
        vecBatch.getVectors()[1].setNull(2);

        omniOperator.addInput(vecBatch);
        Iterator<VecBatch> output = omniOperator.getOutput();
        if (output.hasNext()) {
            VecBatch outputVecBatch = output.next();
            Object[][] expectedData = {{1L, 1L, 1L, 2L, 2L}, {2L, 6L, null, -1L, 5L}, {1L, 2L, 2L, 1L, 2L},
                    {1L, 2L, 3L, 1L, 2L}};

            assertEquals(outputVecBatch.getRowCount(), 5);
            assertEquals(outputVecBatch.getVectors().length, 4);
            assertVecBatchEquals(outputVecBatch, expectedData);

            freeVecBatch(outputVecBatch);
        }

        omniOperator.close();
        omniWindowOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1, -1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1, -1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {1};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        int expectedPositions = 10000;
        int[] argumentChannels = {1, -1};
        DataType[] windowFunctionReturnType = {LongDataType.LONG, LongDataType.LONG};
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] outputChannels = {0, 1};
        FactoryContext factory1 = new FactoryContext(sourceTypes, outputChannels, windowFunction, partitionChannels,
                preGroupedChannels, sortChannels, sortOrder, sortNullFirsts, preSortedChannelPrefix, expectedPositions,
                argumentChannels, windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes,
                winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, outputChannels, windowFunction, partitionChannels,
                preGroupedChannels, sortChannels, sortOrder, sortNullFirsts, preSortedChannelPrefix, expectedPositions,
                argumentChannels, windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes,
                winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels, new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    private VecBatch buildData() {
        int rowNum = 5;
        LongVec longVec1 = new LongVec(rowNum);
        LongVec longVec2 = new LongVec(rowNum);
        longVec1.set(0, 2);
        longVec1.set(1, 1);
        longVec1.set(2, 1);
        longVec1.set(3, 2);
        longVec1.set(4, 1);
        longVec2.set(0, -1);
        longVec2.set(1, 2);
        longVec2.set(2, 4);
        longVec2.set(3, 5);
        longVec2.set(4, 6);

        List<Vec> columns = new ArrayList<>();
        columns.add(longVec1);
        columns.add(longVec2);
        return new VecBatch(columns, rowNum);
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
