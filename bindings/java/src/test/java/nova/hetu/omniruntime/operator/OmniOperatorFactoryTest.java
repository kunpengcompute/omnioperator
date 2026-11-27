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

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.operator.config.OperatorConfig;

import org.testng.annotations.Test;

import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * The type Omni operator factory test.
 *
 * @since 2021-6-7
 */
public class OmniOperatorFactoryTest {
    /**
     * The Check factory.
     */
    MockOperatorFactory checkFactory = new MockOperatorFactory(1);

    /**
     * Test operator factory cache.
     */
    @Test
    public void testOperatorFactoryCache() {
        MockOperatorFactory factory1 = new MockOperatorFactory(1);
        MockOperatorFactory factory2 = new MockOperatorFactory(1);
        assertEquals(factory1.getNativeOperatorFactory(), factory2.getNativeOperatorFactory());
        MockOperatorFactory factory3 = new MockOperatorFactory(2);
        assertNotEquals(factory1.getNativeOperatorFactory(), factory3.getNativeOperatorFactory());
    }

    /**
     * Test operator factory cache multi thread.
     *
     * @throws InterruptedException the interrupted exception
     */
    @Test
    public void testOperatorFactoryCacheMultiThread() {
        final int threadNum = 10000;
        final int corePoolSize = 10;
        final int maximumPoolSize = 50;
        CountDownLatch countDownLatch = new CountDownLatch(threadNum);
        ThreadPoolExecutor threadPool = new ThreadPoolExecutor(corePoolSize, maximumPoolSize, 60L, TimeUnit.SECONDS,
                new LinkedBlockingQueue<>(threadNum));

        for (int i = 0; i < threadNum; i++) {
            CompletableFuture.runAsync(() -> {
                try {
                    MockOperatorFactory factory = new MockOperatorFactory(1);
                    assertEquals(checkFactory.getNativeOperatorFactory(), factory.getNativeOperatorFactory());
                    factory.close();
                } finally {
                    countDownLatch.countDown();
                }
            }, threadPool);
        }
        // This will wait until all future ready.
        try {
            countDownLatch.await();
        } catch (InterruptedException ex) {
            assertTrue(false);
        }

        threadPool.shutdown();
    }

    /**
     * The type Mock operator factory.
     */
    public static class MockOperatorFactory extends OmniOperatorFactory<MockOperatorFactory.FactoryContext> {
        /**
         * Instantiates a new Mock operator factory.
         *
         * @param context the context
         */
        public MockOperatorFactory(long context) {
            super(new FactoryContext(context, new OperatorConfig()));
        }

        @Override
        protected long createNativeOperatorFactory(FactoryContext context) {
            return System.nanoTime();
        }

        /**
         * Factory Context
         *
         * @since 2021-7-13
         */
        public static class FactoryContext extends OmniOperatorFactoryContext {
            private final OperatorConfig operatorConfig;

            private final long context;

            /**
             * Instantiates a new Context.
             *
             * @param operatorConfig operatorConfig
             * @param context context
             */
            public FactoryContext(long context, OperatorConfig operatorConfig) {
                this.context = context;
                this.operatorConfig = operatorConfig;
            }

            /**
             * Calculate hash code
             *
             * @return hash value
             */
            @Override
            public int hashCode() {
                return Objects.hash(context);
            }

            /**
             * Check equals
             *
             * @param that object
             * @return whether equals
             */
            @Override
            public boolean equals(Object that) {
                return context == ((FactoryContext) that).context;
            }
        }
    }
}
