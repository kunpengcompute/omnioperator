/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 */

package nova.hetu.omniruntime.vector.serialize;

import static nova.hetu.omniruntime.type.CharDataType.CHAR;
import static nova.hetu.omniruntime.type.Date32DataType.DATE32;
import static nova.hetu.omniruntime.type.Date64DataType.DATE64;
import static nova.hetu.omniruntime.type.Decimal64DataType.DECIMAL64;
import static nova.hetu.omniruntime.type.InvalidDataType.INVALID;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.type.ContainerDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.ShortDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.type.VarBinaryDataType;
import nova.hetu.omniruntime.type.ArrayDataType;
import nova.hetu.omniruntime.type.MapDataType;
import nova.hetu.omniruntime.type.StructDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.BooleanVec;
import nova.hetu.omniruntime.vector.ContainerVec;
import nova.hetu.omniruntime.vector.Decimal128Vec;
import nova.hetu.omniruntime.vector.DictionaryVec;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.ShortVec;
import nova.hetu.omniruntime.vector.VarcharVec;
import nova.hetu.omniruntime.vector.ArrayVec;
import nova.hetu.omniruntime.vector.MapVec;
import nova.hetu.omniruntime.vector.StructVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;
import nova.hetu.omniruntime.vector.VecUtil;

import org.testng.annotations.Test;

import java.nio.charset.StandardCharsets;

/**
 * Vec batch serializer test
 *
 * @since 2021-9-14
 */
public class VecBatchSerializerTest {
    private static final int ROW_COUNT = 1024;

