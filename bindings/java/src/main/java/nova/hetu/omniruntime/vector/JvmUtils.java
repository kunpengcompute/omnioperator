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

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import sun.misc.Unsafe;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * jvm utils.
 *
 * @since 2021-08-05
 */
public final class JvmUtils {
    /**
     * jvm unsafe.
     */
    public static final Unsafe UNSAFE;

    private static final Constructor<?> DIRECT_BUFFER_CONSTRUCTOR;

    private static final long BITS_MAX_DIRECT_MEMORY;

    static {
        try {
            Field field = Unsafe.class.getDeclaredField("theUnsafe");
            field.setAccessible(true);
            Object obj = field.get(null);
            UNSAFE = obj instanceof Unsafe ? (Unsafe) obj : null;
            if (UNSAFE == null) {
                throw new OmniRuntimeException(OmniErrorType.OMNI_NATIVE_ERROR, "Unsafe access not available");
            }

            assertArrayIndexScale("Boolean", Unsafe.ARRAY_BOOLEAN_INDEX_SCALE, 1);
            assertArrayIndexScale("Byte", Unsafe.ARRAY_BYTE_INDEX_SCALE, 1);
            assertArrayIndexScale("Short", Unsafe.ARRAY_SHORT_INDEX_SCALE, 2);
            assertArrayIndexScale("Int", Unsafe.ARRAY_INT_INDEX_SCALE, 4);
            assertArrayIndexScale("Long", Unsafe.ARRAY_LONG_INDEX_SCALE, 8);
            assertArrayIndexScale("Float", Unsafe.ARRAY_FLOAT_INDEX_SCALE, 4);
            assertArrayIndexScale("Double", Unsafe.ARRAY_DOUBLE_INDEX_SCALE, 8);

            long address = -1;
            final ByteBuffer direct = ByteBuffer.allocateDirect(1);
            try {
                final Object directBufferConstructor = AccessController.doPrivileged((PrivilegedAction<Object>) () -> {
                    final Constructor<?> constructor;
                    try {
                        constructor = direct.getClass().getDeclaredConstructor(long.class, int.class);
                        constructor.setAccessible(true);
                        return constructor;
                    } catch (NoSuchMethodException e) {
                        throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, e);
                    }
                });

                if (directBufferConstructor instanceof Constructor<?>) {
                    address = UNSAFE.allocateMemory(1);
                    // try to use the constructor
                    try {
                        ((Constructor<?>) directBufferConstructor).newInstance(address, 1);
                        DIRECT_BUFFER_CONSTRUCTOR = (Constructor<?>) directBufferConstructor;
                    } catch (InstantiationException | IllegalAccessException | InvocationTargetException e) {
                        throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, e);
                    }
                } else {
                    throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT,
                            "get the director byte buffer constructor failed.");
                }
            } finally {
                if (address != -1) {
                    UNSAFE.freeMemory(address);
                }
            }

            Class<?> bitsClass =
                    Class.forName("java.nio.Bits");
            int javaVersion = majorVersion(System.getProperty("java.specification.version"));
            String fieldName = javaVersion >= 11 ? "MAX_MEMORY" : "maxMemory";
            Field maxMemoryField = bitsClass.getDeclaredField(fieldName);
            if (maxMemoryField.getType() == long.class) {
                long offset = UNSAFE.staticFieldOffset(maxMemoryField);
                Object object = UNSAFE.staticFieldBase(maxMemoryField);
                BITS_MAX_DIRECT_MEMORY = UNSAFE.getLong(object, offset);
            } else {
                BITS_MAX_DIRECT_MEMORY = -1;
            }
        } catch (ClassNotFoundException | NoSuchFieldException | IllegalAccessException e) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, e);
        }
    }

    private JvmUtils() {
    }

    private static void assertArrayIndexScale(String name, int actualIndexScale, int expectedIndexScale) {
        if (actualIndexScale != expectedIndexScale) {
            throw new IllegalStateException(
                    name + " array index scale must be " + expectedIndexScale + ", but is " + actualIndexScale);
        }
    }

    private static int majorVersion(final String javaSpecVersion) {
        final String[] components = javaSpecVersion.split("\\.");
        final int[] version = new int[components.length];
        for (int i = 0; i < components.length; i++) {
            version[i] = Integer.parseInt(components[i]);
        }

        if (version[0] == 1) {
            return version[1];
        } else {
            return version[0];
        }
    }

    /**
     * construct a director byte buffer by address and capacity.
     *
     * @param omniBuffer the address of byte buffer
     * @return director byte buffer
     */
    public static ByteBuffer directBuffer(OmniBuffer omniBuffer) {
        if (omniBuffer.getCapacity() < 0) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_PARAM_ERROR,
                    "Capacity is negative, has to be positive or 0");
        }

        if (DIRECT_BUFFER_CONSTRUCTOR == null) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT,
                    "DirectByteBuffer.<ini>(long, int) not available");
        }
        try {
            return ((ByteBuffer) DIRECT_BUFFER_CONSTRUCTOR.newInstance(omniBuffer.getAddress(),
                omniBuffer.getCapacity())).order(ByteOrder.LITTLE_ENDIAN);
        } catch (InstantiationException | IllegalAccessException | InvocationTargetException e) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, e);
        }
    }

    public static long estimateMaxDirectMemory() {
        return BITS_MAX_DIRECT_MEMORY;
    }
}
