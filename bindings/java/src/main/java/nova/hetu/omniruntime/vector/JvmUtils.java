/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Copyright 2013 The Netty Project
 *
 * The Netty Project licenses this file to you under the Apache License,
 * version 2.0 (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at:
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
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
                    Constructor<?> constructor;
                    try {
                        constructor = direct.getClass().getDeclaredConstructor(long.class, long.class);
                    } catch (NoSuchMethodException e) {
                        try {
                            constructor = direct.getClass().getDeclaredConstructor(long.class, int.class);
                        } catch (NoSuchMethodException ex) {
                            throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, ex);
                        }
                    }
                    constructor.setAccessible(true);
                    return constructor;
                });

                if (directBufferConstructor instanceof Constructor<?>) {
                    address = UNSAFE.allocateMemory(1);
                    // try to use the constructor
                    try {
                        newDirectBuffer((Constructor<?>) directBufferConstructor, address, 1);
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

            Class<?> bitsClass = Class.forName("java.nio.Bits");
            Field maxMemoryField = null;
            for (Field curField : bitsClass.getDeclaredFields()) {
                String fieldName = curField.getName();
                if ("maxMemory".equals(fieldName) || "MAX_MEMORY".equals(fieldName)) {
                    maxMemoryField = curField;
                    break;
                }
            }
            if (maxMemoryField != null && maxMemoryField.getType() == long.class) {
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
                    "DirectByteBuffer.<init>(long, int/long) not available");
        }
        try {
            return newDirectBuffer(DIRECT_BUFFER_CONSTRUCTOR, omniBuffer.getAddress(),
                omniBuffer.getCapacity()).order(ByteOrder.LITTLE_ENDIAN);
        } catch (InstantiationException | IllegalAccessException | InvocationTargetException e) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_NOSUPPORT, e);
        }
    }

    private static ByteBuffer newDirectBuffer(Constructor<?> constructor, long address, int capacity)
            throws InstantiationException, IllegalAccessException, InvocationTargetException {
        if (constructor.getParameterTypes()[1] == long.class) {
            return (ByteBuffer) constructor.newInstance(address, (long) capacity);
        }
        return (ByteBuffer) constructor.newInstance(address, capacity);
    }

    public static long estimateMaxDirectMemory() {
        return BITS_MAX_DIRECT_MEMORY;
    }
}