    @Test
    public void testSerializeCommonTypes() {
        // prepare vector batch
        LongVec longVec = new LongVec(ROW_COUNT);
        IntVec intVec = new IntVec(ROW_COUNT);
        VarcharVec varCharVec = new VarcharVec(ROW_COUNT);
        Decimal128Vec decimal128Vec = new Decimal128Vec(ROW_COUNT);
        ShortVec shortVec = new ShortVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            shortVec.set(i, (short) i);
            longVec.set(i, i);
            intVec.set(i, i);
            varCharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            decimal128Vec.set(i, new long[]{i, i + 1});
        }
        Vec[] vecs = {longVec, intVec, varCharVec, decimal128Vec, shortVec};
        VecBatch vecBatch = new VecBatch(vecs);

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        LongVec checkLongVec = (LongVec) checkVecBatch.getVectors()[0];
        IntVec checkIntVec = (IntVec) checkVecBatch.getVectors()[1];
        VarcharVec checkVarCharVec = (VarcharVec) checkVecBatch.getVectors()[2];
        Decimal128Vec checkDecimal128Vec = (Decimal128Vec) checkVecBatch.getVectors()[3];
        ShortVec checkShortVec = (ShortVec) checkVecBatch.getVectors()[4];
        for (int i = 0; i < ROW_COUNT; i++) {
            assertEquals(i, checkLongVec.get(i));
            assertEquals(i, checkIntVec.get(i));
            assertEquals("test" + i, new String(checkVarCharVec.get(i)));
            assertEquals(i, checkDecimal128Vec.get(i)[0]);
            assertEquals(i + 1, checkDecimal128Vec.get(i)[1]);
            assertEquals(i, checkShortVec.get(i));
        }
        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeDirectoryVecContainsLongVec() {
        // prepare vector batch
        LongVec dictionary = new LongVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            dictionary.set(i, i);
        }
        DictionaryVec dictionaryVec = new DictionaryVec(dictionary, new int[]{1, 2, 1000});
        dictionary.close();
        VecBatch vecBatch = new VecBatch(new Vec[]{dictionaryVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        LongVec checkLongVec = (LongVec) checkVecBatch.getVectors()[0];
        assertEquals(3, checkLongVec.getSize());
        assertEquals(1, checkLongVec.get(0));
        assertEquals(2, checkLongVec.get(1));
        assertEquals(1000, checkLongVec.get(2));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeDirectoryVecContainsVarcharVec() {
        // prepare vector batch
        VarcharVec dictionary = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            dictionary.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
        }
        DictionaryVec dictionaryVec = new DictionaryVec(dictionary, new int[]{1, 2, 1000});
        dictionary.close();
        VecBatch vecBatch = new VecBatch(new Vec[]{dictionaryVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkLongVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(3, checkLongVec.getSize());
        assertEquals("test1", new String(checkLongVec.get(0)));
        assertEquals("test2", new String(checkLongVec.get(1)));
        assertEquals("test1000", new String(checkLongVec.get(2)));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    // dictionary vector don't support nested dictionary vector
    @Test(enabled = false)
    public void testSerializeNestedDirectoryVec() {
        // prepare vector batch
        VarcharVec dictionary = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            dictionary.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
        }
        DictionaryVec dictionaryVec = new DictionaryVec(dictionary, new int[]{1, 2, 3, 4, 5, 6, 7, 1000});
        dictionary.close();
        DictionaryVec nestedDictionaryVec = new DictionaryVec(dictionaryVec, new int[]{1, 2, 7});
        dictionaryVec.close();
        VecBatch vecBatch = new VecBatch(new Vec[]{nestedDictionaryVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkLongVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(3, checkLongVec.getSize());
        assertEquals("test2", new String(checkLongVec.get(0)));
        assertEquals("test3", new String(checkLongVec.get(1)));
        assertEquals("test1000", new String(checkLongVec.get(2)));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    // container vector don't support copyPositions
    @Test
    public void testSerializeContainerVec() {
        // prepare vector batch
        LongVec longVec = new LongVec(ROW_COUNT);
        IntVec intVec = new IntVec(ROW_COUNT);
        VarcharVec varCharVec = new VarcharVec(ROW_COUNT);
        Decimal128Vec decimal128Vec = new Decimal128Vec(ROW_COUNT);
        ShortVec shortVec = new ShortVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            longVec.set(i, i);
            intVec.set(i, i);
            varCharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            decimal128Vec.set(i, new long[]{i, i + 1});
            shortVec.set(i, (short) i);
        }
        long[] vecAddresses = new long[]{longVec.getNativeVector(), intVec.getNativeVector(),
                varCharVec.getNativeVector(), decimal128Vec.getNativeVector(), shortVec.getNativeVector()};
        ContainerVec containerVec = new ContainerVec(vecAddresses.length, ROW_COUNT, vecAddresses,
                new DataType[]{new LongDataType(), new IntDataType(), new VarcharDataType(20),
                        new Decimal128DataType(10, 1), new ShortDataType()});
        VecBatch vecBatch = new VecBatch(new Vec[]{containerVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        ContainerVec checkContainerVec = (ContainerVec) checkVecBatch.getVectors()[0];
        assertEquals(1024, checkContainerVec.getSize());
        LongVec checkLongVec = new LongVec(checkContainerVec.getVector(0));
        IntVec checkIntVec = new IntVec(checkContainerVec.getVector(1));
        VarcharVec checkVarCharVec = new VarcharVec(checkContainerVec.getVector(2));
        Decimal128Vec checkDecimal128Vec = new Decimal128Vec(checkContainerVec.getVector(3));
        ShortVec checkShortVec = new ShortVec(checkContainerVec.getVector(4));
        for (int i = 0; i < ROW_COUNT; i++) {
            assertEquals(i, checkLongVec.get(i));
            assertEquals(i, checkIntVec.get(i));
            assertEquals("test" + i, new String(checkVarCharVec.get(i)));
            assertEquals(i, checkDecimal128Vec.get(i)[0]);
            assertEquals(i + 1, checkDecimal128Vec.get(i)[1]);
            assertEquals(i, checkShortVec.get(i));
        }

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    // container vector don't support slice
    @Test
    public void testSerializeNestedContainerVec() {
        // prepare vector batch
        LongVec longVec = new LongVec(ROW_COUNT);
        IntVec intVec = new IntVec(ROW_COUNT);
        VarcharVec varCharVec = new VarcharVec(ROW_COUNT);
        Decimal128Vec decimal128Vec = new Decimal128Vec(ROW_COUNT);
        ShortVec shortVec = new ShortVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            longVec.set(i, i);
            intVec.set(i, i);
            varCharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            decimal128Vec.set(i, new long[]{i, i + 1});
            shortVec.set(i, (short) i);
        }
        long[] vecAddresses = new long[]{longVec.getNativeVector(), intVec.getNativeVector(),
                varCharVec.getNativeVector(), decimal128Vec.getNativeVector(), shortVec.getNativeVector()};
        ContainerVec containerVec = new ContainerVec(vecAddresses.length, ROW_COUNT, vecAddresses,
                new DataType[]{new LongDataType(), new IntDataType(), new VarcharDataType(20),
                        new Decimal128DataType(10, 1), new ShortDataType()});
        ContainerVec nestedContainerVec = new ContainerVec(1, ROW_COUNT, new long[]{containerVec.getNativeVector()},
                new DataType[]{new ContainerDataType()});
        VecBatch vecBatch = new VecBatch(new Vec[]{nestedContainerVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        ContainerVec nestCheckContainerVec = (ContainerVec) checkVecBatch.getVectors()[0];
        assertEquals(1024, nestCheckContainerVec.getSize());
        ContainerVec checkContainerVec = new ContainerVec(nestCheckContainerVec.getVector(0));
        LongVec checkLongVec = new LongVec(checkContainerVec.getVector(0));
        IntVec checkIntVec = new IntVec(checkContainerVec.getVector(1));
        VarcharVec checkVarCharVec = new VarcharVec(checkContainerVec.getVector(2));
        Decimal128Vec checkDecimal128Vec = new Decimal128Vec(checkContainerVec.getVector(3));
        ShortVec checkShortVec = new ShortVec(checkContainerVec.getVector(4));
        for (int i = 0; i < ROW_COUNT; i++) {
            assertEquals(i, checkLongVec.get(i));
            assertEquals(i, checkIntVec.get(i));
            assertEquals("test" + i, new String(checkVarCharVec.get(i)));
            assertEquals(i, checkDecimal128Vec.get(i)[0]);
            assertEquals(i + 1, checkDecimal128Vec.get(i)[1]);
            assertEquals(i, checkShortVec.get(i));
        }

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeSameVectorMultipleTimes() {
        int size = 10;
        LongVec col1 = new LongVec(size);
        for (int i = 0; i < size; i++) {
            col1.set(i, i);
        }

        Vec[] vecs = new Vec[1];
        for (int count = 0; count < 2; count++) {
            vecs[0] = col1;
            VecBatch vecBatch = new VecBatch(vecs, size);
            // serialize
            VecBatchSerializer serializer = VecBatchSerializerFactory.create();
            byte[] str = serializer.serialize(vecBatch);
            // deserialize
            VecBatch resultVecBatch = serializer.deserialize(str);
            Object[][] expectedDatas = {{0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L}};
            assertVecBatchEquals(resultVecBatch, expectedDatas);
            vecBatch.close();
            freeVecBatch(resultVecBatch);
        }
        col1.close();
    }

    @Test
    public void testSerializeCharVec() {
        // prepare vector batch
        VarcharVec dictionary = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            dictionary.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
        }
        DictionaryVec dictionaryVec = new DictionaryVec(dictionary, new int[]{1, 2, 1000});
        dictionary.close();
        VecBatch vecBatch = new VecBatch(new Vec[]{dictionaryVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(3, checkResultVec.getSize());
        assertEquals("test1", new String(checkResultVec.get(0)));
        assertEquals("test2", new String(checkResultVec.get(1)));
        assertEquals("test1000", new String(checkResultVec.get(2)));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeSlicedVarCharVec() {
        // prepare vector batch
        VarcharVec varcharVec = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            varcharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
        }

        int size = 10;
        VarcharVec slicedVec = varcharVec.slice(10, size);

        VecBatch vecBatch = new VecBatch(new Vec[]{slicedVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(size, checkResultVec.getSize());
        for (int i = 0; i < size; i++) {
            int tmp = i + 10;
            assertEquals("test" + tmp, new String(checkResultVec.get(i)));
        }

        varcharVec.close();
        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeSlicedVarCharVecWithNull() {
        // prepare vector batch
        VarcharVec varcharVec = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            if (i % 2 == 0) {
                varcharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            } else {
                varcharVec.setNull(i);
            }
        }

        int positionOffset = 5;
        int size = 10;
        VarcharVec slicedVec = varcharVec.slice(positionOffset, size);

        VecBatch vecBatch = new VecBatch(new Vec[]{slicedVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(size, checkResultVec.getSize());
        for (int i = 0; i < size; i++) {
            if (i % 2 == positionOffset % 2) {
                int tmp = i + positionOffset;
                assertEquals("test" + tmp, new String(checkResultVec.get(i)));
            } else {
                assertTrue(checkResultVec.isNull(i));
            }
        }

        varcharVec.close();
        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeSlicedVarCharVecWithFullNull() {
        // prepare vector batch
        VarcharVec varcharVec = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            varcharVec.setNull(i);
        }

        int positionOffset = 5;
        int size = 10;
        VarcharVec slicedVec = varcharVec.slice(positionOffset, size);

        VecBatch vecBatch = new VecBatch(new Vec[]{slicedVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(size, checkResultVec.getSize());
        for (int i = 0; i < size; i++) {
            assertTrue(checkResultVec.isNull(i));
        }

        varcharVec.close();
        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeSlicedVarCharVecWithEmptyValue() {
        // prepare vector batch
        VarcharVec varcharVec = new VarcharVec(ROW_COUNT);
        for (int i = 0; i < ROW_COUNT; i++) {
            varcharVec.set(i, (" ").getBytes(StandardCharsets.UTF_8));
        }

        int positionOffset = 5;
        int size = 10;
        VarcharVec slicedVec = varcharVec.slice(positionOffset, size);

        VecBatch vecBatch = new VecBatch(new Vec[]{slicedVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVectors()[0];
        assertEquals(size, checkResultVec.getSize());
        for (int i = 0; i < size; i++) {
            assertEquals(" ", new String(checkResultVec.get(i)));
        }

        varcharVec.close();
        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeVectorSizeReset() {
        int size = 1000;
        LongVec col1 = new LongVec(size);
        for (int i = 0; i < size; i++) {
            col1.set(i, i);
        }
        col1.setSize(5);
        Vec[] vecs = new Vec[]{col1};
        VecBatch vecBatch = new VecBatch(vecs, size);
        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);
        // deserialize
        VecBatch resultVecBatch = serializer.deserialize(str);
        Object[][] expectedDatas = {{0L, 1L, 2L, 3L, 4L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(vecBatch);
        freeVecBatch(resultVecBatch);
    }

    @Test
    public void testSerializeVarcharVecWithNull() {
        // prepare vector batch
        int row = 10;
        VarcharVec vec = new VarcharVec(row);
        for (int i = 0; i < row; i++) {
            if (i % 2 == 0) {
                vec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            } else {
                vec.setNull(i);
            }
        }
        VecBatch vecBatch = new VecBatch(new Vec[]{vec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        VarcharVec checkResultVec = (VarcharVec) checkVecBatch.getVector(0);
        assertEquals(row, checkResultVec.getSize());
        for (int i = 0; i < row; i++) {
            if (i % 2 == 0) {
                assertEquals("test" + i, new String(checkResultVec.get(i)));
            } else {
                assertTrue(checkResultVec.isNull(i));
            }
        }

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeWithSetDataType() {
        int row = 5;
        IntVec data32 = new IntVec(row);
        VecUtil.setDataType(data32, DATE32);
        data32.put(new int[]{1, 2, 3, 4, 5}, 0, 0, row);
        LongVec data64 = new LongVec(row);
        VecUtil.setDataType(data64, DATE64);
        data64.put(new long[]{1, 2, 3, 4, 5}, 0, 0, row);
        LongVec decimal64 = new LongVec(row);
        VecUtil.setDataType(decimal64, DECIMAL64);
        decimal64.put(new long[]{1, 2, 3, 4, 5}, 0, 0, row);
        VarcharVec charVec = new VarcharVec(row);
        VecUtil.setDataType(charVec, CHAR);
        charVec.put(0, "12345".getBytes(StandardCharsets.UTF_8), 0, new int[]{0, 1, 2, 3, 4, 5}, 0, row);

        DoubleVec doubleVec = new DoubleVec(row);
        doubleVec.put(new double[]{1.1, 2.2, 3.3, 4.4, 5.5}, 0, 0, row);
        BooleanVec booleanVec = new BooleanVec(row);
        booleanVec.put(new boolean[]{true, false, true, false, true}, 0, 0, row);

        VecBatch vecBatch = new VecBatch(new Vec[]{data32, data64, decimal64, charVec, doubleVec, booleanVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] serialized = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(serialized);

        // check result
        Object[][] expectedDatas = {{1, 2, 3, 4, 5}, {1L, 2L, 3L, 4L, 5L}, {1L, 2L, 3L, 4L, 5L},
                {"1", "2", "3", "4", "5"}, {1.1D, 2.2D, 3.3D, 4.4D, 5.5D}, {true, false, true, false, true}};
        assertVecBatchEquals(checkVecBatch, expectedDatas);

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test(expectedExceptions = IllegalStateException.class, expectedExceptionsMessageRegExp = "Unexpected data type: "
            + "OMNI_INVALID")
    public void testSerializeInvalidType() {
        int row = 5;
        IntVec invalidType = new IntVec(row);
        VecUtil.setDataType(invalidType, INVALID);
        VecBatch vecBatch = new VecBatch(new Vec[]{invalidType});
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        try {
            serializer.serialize(vecBatch);
        } finally {
            freeVecBatch(vecBatch);
        }
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = "deserialize failed.null")
    public void deserializeInvalid() {
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        serializer.deserialize("invalid".getBytes(StandardCharsets.UTF_8));
    }

    @Test
    public void testSerializeDecimal128Vec() {
        int row = 8;
        IntVec intVec = new IntVec(row);
        VarcharVec varcharVec = new VarcharVec(row);
        Decimal128Vec decimal128Vec = new Decimal128Vec(row);
        for (int i = 0; i < row; i++) {
            intVec.set(i, i);
            varcharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            decimal128Vec.set(i, new long[]{i, i + 1});
        }
        Vec[] vecArray = new Vec[]{intVec, varcharVec, decimal128Vec};

        long[] nativeVectors = new long[vecArray.length];
        long[] nativeVectorValueBufAddresses = new long[vecArray.length];
        long[] nativeVectorNullBufAddresses = new long[vecArray.length];
        long[] nativeVectorOffsetBufAddresses = new long[]{0, varcharVec.getOffsetsBuf().getAddress(), 0};
        int[] encodings = new int[vecArray.length];
        int[] dataTypeIds = new int[vecArray.length];
        for (int i = 0; i < vecArray.length; i++) {
            nativeVectors[i] = vecArray[i].getNativeVector();
            nativeVectorValueBufAddresses[i] = vecArray[i].getValuesBuf().getAddress();
            nativeVectorNullBufAddresses[i] = vecArray[i].getValueNullsBuf().getAddress();
            encodings[i] = vecArray[i].getEncoding().ordinal();
            dataTypeIds[i] = vecArray[i].getType().getId().ordinal();
        }
        VecBatch vecBatch = new VecBatch(vecArray, row);
        VecBatch vecBatchFromNative = new VecBatch(vecBatch.getNativeVectorBatch(), nativeVectors,
                nativeVectorValueBufAddresses, nativeVectorNullBufAddresses, nativeVectorOffsetBufAddresses, encodings,
                dataTypeIds, row);
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] vecBatchSerialized = serializer.serialize(vecBatchFromNative);
        VecBatch vecBatchDeserialized = serializer.deserialize(vecBatchSerialized);
        assertVecBatchEquals(vecBatchDeserialized, vecBatchFromNative);
        assertVecBatchEquals(vecBatch, vecBatchFromNative);

        freeVecBatch(vecBatch);
        freeVecBatch(vecBatchDeserialized);
    }

    @Test
    public void testSerializeArrayVec() {
        // prepare vector batch
        int elementSize = 10;
        int rowSize = 3;
        ArrayDataType arrayDataType = new ArrayDataType(new VarcharDataType(16));
        ArrayVec arrayVec = new ArrayVec(arrayDataType, rowSize, true);

        VarcharVec elementVec = new VarcharVec(elementSize);
        String tmpValStr = "testvarcharVal";
        for (int i = 0; i < elementSize; i++) {
            String str = tmpValStr.substring(0, i) + i;
            elementVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        int[] offsets = new int[]{0, 2, 5, 10};
        arrayVec.addElements(elementVec);
        arrayVec.addOffsets(offsets);
        VecBatch vecBatch = new VecBatch(new Vec[]{arrayVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        ArrayVec checkResultVec = (ArrayVec) checkVecBatch.getVector(0);
        VarcharVec deElementVec = (VarcharVec) checkResultVec.getElementVec();
        assertEquals(rowSize, checkResultVec.getSize());
        assertEquals(elementVec.getSize(), deElementVec.getSize());
        assertEquals(new String(elementVec.get(2)), new String(deElementVec.get(2)));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeArrayVecWithNull() {
        // prepare vector batch
        int elementSize = 11;
        int rowSize = 4;
        ArrayDataType arrayDataType = new ArrayDataType(new VarcharDataType(16));
        ArrayVec arrayVec = new ArrayVec(arrayDataType, rowSize, true);

        VarcharVec elementVec = new VarcharVec(elementSize);
        String tmpValStr = "testvarcharVal";
        for (int i = 0; i < elementSize - 1; i++) {
            String str = tmpValStr.substring(0, i) + i;
            elementVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }
        elementVec.setNull(10);

        int[] offsets = new int[]{0, 2, 5, 10, 11};
        arrayVec.addElements(elementVec);
        arrayVec.addOffsets(offsets);
        VecBatch vecBatch = new VecBatch(new Vec[]{arrayVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        ArrayVec checkResultVec = (ArrayVec) checkVecBatch.getVector(0);
        VarcharVec deElementVec = (VarcharVec) checkResultVec.getElementVec();
        assertEquals(checkResultVec.getSize(3), 0);
        assertEquals(rowSize, checkResultVec.getSize());
        assertEquals(elementVec.getSize(), deElementVec.getSize());
        assertEquals(new String(elementVec.get(2)), new String(deElementVec.get(2)));

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeMapVec() {
        // 2,3,5
        int kvSize = 10;
        int rowSize = 3;
        MapDataType mapDataType = new MapDataType(new VarcharDataType(16), new VarcharDataType(16));
        MapVec mapVec = new MapVec(mapDataType, rowSize, true);
        VarcharVec originalVec = new VarcharVec(kvSize);
        String tmpStr = "testvarchar";
        for (int i = 0; i < kvSize; i++) {
            String str = tmpStr.substring(0, i) + i;
            originalVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        VarcharVec valueVec = new VarcharVec(kvSize);
        String tmpValStr = "testvarcharVal";
        for (int i = 0; i < kvSize; i++) {
            String str = tmpValStr.substring(0, i) + i;
            valueVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        int[] offsets = new int[]{0, 2, 5, 10};
        mapVec.AddKeys(originalVec);
        mapVec.AddValues(valueVec);
        mapVec.AddOffsets(offsets);

        VecBatch vecBatch = new VecBatch(new Vec[]{mapVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        MapVec checkResultVec = (MapVec) checkVecBatch.getVector(0);
        VarcharVec checkOriginalVec = (VarcharVec) (checkResultVec.getKeyVec());
        for (int i = 0; i < checkOriginalVec.getSize(); i++) {
            byte[] actualValue = checkOriginalVec.get(i);
            byte[] expectedValue = originalVec.get(i);
            assertEquals(actualValue, expectedValue);
        }
        assertEquals(checkOriginalVec.getSize(), 10);

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeMapVecWithNull() {
        // 2,null,8
        int kvSize = 10;
        int rowSize = 3;
        MapDataType mapDataType = new MapDataType(new VarcharDataType(16), new VarcharDataType(16));
        MapVec mapVec = new MapVec(mapDataType, rowSize, true);
        VarcharVec originalVec = new VarcharVec(kvSize);
        String tmpStr = "testvarchar";
        for (int i = 0; i < kvSize; i++) {
            String str = tmpStr.substring(0, i) + i;
            originalVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        VarcharVec valueVec = new VarcharVec(kvSize);
        String tmpValStr = "testvarcharVal";
        for (int i = 0; i < kvSize; i++) {
            String str = tmpValStr.substring(0, i) + i;
            valueVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }
        mapVec.setNull(1);

        int[] offsets = new int[]{0, 2, 2, 10};
        mapVec.AddKeys(originalVec);
        mapVec.AddValues(valueVec);
        mapVec.AddOffsets(offsets);

        VecBatch vecBatch = new VecBatch(new Vec[]{mapVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        MapVec checkResultVec = (MapVec) checkVecBatch.getVector(0);
        assertTrue(checkResultVec.isNull(1));
        VarcharVec checkOriginalVec = (VarcharVec) (checkResultVec.getKeyVec());
        for (int i = 0; i < checkOriginalVec.getSize(); i++) {
            byte[] actualValue = checkOriginalVec.get(i);
            byte[] expectedValue = originalVec.get(i);
            assertEquals(actualValue, expectedValue);
        }
        assertEquals(checkOriginalVec.getSize(), 10);

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeStructVec() {
        DataType[] fieldTypes = new DataType[]{new VarcharDataType(16)};

        int size = 10;
        StructDataType structDataType = new StructDataType(fieldTypes);
        StructVec structVec = new StructVec(structDataType, size);
        VarcharVec originalVec = (VarcharVec) (structVec.getChild(0));
        String tmpStr = "testvarchar";
        for (int i = 0; i < size; i++) {
            String str = tmpStr.substring(0, i) + i;
            originalVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        VecBatch vecBatch = new VecBatch(new Vec[]{structVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        StructVec checkResultVec = (StructVec) checkVecBatch.getVector(0);
        VarcharVec checkOriginalVec = (VarcharVec) (checkResultVec.getChild(0));
        for (int i = 0; i < checkOriginalVec.getSize(); i++) {
            byte[] actualValue = checkOriginalVec.get(i);
            byte[] expectedValue = originalVec.get(i);
            assertEquals(actualValue, expectedValue);
        }
        assertEquals(checkOriginalVec.getRealValueBufCapacityInBytes(), 55);
        assertEquals(checkOriginalVec.getSize(), 10);

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }

    @Test
    public void testSerializeStructVecWithNull() {
        DataType[] fieldTypes = new DataType[]{new VarcharDataType(16)};

        // 10 row
        // index: 0,null,2,null,4,null,6,null,8,null
        int size = 10;
        StructDataType structDataType = new StructDataType(fieldTypes);
        StructVec structVec = new StructVec(structDataType, size);
        VarcharVec originalVec = new VarcharVec(10);
        String tmpStr = "testvarchar";
        for (int i = 0; i < size; i++) {
            if (i % 2 == 0) {
                String str = tmpStr.substring(0, i) + i;
                originalVec.set(i, str.getBytes(StandardCharsets.UTF_8));
            } else {
                originalVec.setNull(i);
                structVec.setNull(i);
            }
        }
        structVec.setChild(0, originalVec);

        VecBatch vecBatch = new VecBatch(new Vec[]{structVec});

        // serialize
        VecBatchSerializer serializer = VecBatchSerializerFactory.create();
        byte[] str = serializer.serialize(vecBatch);

        // deserialize
        VecBatch checkVecBatch = serializer.deserialize(str);

        // check result
        StructVec checkResultVec = (StructVec) checkVecBatch.getVector(0);
        VarcharVec checkOriginalVec = (VarcharVec) (checkResultVec.getChild(0));
        for (int i = 0; i < checkOriginalVec.getSize(); i++) {
            byte[] actualValue = checkOriginalVec.get(i);
            byte[] expectedValue = originalVec.get(i);
            assertEquals(actualValue, expectedValue);
            if (i % 2 != 0) {
                assertTrue(checkResultVec.isNull(i));
            }
        }

        freeVecBatch(vecBatch);
        freeVecBatch(checkVecBatch);
    }
    @Test(expectedExceptions = IllegalStateException.class)
    public void testDeserializePropagatesConstructionFailure() {
        VecBatchSerde.Vec unsupported = VecBatchSerde.Vec.newBuilder()
                .setTypeExt(VecBatchSerde.DataTypeExt.newBuilder()
                        .setId(VecBatchSerde.DataTypeExt.DataTypeId.OMNI_INVALID))
                .setSize(1).setNulls(com.google.protobuf.ByteString.copyFrom(new byte[1])).build();
        byte[] bytes = VecBatchSerde.VecBatch.newBuilder().setRowCount(1).setVecCount(1)
                .addVectors(unsupported).build().toByteArray();
        new ProtoVecBatchSerializer().deserialize(bytes);
    }

    @Test(expectedExceptions = IllegalStateException.class)
    public void testDeserializePropagatesFailureAfterClosingEarlierVectors() {
        VecBatchSerde.Vec first = VecBatchSerde.Vec.newBuilder()
                .setTypeExt(VecBatchSerde.DataTypeExt.newBuilder()
                        .setId(VecBatchSerde.DataTypeExt.DataTypeId.OMNI_INT))
                .setSize(1).setNulls(com.google.protobuf.ByteString.copyFrom(new byte[1]))
                .setValues(com.google.protobuf.ByteString.copyFrom(new byte[4])).build();
        VecBatchSerde.Vec unsupported = first.toBuilder().setTypeExt(
                first.getTypeExt().toBuilder().setId(VecBatchSerde.DataTypeExt.DataTypeId.OMNI_INVALID)).build();
        byte[] bytes = VecBatchSerde.VecBatch.newBuilder().setRowCount(1).setVecCount(2)
                .addVectors(first).addVectors(unsupported).build().toByteArray();
        new ProtoVecBatchSerializer().deserialize(bytes);
    }

}
