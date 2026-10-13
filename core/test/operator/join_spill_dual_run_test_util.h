/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Correctness-first nospill/spill dual-run helpers for Join Spill gtests.
 */
#ifndef JOIN_SPILL_DUAL_RUN_TEST_UTIL_H
#define JOIN_SPILL_DUAL_RUN_TEST_UTIL_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "operator/join/join_spill_state.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"

namespace join_spill_dual_run {

using omniruntime::op::JoinHandoffPhase;
using omniruntime::type::DataTypeId;
using omniruntime::vec::BaseVector;
using omniruntime::vec::VectorBatch;
using omniruntime::vec::VectorHelper;

enum class SpillMode {
    NoSpill,
    ForcedSpillPathA,
};

using BatchFactory = std::function<VectorBatch *()>;

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

struct RunResult {
    TupleMultiset rows;
    size_t outputBatchCount = 0;
    uint64_t buildSpilledRows = 0;
    uint64_t probeSpilledRows = 0;
    size_t spillFileCount = 0;
    bool sawPathA = false;
    bool sawRestore = false;
    JoinHandoffPhase finalPhase = JoinHandoffPhase::Idle;
};

struct ScenarioScheduler {
    std::function<RunResult(const BatchFactory &, const BatchFactory &)> residentFullTable;
    std::function<RunResult(const BatchFactory &, const BatchFactory &)> forcedSpillPathARestore;
};

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
        case omniruntime::type::OMNI_BYTE:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int8_t>(vector, row))};
        case omniruntime::type::OMNI_SHORT:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int16_t>(vector, row))};
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int32_t>(vector, row))};
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_TIMESTAMP:
        case omniruntime::type::OMNI_DATE64:
        case omniruntime::type::OMNI_DECIMAL64:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<int64_t>(vector, row))};
        case omniruntime::type::OMNI_FLOAT:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<float>(vector, row))};
        case omniruntime::type::OMNI_DOUBLE:
            return Cell{type, false, ExactBytes(VectorHelper::GetValueFromVector<double>(vector, row))};
        case omniruntime::type::OMNI_VARBINARY:
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

inline RunResult RunResidentFullTableNoSpill(const ScenarioScheduler &scheduler,
    const BatchFactory &buildFactory, const BatchFactory &probeFactory)
{
    if (!scheduler.residentFullTable) {
        throw std::invalid_argument("resident full-table scheduler is not configured");
    }
    return scheduler.residentFullTable(buildFactory, probeFactory);
}

inline RunResult RunForcedSpillPathARestore(const ScenarioScheduler &scheduler,
    const BatchFactory &buildFactory, const BatchFactory &probeFactory)
{
    if (!scheduler.forcedSpillPathARestore) {
        throw std::invalid_argument("forced Path A spill scheduler is not configured");
    }
    return scheduler.forcedSpillPathARestore(buildFactory, probeFactory);
}

inline void AssertTupleMultisetEqual(RunResult expected, RunResult actual)
{
    SortTupleMultiset(&expected.rows);
    SortTupleMultiset(&actual.rows);
    ASSERT_EQ(expected.rows, actual.rows);
}

inline void AssertNoSpillEvidence(const RunResult &result)
{
    EXPECT_EQ(0u, result.buildSpilledRows);
    EXPECT_EQ(0u, result.probeSpilledRows);
    EXPECT_EQ(0u, result.spillFileCount);
    EXPECT_FALSE(result.sawPathA);
    EXPECT_FALSE(result.sawRestore);
}

// Path A protocol: build group-spill and probe mirror must both write.
inline void AssertSpillEvidence(const RunResult &result)
{
    EXPECT_GT(result.buildSpilledRows, 0u);
    EXPECT_GT(result.probeSpilledRows, 0u);
    EXPECT_GT(result.spillFileCount, 0u);
    EXPECT_TRUE(result.sawPathA);
    EXPECT_TRUE(result.sawRestore);
    EXPECT_EQ(JoinHandoffPhase::Complete, result.finalPhase);
}

// Empty probe: Path A + restore still run, but probe writes no rows.
inline void AssertPathABuildSpillEmptyProbe(const RunResult &result)
{
    EXPECT_GT(result.buildSpilledRows, 0u);
    EXPECT_EQ(0u, result.probeSpilledRows);
    EXPECT_TRUE(result.sawPathA);
    EXPECT_TRUE(result.sawRestore);
    EXPECT_EQ(JoinHandoffPhase::Complete, result.finalPhase);
}

// Zero build rows: empty full-table handoff, no Path A, no files on either side.
inline void AssertEmptyBuildNoPathA(const RunResult &result)
{
    EXPECT_EQ(0u, result.buildSpilledRows);
    EXPECT_EQ(0u, result.probeSpilledRows);
    EXPECT_FALSE(result.sawPathA);
    EXPECT_EQ(JoinHandoffPhase::Complete, result.finalPhase);
}

inline size_t CountRegularFiles(const std::filesystem::path &root)
{
    if (!std::filesystem::exists(root)) {
        return 0;
    }
    size_t count = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
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

class ScopedEnvironment {
public:
    ScopedEnvironment(std::string name, std::string value) : name_(std::move(name))
    {
        if (const char *current = std::getenv(name_.c_str()); current != nullptr) {
            previous_ = current;
        }
        if (setenv(name_.c_str(), value.c_str(), 1) != 0) {
            throw std::runtime_error("failed to set environment variable " + name_);
        }
    }

    ~ScopedEnvironment()
    {
        if (previous_.has_value()) {
            (void)setenv(name_.c_str(), previous_->c_str(), 1);
        } else {
            (void)unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    std::optional<std::string> previous_;
};

} // namespace join_spill_dual_run

#endif
