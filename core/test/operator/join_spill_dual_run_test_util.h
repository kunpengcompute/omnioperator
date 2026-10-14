/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Correctness-first nospill/spill dual-run helpers for Join Spill gtests.
 */
#ifndef JOIN_SPILL_DUAL_RUN_TEST_UTIL_H
#define JOIN_SPILL_DUAL_RUN_TEST_UTIL_H
#include <algorithm>
#include <cstdint>
#include <memory>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "operator/config/operator_config.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_sub_partitioner.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
namespace join_spill_dual_run {
using omniruntime::op::JoinSubPartitionConfig;
using omniruntime::op::JoinSubPartitioner;
using omniruntime::op::SparkSpillConfig;
using omniruntime::type::DataTypeId;
using omniruntime::type::DataTypes;
using omniruntime::type::LongType;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;
using omniruntime::vec::VectorHelper;
inline JoinSubPartitionConfig JoinSpillTestSubPartitionConfig()
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    return cfg;
}
inline std::shared_ptr<SparkSpillConfig> MakeJoinSpillTestSparkConfig(const std::string &path)
{
    return std::make_shared<SparkSpillConfig>(true, path, 100UL << 30, INT32_MAX, 100, 0, false);
}
inline VectorBatch *MakeLongKeyValueBatch(const std::vector<int64_t> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<int64_t>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), keys[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}
inline int64_t FindLongKeyForSubPartition(uint32_t partition, int64_t exclude = -1)
{
    JoinSubPartitioner partitioner(JoinSpillTestSubPartitionConfig());
    const DataTypes types({LongType(), LongType()});
    for (int64_t key = 0; key < 20000; ++key) {
        if (key == exclude) {
            continue;
        }
        auto *batch = MakeLongKeyValueBatch({key}, {0});
        const auto buckets = partitioner.PartitionRowsByKeyColumns(batch, types, {0});
        VectorHelper::FreeVecBatch(batch);
        if (!buckets.empty() && buckets[0] == partition) {
            return key;
        }
    }
    return -1;
}
inline bool DualRunCellsPhysicallyEqual(DataTypeId leftType, DataTypeId rightType,
    const std::string &leftValue, const std::string &rightValue)
{
    if (leftValue != rightValue) {
        return false;
    }
    if (leftType == rightType) {
        return true;
    }
    // Resident hash-join may emit OMNI_INT for DATE32 keys; Path A restore uses schema DATE32.
    if ((leftType == omniruntime::type::OMNI_INT && rightType == omniruntime::type::OMNI_DATE32) ||
        (leftType == omniruntime::type::OMNI_DATE32 && rightType == omniruntime::type::OMNI_INT)) {
        return leftValue.size() == sizeof(int32_t);
    }
    return false;
}
struct Cell {
    DataTypeId type;
    bool isNull;
    std::string exactValue;
    bool operator==(const Cell &other) const
    {
        if (isNull != other.isNull) {
            return false;
        }
        if (isNull) {
            return true;
        }
        return DualRunCellsPhysicallyEqual(type, other.type, exactValue, other.exactValue);
    }
    bool operator<(const Cell &other) const
    {
        return std::tie(type, isNull, exactValue) < std::tie(other.type, other.isNull, other.exactValue);
    }
};
using Tuple = std::vector<Cell>;
using TupleMultiset = std::vector<Tuple>;
template <typename T>
std::string ExactBytes(const T &value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    return std::string(reinterpret_cast<const char *>(&value), sizeof(T));
}
inline Cell CanonicalizeCell(BaseVector *vector, int32_t row)
{
    if (vector == nullptr) {
        throw std::invalid_argument("cannot canonicalize a null vector");
    }
    const auto type = vector->GetTypeId();
    if (vector->IsNull(row)) {
        return Cell{type, true, {}};
    }
    switch (type) {
        case omniruntime::type::OMNI_BOOLEAN:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<bool>(vector, row))};
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int32_t>(vector, row))};
        case omniruntime::type::OMNI_LONG:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int64_t>(vector, row))};
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR: {
            // Flat VARCHAR uses LargeStringContainer; do not cast via GetValueFromVector<string_view>.
            const auto value = VectorHelper::GetStringValueFromVector(vector, row);
            return Cell{type, false, std::string(value)};
        }
        default:
            throw std::invalid_argument("unsupported dual-run cell type: " +
                std::to_string(static_cast<int32_t>(type)));
    }
}
inline void AppendCanonicalRowsAndFree(VectorBatch *batch, TupleMultiset *rows)
{
    if (batch == nullptr) {
        return;
    }
    try {
        for (int32_t row = 0; row < batch->GetRowCount(); ++row) {
            Tuple tuple;
            tuple.reserve(static_cast<size_t>(batch->GetVectorCount()));
            for (int32_t column = 0; column < batch->GetVectorCount(); ++column) {
                tuple.push_back(CanonicalizeCell(batch->Get(column), row));
            }
            rows->push_back(std::move(tuple));
        }
    } catch (...) {
        VectorHelper::FreeVecBatch(batch);
        throw;
    }
    VectorHelper::FreeVecBatch(batch);
}
inline void SortTupleMultiset(TupleMultiset *rows)
{
    std::sort(rows->begin(), rows->end());
}
class ScopedTempDirectory {
public:
    explicit ScopedTempDirectory(std::string tag)
    {
        static uint64_t next = 0;
        path_ = std::filesystem::temp_directory_path() /
            ("omni-join-spill-dual-" + std::move(tag) + "-" + std::to_string(++next));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScopedTempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    const std::filesystem::path &path() const
    {
        return path_;
    }
private:
    std::filesystem::path path_;
};
inline void DrainProbeRows(omniruntime::op::Operator *builder, omniruntime::op::Operator *probe, TupleMultiset *rows,
    omniruntime::op::HashBuilderOperatorFactory *hashFactory, omniruntime::BuildSide buildSide)
{
    builder->noMoreInput();
    probe->noMoreInput();
    auto *lookup = dynamic_cast<omniruntime::op::LookupJoinOperator *>(probe);
    for (int32_t i = 0; i < 20000 && (builder->GetStatus() != ::OMNI_STATUS_FINISHED ||
        probe->GetStatus() != ::OMNI_STATUS_FINISHED); ++i) {
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        if (buildOut != nullptr) VectorHelper::FreeVecBatch(buildOut);
        VectorBatch *probeOut = nullptr;
        probe->GetOutput(&probeOut);
        if (probeOut != nullptr) AppendCanonicalRowsAndFree(probeOut, rows);
    }
    if (lookup == nullptr || !lookup->NeedsUnvisitedBuildOutput() || hashFactory == nullptr) {
        return;
    }
    const DataTypes &types = hashFactory->GetBuildTypes();
    std::vector<int32_t> outputs(static_cast<size_t>(types.GetSize()));
    for (int32_t i = 0; i < types.GetSize(); ++i) {
        outputs[static_cast<size_t>(i)] = i;
    }
    auto *outerFactory = omniruntime::op::LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
        types, outputs.data(), types.GetSize(), outputs.data(), types, reinterpret_cast<int64_t>(hashFactory),
        buildSide);
    auto *outer = outerFactory->CreateOperator();
    for (int32_t i = 0; i < 100000 && !outer->isFinished(); ++i) {
        VectorBatch *output = nullptr;
        outer->GetOutput(&output);
        if (output != nullptr) AppendCanonicalRowsAndFree(output, rows);
    }
    lookup->FinishFullTableProbeAfterOuter();
    VectorBatch *buildOut = nullptr;
    builder->GetOutput(&buildOut);
    if (buildOut != nullptr) VectorHelper::FreeVecBatch(buildOut);
    omniruntime::op::Operator::DeleteOperator(outer);
    delete outerFactory;
}
} // namespace join_spill_dual_run
#endif
