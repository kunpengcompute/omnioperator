/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "ConfigBase.h"

namespace omniruntime::config {
/// A simple wrapper around velox::ConfigBase. Defines constants for query
/// config properties and accessor methods.
/// Create per query context. Does not have a singleton instance.
/// Does not allow altering properties on the fly. Only at creation time.
class QueryConfig {
public:
    explicit QueryConfig(): config_(
        std::make_shared<config::ConfigBase>(std::unordered_map<std::string, std::string>()))
    {
        ValidateConfig();
    }

    explicit QueryConfig(const std::unordered_map<std::string, std::string> &values): config_(
        std::make_shared<config::ConfigBase>(std::unordered_map<std::string, std::string>(values)))
    {
        ValidateConfig();
    }

    explicit QueryConfig(std::unordered_map<std::string, std::string> &&values): config_(
        std::make_shared<config::ConfigBase>(std::move(values)))
    {
        ValidateConfig();
    }

    /// Global enable spilling flag.
    static constexpr const char *kSpillEnabled = "spill_enabled";

    /// Aggregation spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kAggregationSpillEnabled = "aggregation_spill_enabled";
    //
    static constexpr const char *kMemFraction = "mem_fraction";

    /// Join spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kJoinSpillEnabled = "join_spill_enabled";

    /// OrderBy spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kOrderBySpillEnabled = "order_by_spill_enabled";

    /// Window spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kWindowSpillEnabled = "window_spill_enabled";

    /// If true, the memory arbitrator will reclaim memory from table writer by
    /// flushing its buffered data to disk. only applies if "spill_enabled" flag
    /// is set.
    static constexpr const char *kWriterSpillEnabled = "writer_spill_enabled";

    /// RowNumber spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kRowNumberSpillEnabled = "row_number_spill_enabled";

    /// TopNRowNumber spilling flag, only applies if "spill_enabled" flag is set.
    static constexpr const char *kTopNRowNumberSpillEnabled = "topn_row_number_spill_enabled";

    /// The max row numbers to fill and spill for each spill run. This is used to
    /// cap the memory used for spilling. If it is zero, then there is no limit
    /// and spilling might run out of memory.
    /// Based on offline test results, the default value is set to 12 million rows
    /// which uses ~128MB memory when to fill a spill run.
    static constexpr const char *kMaxSpillRunRows = "max_spill_run_rows";

    /// The max spill bytes limit set for each query. This is used to cap the
    /// storage used for spilling. If it is zero, then there is no limit and
    /// spilling might exhaust the storage or takes too long to run. The default
    /// value is set to 100 GB.
    static constexpr const char *kMaxSpillBytes = "max_spill_bytes";

    /// The max allowed spilling level with zero being the initial spilling level.
    /// This only applies for hash build spilling which might trigger recursive
    /// spilling when the build table is too big. If it is set to -1, then there
    /// is no limit and then some extreme large query might run out of spilling
    /// partition bits (see kSpillPartitionBits) at the end. The max spill level
    /// is used in production to prevent some bad user queries from using too much
    /// io and cpu resources.
    static constexpr const char *kMaxSpillLevel = "max_spill_level";

    /// The max allowed spill file size. If it is zero, then there is no limit.
    static constexpr const char *kMaxSpillFileSize = "max_spill_file_size";

    static constexpr const char *kSpillCompressionKind = "spill_compression_codec";

    /// Enable the prefix sort or fallback to timsort in spill. The prefix sort is
    /// faster than std::sort but requires the memory to build normalized prefix
    /// keys, which might have potential risk of running out of server memory.
    static constexpr const char *kSpillPrefixSortEnabled = "spill_prefixsort_enabled";

    /// Default offset spill start partition bit. It is used with
    /// 'kJoinSpillPartitionBits' or 'kAggregationSpillPartitionBits' together to
    /// calculate the spilling partition number for join spill or aggregation
    /// spill.
    static constexpr const char *kSpillStartPartitionBit = "spiller_start_partition_bit";

