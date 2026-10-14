/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 */
#include "join_sub_partitioner.h"

#include <string>

#include "operator/hash_util.h"
#include "type/data_type.h"
#include "util/omni_exception.h"
#include "vector/large_string_container.h"
#include "vector/vector.h"

namespace omniruntime::op {

JoinSubPartitionConfig JoinSubPartitionConfig::FromQueryConfig(
    const omniruntime::config::QueryConfig &queryConfig)
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 1U << queryConfig.spillNumPartitionBits();
    cfg.startPartitionBit = queryConfig.spillStartPartitionBit();
    cfg.activeSubPartition = 0;
    return cfg;
}

uint32_t JoinSubPartitioner::SubPartitionFromHash(int64_t hash) const
{
    if (config_.numSubPartitions <= 1U) {
        return 0;
    }
    const uint32_t mask = config_.numSubPartitions - 1U;
    return static_cast<uint32_t>((static_cast<uint64_t>(hash) >> config_.startPartitionBit) & mask);
}

namespace {

template <typename T>
int64_t HashPrimitiveColumn(omniruntime::vec::BaseVector *col, int32_t row)
{
    using namespace omniruntime::vec;
    const auto enc = col->GetEncoding();
    if (enc == OMNI_ENCODING_CONST) {
        return HashUtil::HashValue(static_cast<ConstVector<T> *>(col)->GetConstValue());
    }
    if (enc == OMNI_DICTIONARY) {
        return HashUtil::HashValue(static_cast<Vector<DictionaryContainer<T>> *>(col)->GetValue(row));
    }
    if (enc != OMNI_FLAT) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Unsupported join key vector encoding " + std::to_string(static_cast<int32_t>(enc)));
    }
    return HashUtil::HashValue(static_cast<Vector<T> *>(col)->GetValue(row));
}

int64_t HashVarcharColumn(omniruntime::vec::BaseVector *col, int32_t row)
{
    using namespace omniruntime::vec;
    using LC = LargeStringContainer<std::string_view>;
    const auto enc = col->GetEncoding();
    std::string_view value;
    if (enc == OMNI_ENCODING_CONST) {
        value = static_cast<ConstVector<std::string_view> *>(col)->GetConstValue();
    } else if (enc == OMNI_DICTIONARY) {
        value = static_cast<Vector<DictionaryContainer<std::string_view>> *>(col)->GetValue(row);
    } else if (enc == OMNI_FLAT) {
        value = static_cast<Vector<LC> *>(col)->GetValue(row);
    } else {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Unsupported VARCHAR join key vector encoding " + std::to_string(static_cast<int32_t>(enc)));
    }
    auto *data = reinterpret_cast<int8_t *>(const_cast<char *>(value.data()));
    return HashUtil::HashValue(data, static_cast<int32_t>(value.size()));
}

int64_t HashKeyColumn(omniruntime::vec::BaseVector *col, int32_t row, int32_t typeId)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BOOLEAN:
            return HashPrimitiveColumn<bool>(col, row);
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            return HashPrimitiveColumn<int32_t>(col, row);
        case omniruntime::type::OMNI_LONG:
            return HashPrimitiveColumn<int64_t>(col, row);
        case omniruntime::type::OMNI_VARCHAR:
            return HashVarcharColumn(col, row);
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "Unsupported join key type id " + std::to_string(typeId) +
                " (supported: BOOLEAN, INT, LONG, DATE32, VARCHAR).");
    }
}

} // namespace

std::vector<uint32_t> JoinSubPartitioner::PartitionRowsByKeyColumns(omniruntime::vec::VectorBatch *batch,
    const omniruntime::type::DataTypes &rowTypes, const std::vector<int32_t> &keyColIndices) const
{
    if (batch == nullptr) {
        return {};
    }
    const int32_t rows = batch->GetRowCount();
    std::vector<uint32_t> partitions(static_cast<size_t>(rows), config_.activeSubPartition);
    if (rows <= 0) {
        return partitions;
    }
    for (int32_t row = 0; row < rows; ++row) {
        int64_t combinedHash = 0;
        bool firstKey = true;
        bool anyNull = false;
        for (auto keyCol : keyColIndices) {
            auto *col = batch->Get(keyCol);
            const int32_t nullIdx = (col->GetEncoding() == omniruntime::vec::OMNI_ENCODING_CONST) ? 0 : row;
            if (col->IsNull(nullIdx)) {
                anyNull = true;
                break;
            }
            const int64_t keyHash = HashKeyColumn(col, row, rowTypes.GetIds()[keyCol]);
            combinedHash = firstKey ? keyHash : HashUtil::CombineHash(combinedHash, keyHash);
            firstKey = false;
        }

        partitions[static_cast<size_t>(row)] =
            anyNull ? config_.activeSubPartition : SubPartitionFromHash(combinedHash);
    }
    return partitions;
}

} // namespace omniruntime::op
