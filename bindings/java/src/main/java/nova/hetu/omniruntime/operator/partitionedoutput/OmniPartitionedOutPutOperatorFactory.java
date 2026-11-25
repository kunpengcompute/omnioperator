/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.operator.partitionedoutput;

import nova.hetu.omniruntime.operator.OmniOperatorFactory;
import nova.hetu.omniruntime.operator.OmniOperatorFactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DataTypeSerializer;

import java.util.Arrays;
import java.util.Objects;
import java.util.OptionalInt;

/**
 * The type Omni partitionedoutput operator factory.
 *
 * @since 2021-06-30
 */
public class OmniPartitionedOutPutOperatorFactory
        extends OmniOperatorFactory<OmniPartitionedOutPutOperatorFactory.FactoryContext> {
    /**
     * Instantiates a new Omni partitioned out put operator factory.
     *
     * @param sourceTypes the source types
     * @param isReplicatesAnyRow the replicates any row
     * @param nullChannel the null channel
     * @param partitionChannels the partition channels
     * @param partitionCount the partition count
     * @param bucketToPartition the bucket to partition
     * @param isHashPrecomputed the is hash precomputed
     * @param hashChannelTypes the hash channel types
     * @param hashChannels the hash channels
     * @param operatorConfig the operator config
     */
    public OmniPartitionedOutPutOperatorFactory(DataType[] sourceTypes, boolean isReplicatesAnyRow,
            OptionalInt nullChannel, int[] partitionChannels, int partitionCount, int[] bucketToPartition,
            boolean isHashPrecomputed, DataType[] hashChannelTypes, int[] hashChannels, OperatorConfig operatorConfig) {
        super(new FactoryContext(sourceTypes, isReplicatesAnyRow, nullChannel, partitionChannels, partitionCount,
                bucketToPartition, isHashPrecomputed, hashChannelTypes, hashChannels, operatorConfig));
    }

    /**
     * Instantiates a new Omni partitioned out put operator factory with default
     * operator config.
     *
     * @param sourceTypes the source types
     * @param isReplicatesAnyRow the replicates any row
     * @param nullChannel the null channel
     * @param partitionChannels the partition channels
     * @param partitionCount the partition count
     * @param bucketToPartition the bucket to partition
     * @param isHashPrecomputed the is hash precomputed
     * @param hashChannelTypes the hash channel types
     * @param hashChannels the hash channels
     */
    public OmniPartitionedOutPutOperatorFactory(DataType[] sourceTypes, boolean isReplicatesAnyRow,
            OptionalInt nullChannel, int[] partitionChannels, int partitionCount, int[] bucketToPartition,
            boolean isHashPrecomputed, DataType[] hashChannelTypes, int[] hashChannels) {
        this(sourceTypes, isReplicatesAnyRow, nullChannel, partitionChannels, partitionCount, bucketToPartition,
                isHashPrecomputed, hashChannelTypes, hashChannels, new OperatorConfig());
    }

    private static native long createPartitionedOutputOperatorFactory(String sourceTypes, boolean isReplicatesAnyRow,
            int nullChannel, int[] partitionChannels, int partitionCount, int[] bucketToPartition,
            boolean isHashPrecomputed, String hashChannelTypes, int[] hashChannels);

    @Override
    protected long createNativeOperatorFactory(FactoryContext context) {
        int nullChannel = context.nullChannel.isPresent() ? context.nullChannel.getAsInt() : -1;
        return createPartitionedOutputOperatorFactory(DataTypeSerializer.serialize(context.sourceTypes),
                context.isReplicatesAnyRow, nullChannel, context.partitionChannels, context.partitionCount,
                context.bucketToPartition, context.isHashPrecomputed,
                DataTypeSerializer.serialize(context.hashChannelTypes), context.hashChannels);
    }

    /**
     * The type Factory context.
     *
     * @since 2021-06-30
     */
    public static class FactoryContext extends OmniOperatorFactoryContext {
        private final DataType[] sourceTypes;

        private final boolean isReplicatesAnyRow;

        private final OptionalInt nullChannel;

        private final int[] partitionChannels;

        private final int partitionCount;

        private final int[] bucketToPartition;

        private final boolean isHashPrecomputed;

        private final DataType[] hashChannelTypes;

        private final int[] hashChannels;

        private OperatorConfig operatorConfig;

        /**
         * Instantiates a new Jit context.
         *
         * @param sourceTypes the source types
         * @param isReplicatesAnyRow the replicates any row
         * @param nullChannel the null channel
         * @param partitionChannels the partition channels
         * @param partitionCount the partition count
         * @param bucketToPartition the bucket to partition
         * @param isHashPrecomputed the is hash precomputed
         * @param hashChannelTypes the hash channel types
         * @param hashChannels the hash channels
         * @param operatorConfig the operator config
         */
        public FactoryContext(DataType[] sourceTypes, boolean isReplicatesAnyRow, OptionalInt nullChannel,
                int[] partitionChannels, int partitionCount, int[] bucketToPartition, boolean isHashPrecomputed,
                DataType[] hashChannelTypes, int[] hashChannels, OperatorConfig operatorConfig) {
            this.sourceTypes = sourceTypes;
            this.isReplicatesAnyRow = isReplicatesAnyRow;
            this.nullChannel = nullChannel;
            this.partitionChannels = partitionChannels;
            this.partitionCount = partitionCount;
            this.bucketToPartition = bucketToPartition;
            this.isHashPrecomputed = isHashPrecomputed;
            this.hashChannelTypes = hashChannelTypes;
            this.hashChannels = hashChannels;
            this.operatorConfig = operatorConfig;
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null || getClass() != obj.getClass()) {
                return false;
            }
            FactoryContext context = (FactoryContext) obj;
            return isReplicatesAnyRow == context.isReplicatesAnyRow && partitionCount == context.partitionCount
                    && Arrays.equals(sourceTypes, context.sourceTypes)
                    && Objects.equals(nullChannel, context.nullChannel)
                    && Arrays.equals(partitionChannels, context.partitionChannels)
                    && Arrays.equals(bucketToPartition, context.bucketToPartition)
                    && context.isHashPrecomputed == isHashPrecomputed
                    && Arrays.equals(hashChannelTypes, context.hashChannelTypes)
                    && Arrays.equals(hashChannels, context.hashChannels)
                    && operatorConfig.equals(context.operatorConfig);
        }

        @Override
        public int hashCode() {
            return Objects.hash(Arrays.hashCode(sourceTypes), isReplicatesAnyRow, nullChannel,
                    Arrays.hashCode(partitionChannels), partitionCount, Arrays.hashCode(bucketToPartition),
                    isHashPrecomputed, Arrays.hashCode(hashChannelTypes), Arrays.hashCode(hashChannels),
                    operatorConfig);
        }
    }
}