    /// Default number of spill partition bits. It is the number of bits used to
    /// calculate the spill partition number for hash join and RowNumber. The
    /// number of spill partitions will be power of two.
    ///
    /// NOTE: as for now, we only support up to 8-way spill partitioning.
    static constexpr const char *kSpillNumPartitionBits = "spiller_num_partition_bits";

    /// The minimal available spillable memory reservation in percentage of the
    /// current memory usage. Suppose the current memory usage size of M,
    /// available memory reservation size of N and min reservation percentage of
    /// P, if M * P / 100 > N, then spiller operator needs to grow the memory
    /// reservation with percentage of spillableReservationGrowthPct(). This
    /// ensures we have sufficient amount of memory reservation to process the
    /// large input outlier.
    static constexpr const char *kMinSpillableReservationPct = "min_spillable_reservation_pct";

    /// The spillable memory reservation growth percentage of the previous memory
    /// reservation size. 10 means exponential growth along a series of integer
    /// powers of 11/10. The reservation grows by this much until it no longer
    /// can, after which it starts spilling.
    static constexpr const char *kSpillableReservationGrowthPct = "spillable_reservation_growth_pct";

    /// Specifies the shuffle compression kind which is defined by
    /// CompressionKind. If it is CompressionKind_NONE, then no compression.
    static constexpr const char *kShuffleCompressionKind = "shuffle_compression_codec";

    /// User provided session timezone. Stores a string with the actual timezone
    /// name, e.g: "America/Los_Angeles".
    static constexpr const char *kSessionTimezone = "session_timezone";

    /// If true, timezone-less timestamp conversions (e.g. string to timestamp,
    /// when the string does not specify a timezone) will be adjusted to the user
    /// provided session timezone (if any).
    ///
    /// For instance:
    ///
    ///  if this option is true and user supplied "America/Los_Angeles",
    ///  "1970-01-01" will be converted to -28800 instead of 0.
    ///
    /// False by default.
    static constexpr const char *kAdjustTimestampToTimezone = "adjust_timestamp_to_session_timezone";

    static constexpr const char *KRowShuffleEnabled = "rowShuffle_enabled";
    static constexpr const char *KMaxBatchSizeInBytes = "maxBatchSizeInBytes";
    static constexpr const char *KMaxRowCount = "maxRowCount";
    static constexpr const char *KMaxBatchSize = "max_batch_size";
    static constexpr const char *KAqeShuffle = "aqe_shuffle";
    static constexpr const char *KShuffleSpillBatchRowNum = "shuffleSpillBatchRowNum";
    static constexpr const char *KCompressBlockSize = "compressBlockSize";
    static constexpr const char *KJoinReorderEnhance = "JoinReorderEnhance";
    static constexpr const char *KSpillDir = "spill_dir";
    static constexpr const char *KIsOverFlowASNull = "is_overflow_as_null";

    // spill config
    static constexpr const char *KSpillHashAggRowThreshold = "spill_hash_agg_row_threshold";
    static constexpr const char *KSpillSortRowThreshold = "spill_sort_row_threshold";

    static constexpr const char *KColumnarSpillMemThreshold = "columnar_spil_mem_threshold";
    static constexpr const char *KColumnarSpillWriteBufferSize = "columnar_spill_write_buffer_size";
    static constexpr const char *KColumnarSpillDirDiskReserveSize = "columnar_spil_dir_disk_reserve_size";
    static constexpr const char *KColumnarSpillEnableCompress = "columnar_spil_enable_compress";

    static constexpr const char *KEnableAdaptivePartialAggregation = "enable_adaptive_partial_aggregation";
    static constexpr const char *KAdaptivePartialAggregationMinRows = "adaptive_partial_aggregation_min_rows";
    static constexpr const char *KAdaptivePartialAggregationRatio = "adaptive_partial_aggregation_ratio";
    static constexpr const char *KPreferVectorizationExpression = "prefer_vectorization_expression";

    /// When true, each executor builds the BHJ hash table only once and shares it
    /// across all tasks that probe the same broadcast relation.
    static constexpr const char *KBuildHashTableOncePerExecutor = "build_hash_table_once_per_executor";

