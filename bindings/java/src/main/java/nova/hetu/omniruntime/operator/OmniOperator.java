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

import static nova.hetu.omniruntime.constants.Status.OMNI_STATUS_NORMAL;

import nova.hetu.omniruntime.vector.VecBatch;

import java.util.Iterator;

/**
 * The type Omni operator.
 *
 * @since 2021-06-30
 */
public final class OmniOperator implements AutoCloseable {
    /**
     * The Native operator.
     */
    protected final long nativeOperator;

    private VecBatchIterator outputIterator;

    /**
     * Instantiates a new Omni operator.
     *
     * @param nativeOperator the native operator
     */
    protected OmniOperator(long nativeOperator) {
        this.nativeOperator = nativeOperator;
    }

    // addInput
    private static native int addInputNative(long nativeOperator, long nativeVectorBatch);

    // getOutput
    private static native OmniResults getOutputNative(long nativeOperator);

    // close
    private static native void closeNative(long nativeOperator);

    // getSpilledBytes
    private static native long getSpilledBytesNative(long nativeOperator);

    // getMetricsInfo
    private static native long[] getMetricsInfoNative(long nativeOperator);

    // getHashMapUniqueKeys called by the adaptive partial hashagg optimization
    private static native long getHashMapUniqueKeysNative(long nativeOperator);

    // called by the adaptive partial hashagg optimization
    private static native VecBatch alignSchemaNative(long nativeOperator, long inputVecBatchNative);


    /**
     * Add input.
     *
     * @param vecBatch the vec batch
     * @return the int
     */
    public int addInput(VecBatch vecBatch) {
        return addInputNative(nativeOperator, vecBatch.getNativeVectorBatch());
    }

    /**
     * Gets output.
     *
     * @return the output
     */
    public Iterator<VecBatch> getOutput() {
        if (outputIterator == null) {
            outputIterator = new VecBatchIterator();
        }
        outputIterator.reset();
        return outputIterator;
    }

    /**
     * Close native operator.
     */
    public void close() {
        closeNative(nativeOperator);
    }

    /**
     * Get spill size.
     *
     * @return the spilled size
     */
    public long getSpilledBytes() {
        return getSpilledBytesNative(nativeOperator);
    }

    /**
     * Get all Metrics info.
     *
     * @return the metrics info array
     */
    public long[] getMetricsInfo() {
        return getMetricsInfoNative(nativeOperator);
    }

    /**
     * Get the number of hashmap unique key.
     *
     * @return the unique key number
     */
    public long getHashMapUniqueKeys() {
        return getHashMapUniqueKeysNative(nativeOperator);
    }

    /**
     * The input vecBatch is aligned based on the operator schema.
     *
     * @param inputVecBatch the input vec batch
     * @return aligned vecBatch
     */
    public VecBatch alignSchema(VecBatch inputVecBatch) {
        return alignSchemaNative(nativeOperator, inputVecBatch.getNativeVectorBatch());
    }

    private class VecBatchIterator implements Iterator<VecBatch> {
        private boolean hasNext;

        private OmniResults results;

        private VecBatch next;

        /**
         * Instantiates a new Vec batch iterator.
         */
        public VecBatchIterator() {
            resetIterator();
            advanced();
            hasNext = true;
        }

        public void reset() {
            hasNext = true;
        }

        @Override
        public boolean hasNext() {
            if (!hasNext) {
                return false;
            }
            // if it first, the results is null,
            // or index reach the count of vector batches but it don't finished,
            // then advanced().
            if (results == null || (next == results.getVecBatch() && !isFinished())) {
                resetIterator();
                advanced();
            }

            // after advanced(), if results is still null,
            // or vectorBatch hash been pulled, or vecBatch is null
            // means there is no more data.
            if (results == null || next == results.getVecBatch()) {
                resetIterator();
                hasNext = false;
                return false;
            }

            hasNext = true;
            return true;
        }

        @Override
        public VecBatch next() {
            next = results.getVecBatch();
            return next;
        }

        private void resetIterator() {
            results = null;
            next = null;
        }

        private void advanced() {
            results = getOutputNative(nativeOperator);
        }

        private boolean isFinished() {
            return !OMNI_STATUS_NORMAL.equals(results.getStatus());
        }
    }
}
