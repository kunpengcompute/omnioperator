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
import static nova.hetu.omniruntime.constants.OmniWindowFrameBoundType.OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING;
import static nova.hetu.omniruntime.constants.OmniWindowFrameBoundType.OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING;
import static nova.hetu.omniruntime.constants.OmniWindowFrameType.OMNI_FRAME_TYPE_RANGE;
import static nova.hetu.omniruntime.constants.OmniWindowFrameType.OMNI_FRAME_TYPE_ROWS;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.constants.OmniWindowFrameBoundType;
import nova.hetu.omniruntime.constants.OmniWindowFrameType;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.window.OmniWindowWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.window.OmniWindowWithExprOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
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
 * @since 2021-11-3
 */
public class OmniWindowWithExprOperatorTest {
    /**
     * Test iterative output
     */
    @Test
    public void testOutputMultiVecBatch() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        int[] outputChannels = {0, 1, 2};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_MAX};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {0};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        String[] argumentKeys = {omniJsonFourArithmeticExpr("ADD", 3, getOmniJsonFieldReference(3, 2),
                getOmniJsonLiteral(3, false, 50.0))};
        DataType[] windowFunctionReturnType = {DoubleDataType.DOUBLE};
        OmniWindowWithExprOperatorFactory omniWindowOperatorFactory = new OmniWindowWithExprOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, 10000, argumentKeys, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
        OmniOperator omniOperator = omniWindowOperatorFactory.createOperator();

        int column = 4;
        int rowNum = 30000;
        VecBatch vecBatch = new VecBatch(buildDataForOutputMultiVectorBatch(rowNum));
        omniOperator.addInput(vecBatch);

        // the value rowsPerBatch = (1M / 36) + 1
        int rowsPerBatch = 30000;
        Object[][] expectedData1 = new Object[column][rowsPerBatch];
        Object[][] expectedData2 = new Object[column][rowNum - rowsPerBatch];
        buildIterativeExpectedData(expectedData1, expectedData2, rowsPerBatch, rowNum);
        Iterator<VecBatch> outputVecBatch = omniOperator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (outputVecBatch.hasNext()) {
            resultList.add(outputVecBatch.next());
        }

        int totalRowCount = 0;
        for (int i = 0; i < resultList.size(); i++) {
            totalRowCount += resultList.get(i).getRowCount();
        }

        assertEquals(totalRowCount, rowNum);
        assertVecBatchEquals(resultList.get(0), expectedData1);

        for (int i = 0; i < resultList.size(); i++) {
            freeVecBatch(resultList.get(i));
        }

        omniOperator.close();
        omniWindowOperatorFactory.close();
    }

    /**
     * Test max.
     */
    @Test
    public void testMax() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        int[] outputChannels = {0, 1, 2};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_MAX};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {0};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        String[] argumentKeys = {omniJsonFourArithmeticExpr("ADD", 3, getOmniJsonFieldReference(3, 2),
                getOmniJsonLiteral(3, false, 50))};
        DataType[] windowFunctionReturnType = {DoubleDataType.DOUBLE};
        OmniWindowWithExprOperatorFactory omniWindowOperatorFactory = new OmniWindowWithExprOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, 10000, argumentKeys, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
        OmniOperator omniOperator = omniWindowOperatorFactory.createOperator();

        VecBatch vecBatch = buildData();

        omniOperator.addInput(vecBatch);
        Iterator<VecBatch> output = omniOperator.getOutput();
        VecBatch outputVecBatch = output.next();
        Object[][] expectedDatas = {{0, 0, 1, 1, 2, 2}, {8L, 8L, 4L, 1L, 5L, 2L}, {6.6D, 3.3D, 2.2D, 5.5D, 1.1D, 4.4D},
                {56.6D, 56.6D, 52.2D, 55.5D, 51.1D, 54.4D}};
        assertVecBatchEquals(outputVecBatch, expectedDatas);
        freeVecBatch(outputVecBatch);

        omniOperator.close();
        omniWindowOperatorFactory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = ".*EXPRESSION_NOT_SUPPORT.*")
    public void testWindowWithInvalidKeys() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        int[] outputChannels = {0, 1, 2};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_MAX};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {0};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        String[] argumentKeys = {omniFunctionExpr("abc", 3, getOmniJsonFieldReference(3, 2))};
        DataType[] windowFunctionReturnType = {DoubleDataType.DOUBLE};
        OmniWindowWithExprOperatorFactory omniWindowOperatorFactory = new OmniWindowWithExprOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, 10000, argumentKeys, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        int[] outputChannels = {0, 1, 2};
        FunctionType[] windowFunction = {FunctionType.OMNI_AGGREGATION_TYPE_MAX};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_RANGE};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_CURRENT_ROW};
        int[] winddowFrameEndChannels = {-1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {1};
        int[] sortOrder = {0};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        String[] argumentKeys = {omniJsonFourArithmeticExpr("ADD", 3, getOmniJsonFieldReference(3, 2),
                getOmniJsonLiteral(3, false, 50))};
        DataType[] windowFunctionReturnType = {DoubleDataType.DOUBLE};
        FactoryContext factory1 = new FactoryContext(sourceTypes, outputChannels, windowFunction, partitionChannels,
                preGroupedChannels, sortChannels, sortOrder, sortNullFirsts, preSortedChannelPrefix, 10000,
                argumentKeys, windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes,
                winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, outputChannels, windowFunction, partitionChannels,
                preGroupedChannels, sortChannels, sortOrder, sortNullFirsts, preSortedChannelPrefix, 10000,
                argumentKeys, windowFunctionReturnType, windowFrameTypes, windowFrameStartTypes,
                winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels, new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    @Test
    public void testWindowFunctionMix() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE};
        int[] outputChannels = {0, 1, 2};
        FunctionType[] windowFunction = {FunctionType.OMNI_WINDOW_TYPE_RANK, FunctionType.OMNI_AGGREGATION_TYPE_AVG};
        OmniWindowFrameType[] windowFrameTypes = {OMNI_FRAME_TYPE_ROWS, OMNI_FRAME_TYPE_ROWS};
        OmniWindowFrameBoundType[] windowFrameStartTypes = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
        int[] winddowFrameStartChannels = {-1, -1};
        OmniWindowFrameBoundType[] windowFrameEndTypes = {OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING,
                OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING};
        int[] winddowFrameEndChannels = {-1, -1};
        int[] partitionChannels = {0};
        int[] preGroupedChannels = {};
        int[] sortChannels = {2};
        int[] sortOrder = {1};
        int[] sortNullFirsts = {0};
        int preSortedChannelPrefix = 0;
        String[] argumentKeys = {omniFunctionExpr("abs", 2, getOmniJsonFieldReference(2, 1))};
        DataType[] windowFunctionReturnType = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        OmniWindowWithExprOperatorFactory omniWindowOperatorFactory = new OmniWindowWithExprOperatorFactory(sourceTypes,
                outputChannels, windowFunction, partitionChannels, preGroupedChannels, sortChannels, sortOrder,
                sortNullFirsts, preSortedChannelPrefix, 10000, argumentKeys, windowFunctionReturnType, windowFrameTypes,
                windowFrameStartTypes, winddowFrameStartChannels, windowFrameEndTypes, winddowFrameEndChannels);
        OmniOperator omniOperator = omniWindowOperatorFactory.createOperator();

        VecBatch vecBatch = buildData();

        omniOperator.addInput(vecBatch);
        Iterator<VecBatch> output = omniOperator.getOutput();
        VecBatch outputVecBatch = output.next();

        Object[][] expectedDatas = {{0, 0, 1, 1, 2, 2}, {8L, 8L, 4L, 1L, 5L, 2L}, {3.3D, 6.6D, 2.2D, 5.5D, 1.1D, 4.4D},
                {1, 2, 1, 2, 1, 2}, {8.0D, 8.0D, 2.5D, 2.5D, 3.5D, 3.5D}};
        assertVecBatchEquals(outputVecBatch, expectedDatas);
        freeVecBatch(outputVecBatch);

        omniOperator.close();
        omniWindowOperatorFactory.close();
    }

    private VecBatch buildData() {
        int rowNum = 6;
        IntVec vec1 = new IntVec(rowNum);
        vec1.set(0, 0);
        vec1.set(1, 1);
        vec1.set(2, 2);
        vec1.set(3, 0);
        vec1.set(4, 1);
        vec1.set(5, 2);
        LongVec vec2 = new LongVec(rowNum);
        vec2.set(0, 8);
        vec2.set(1, 1);
        vec2.set(2, 2);
        vec2.set(3, 8);
        vec2.set(4, 4);
        vec2.set(5, 5);
        DoubleVec vec3 = new DoubleVec(rowNum);
        vec3.set(0, 6.6);
        vec3.set(1, 5.5);
        vec3.set(2, 4.4);
        vec3.set(3, 3.3);
        vec3.set(4, 2.2);
        vec3.set(5, 1.1);
        List<Vec> columns = new ArrayList<>();
        columns.add(vec1);
        columns.add(vec2);
        columns.add(vec3);
        return new VecBatch(columns);
    }

    private List<Vec> buildDataForOutputMultiVectorBatch(int rowNum) {
        IntVec c1 = new IntVec(rowNum);
        for (int i = 0; i < rowNum / 2; i++) {
            c1.set(i, i);
        }

        for (int i = rowNum / 2; i < rowNum; i++) {
            c1.set(i, i - rowNum / 2);
        }

        LongVec c2 = new LongVec(rowNum);
        DoubleVec c3 = new DoubleVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c2.set(i, i);
            c3.set(i, i);
        }

        List<Vec> columns = new ArrayList<>();
        columns.add(c1);
        columns.add(c2);
        columns.add(c3);

        return columns;
    }

    private void buildIterativeExpectedData(Object[][] expectedData1, Object[][] expectedData2, int maxRowCount,
                                            int expectedRowSize) {
        int offset1 = maxRowCount / 2;
        int offset2 = expectedRowSize / 2;
        for (int i = 0; i < offset1; i++) {
            expectedData1[0][i * 2] = i;
            expectedData1[0][i * 2 + 1] = i;
            expectedData1[1][i * 2] = (long) (i + offset2);
            expectedData1[1][i * 2 + 1] = (long) i;
            expectedData1[2][i * 2] = (double) (i + offset2);
            expectedData1[2][i * 2 + 1] = (double) i;
            expectedData1[3][i * 2] = (double) (i + offset2 + 50);
            expectedData1[3][i * 2 + 1] = (double) (i + offset2 + 50);
        }

        int offset3 = offset1 + offset2;
        int offset4 = (expectedRowSize - maxRowCount) / 2;
        for (int i = 0; i < offset4; i++) {
            expectedData2[0][i * 2] = i + offset1;
            expectedData2[0][i * 2 + 1] = i + offset1;
            expectedData2[1][i * 2] = (long) (i + offset3);
            expectedData2[1][i * 2 + 1] = (long) (i + offset1);
            expectedData2[2][i * 2] = (double) (i + offset3);
            expectedData2[2][i * 2 + 1] = (double) (i + offset1);
            expectedData2[3][i * 2] = (double) (i + 50 + offset3);
            expectedData2[3][i * 2 + 1] = (double) (i + 50 + offset3);
        }
    }
}