    /// Spark partition id for deterministic per-partition behavior (e.g. rand(seed)). Aligned with Velox "spark.partition_id".
    static constexpr const char *kSparkPartitionId = "spark.partition_id";

    uint64_t maxRowCount() const
    {
        static constexpr uint64_t kDefault = 12UL << 20;
        return get<uint64_t>(KMaxRowCount, kDefault);
    }

    uint64_t MaxBatchSize() const
    {
        static constexpr uint64_t kDefault = 4096;
        return get<uint64_t>(KMaxBatchSize, kDefault);
    }

    std::string SpillDir() const
    {
        return get<std::string>(KSpillDir, "/tmp/spill");
    }

    bool aqeShuffle() const
    {
        return get<bool>(KAqeShuffle, false);
    }

    bool IsOverFlowASNull() const
    {
        return get<bool>(KIsOverFlowASNull, true);
    }

    bool joinReorderEnhance() const
    {
        return get<bool>(KJoinReorderEnhance, false);
    }

    uint64_t compressBlockSize() const
    {
        return get<uint64_t>(KCompressBlockSize, 1048);
    }

    uint64_t maxSpillRunRows() const
    {
        static constexpr uint64_t kDefault = 12UL << 20;
        return get<uint64_t>(kMaxSpillRunRows, kDefault);
    }

    uint64_t maxSpillBytes() const
    {
        static constexpr uint64_t kDefault = 100UL << 30;
        return get<uint64_t>(kMaxSpillBytes, kDefault);
    }

    static uint32_t writeStrideSize()
    {
        static constexpr uint32_t kDefault = 100'000;
        return kDefault;
    }

    static bool flushPerBatch()
    {
        static constexpr bool kDefault = true;
        return kDefault;
    }

    bool spillEnabled() const
    {
        return get<bool>(kSpillEnabled, false);
    }

    bool aggregationSpillEnabled() const
    {
        return get<bool>(kAggregationSpillEnabled, true);
    }

    bool joinSpillEnabled() const
    {
        // todo: join spill is not ready, set fix false. return `get<bool>(kJoinSpillEnabled, true)` in final version
        return false;
    }

    bool orderBySpillEnabled() const
    {
        return get<bool>(kOrderBySpillEnabled, false);
    }

    bool windowSpillEnabled() const
    {
        return get<bool>(kWindowSpillEnabled, true);
    }

    bool writerSpillEnabled() const
    {
        return get<bool>(kWriterSpillEnabled, true);
    }

    bool rowNumberSpillEnabled() const
    {
        return get<bool>(kRowNumberSpillEnabled, true);
    }

    bool topNRowNumberSpillEnabled() const
    {
        return get<bool>(kTopNRowNumberSpillEnabled, true);
    }

    int32_t maxSpillLevel() const
    {
        return get<int32_t>(kMaxSpillLevel, 1);
    }

    uint8_t spillStartPartitionBit() const
    {
        // Avoid get<uint8_t>: on many platforms uint8_t is unsigned char
        // in configBase stringstream will read a single digit character, not a decimal value.
        constexpr int32_t kDefaultStartBit = 48;
        const int32_t parsed = get<int32_t>(kSpillStartPartitionBit, kDefaultStartBit);
        if (parsed < 0 || parsed > 255) {
            return static_cast<uint8_t>(kDefaultStartBit);
        }
        return static_cast<uint8_t>(parsed);
    }

    uint8_t spillNumPartitionBits() const
    {
        constexpr int32_t kDefaultBits = 3;
        constexpr int32_t kMaxBits = 3;
        const int32_t parsed = get<int32_t>(kSpillNumPartitionBits, kDefaultBits);
        const int32_t clamped = std::min(kMaxBits, std::max<int32_t>(0, parsed));
        return static_cast<uint8_t>(clamped);
    }

    uint64_t maxSpillFileSize() const
    {
        constexpr uint64_t kDefaultMaxFileSize = 0;
        return get<uint64_t>(kMaxSpillFileSize, kDefaultMaxFileSize);
    }

    std::string spillCompressionKind() const
    {
        return get<std::string>(kSpillCompressionKind, "none");
    }

