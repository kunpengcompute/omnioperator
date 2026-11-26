/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package nova.hetu.omniruntime.operator;

import com.google.common.cache.Cache;
import com.google.common.cache.CacheBuilder;
import com.google.common.util.concurrent.UncheckedExecutionException;

import nova.hetu.omniruntime.OmniLibs;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * The type Omni operator factory.
 *
 * @param <T> the type parameter
 * @since 2021-06-30
 */
public abstract class OmniOperatorFactory<T extends OmniOperatorFactoryContext> {
    private static final Cache<OmniOperatorFactoryContext, Long> FACTORY_CACHE = CacheBuilder.newBuilder()
            .expireAfterAccess(24, TimeUnit.HOURS).maximumSize(100000).build();

    static {
        OmniLibs.load();
    }

    private final long nativeOperatorFactory;

    private OmniOperatorFactoryContext context;

    /**
     * Instantiates a new Omni operator factory.
     *
     * @param context the context
     */
    public OmniOperatorFactory(OmniOperatorFactoryContext context) {
        try {
            if (context.isNeedCache()) {
                nativeOperatorFactory = FACTORY_CACHE.get(context, () -> createNativeOperatorFactory((T) context));
            } else {
                nativeOperatorFactory = createNativeOperatorFactory((T) context);
            }
            this.context = context;
        } catch (ExecutionException e) {
            throw new RuntimeException("Get operator factory instance failed.");
        } catch (UncheckedExecutionException e) {
            throw new OmniRuntimeException(e.getCause().getMessage());
        }
    }

    // createOperator
    private static native long createOperatorNative(long factoryAddress);

    /**
     * Gets native operator factory.
     *
     * @return the native operator factory
     */
    public long getNativeOperatorFactory() {
        return nativeOperatorFactory;
    }

    /**
     * Create operator omni operator.
     *
     * @return the omni operator
     */
    public OmniOperator createOperator() {
        long nativeOperator = createOperatorNative(nativeOperatorFactory);
        return new OmniOperator(nativeOperator);
    }

    /**
     * Create native operator factory long.
     *
     * @param context the context
     * @return the long
     */
    protected abstract long createNativeOperatorFactory(T context);

    /**
     * release operator factory
     */
    public void close() {
        if (!context.isNeedCache()) {
            closeNativeOperatorFactory(nativeOperatorFactory);
        }
    }

    private static native void closeNativeOperatorFactory(long nativeOperatorFactory);
}