    bool spillPrefixSortEnabled() const
    {
        return get<bool>(kSpillPrefixSortEnabled, false);
    }

    int32_t minSpillableReservationPct() const
    {
        constexpr int32_t kDefaultPct = 5;
        return get<int32_t>(kMinSpillableReservationPct, kDefaultPct);
    }

    int32_t memFractionPct() const
    {
        constexpr int32_t kDefaultPct = 10;
        return get<int32_t>(kMemFraction, kDefaultPct);
    }

    int32_t spillableReservationGrowthPct() const
    {
        constexpr int32_t kDefaultPct = 10;
        return get<int32_t>(kSpillableReservationGrowthPct, kDefaultPct);
    }

    std::string shuffleCompressionKind() const
    {
        return get<std::string>(kShuffleCompressionKind, "none");
    }

    template <typename T>
    T get(const std::string &key, const T &defaultValue) const
    {
        return config_->Get<T>(key, defaultValue);
    }

    template <typename T>
    std::optional<T> get(const std::string &key) const
    {
        return std::optional<T>(config_->Get<T>(key));
    }

    uint64_t SpillMemThreshold() const
    {
        constexpr uint64_t kDefaultValue = 90;
        return get<uint64_t>(KColumnarSpillMemThreshold, kDefaultValue);
    }

    uint64_t SpillWriteBufferSize() const
    {
        constexpr uint64_t kDefaultValue = 4121440L;
        return get<uint64_t>(KColumnarSpillWriteBufferSize, kDefaultValue);
    }

    uint64_t SpillDirDiskReserveSize() const
    {
        constexpr uint64_t kDefaultValue = 10737418240L;
        return get<uint64_t>(KColumnarSpillDirDiskReserveSize, kDefaultValue);
    }

    uint64_t SpillEnableCompress() const
    {
        return get<bool>(KColumnarSpillEnableCompress, false);
    }

    int32_t SpillHashAggRowThreshold() const
    {
        constexpr int32_t kDefaultValue = INT32_MAX;
        return get<int32_t>(KSpillHashAggRowThreshold, kDefaultValue);
    }

    int32_t SpillSortRowThreshold() const
    {
        constexpr int32_t kDefaultValue = INT32_MAX;
        return get<int32_t>(KSpillSortRowThreshold, kDefaultValue);
    }

    bool EnableAdaptivePartialAggregation() const
    {
        constexpr bool kDefaultValue = true;
        return get<bool>(KEnableAdaptivePartialAggregation, kDefaultValue);
    }

    int32_t AdaptivePartialAggregationMinRows() const
    {
        constexpr int32_t kDefaultValue = 500000;
        return get<int32_t>(KAdaptivePartialAggregationMinRows, kDefaultValue);
    }

    double AdaptivePartialAggregationRatio() const
    {
        constexpr double kDefaultValue = 0.8;
        return get<double>(KAdaptivePartialAggregationRatio, kDefaultValue);
    }

    std::string SessionTimezone() const
    {
        return get<std::string>(kSessionTimezone, "");
    }

    bool AdjustTimestampToTimezone() const
    {
        return get<bool>(kAdjustTimestampToTimezone, false);
    }

    bool PreferVectorizationExpression() const
    {
        constexpr bool kDefaultValue = false;
        return get<bool>(KPreferVectorizationExpression, kDefaultValue);
    }

    bool buildHashTableOncePerExecutor() const
    {
        constexpr bool kDefaultValue = true;
        return get<bool>(KBuildHashTableOncePerExecutor, kDefaultValue);
    }

    /// Spark partition id (aligned with Velox). Default 0 when not set so rand(seed) etc. work without Spark context.
    int32_t sparkPartitionId() const
    {
        constexpr int32_t kDefault = 0;
        return get<int32_t>(kSparkPartitionId, kDefault);
    }

    /// Test-only method to override the current query config properties.
    /// It is not thread safe.
    void testingOverrideConfigUnsafe(std::unordered_map<std::string, std::string> &&values);

private:
    void ValidateConfig();

    std::shared_ptr<ConfigBase> config_;
};
} // namespace omniruntime::config
