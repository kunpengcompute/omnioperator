/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: pages index implementations
 */
#ifndef __PAGES_INDEX_H__
#define __PAGES_INDEX_H__

#include <cstdint>
#include <vector>
#include "type/data_type.h"
#include "type/data_types.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "util/operator_util.h"
#include "operator/memory_builder.h"

namespace omniruntime {
namespace op {
class PagesIndex : public MemoryBuilder {
public:
    explicit PagesIndex(const DataTypes &types, bool pdqSortEnabled, bool inplacePdqSortEnabled,
        bool timSortEnabled, omniruntime::mem::OmniTaskMemoryPool *ownerPool = nullptr);

    explicit PagesIndex(const DataTypes &types,
        omniruntime::mem::OmniTaskMemoryPool *ownerPool = nullptr)
        : PagesIndex(types, false, false, false, ownerPool)
    {}

    ~PagesIndex() override;

    void AddVecBatch(omniruntime::vec::VectorBatch *vecBatch);

    void Prepare();

    template <DataTypeId typeId>
    void PrepareRadixSort(const bool ascending, const bool nullFirst, const uint32_t sortCol);

    template <DataTypeId typeId> void PrepareInplaceSort(int32_t nullFirst)
    {
        auto vecBatchCount = static_cast<int32_t>(inputVecBatches.size());

        for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
            VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
            auto col = vecBatch->Get(0);
            totalNullCount += col->GetNullCount();
            if (col->GetEncoding() == OMNI_DICTIONARY) {
                auto *decodePool = ownerPool_ != nullptr ? ownerPool_ : col->GetPool();
                vecBatch->SetVector(0, VectorHelper::DecodeDictionaryVector(col, decodePool));
                delete col;
            }
        }

        // A Sort-owned PagesIndex must allocate its preparation scratch from
        // the same pool in which Sort reserved the scratch. Falling back to
        // the source vector owner preserves the legacy behavior for PagesIndex
        // users that do not provide an owner pool.
        auto *sortColPool = ownerPool_ != nullptr
            ? ownerPool_
            : ((vecBatchCount > 0) ? inputVecBatches[0]->Get(0)->GetPool() : nullptr);
        inplaceSortColumn = VectorHelper::CreateFlatVector<typeId>(sortColPool, rowCount);
        if (totalNullCount == 0) {
            // all batches have no null value
            PartitionNonNull<typeId>();
        } else {
            // support nulls first and nulls last
            PartitionNull<typeId>(nullFirst);
        }
        inputVecBatches.clear();
    }

    void Sort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int32_t from, int32_t to);

    void SortWithRadixSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int32_t from, int32_t to);

    void SortInplace(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int32_t from, int32_t to);

    void GetOutput(int32_t *outputCols, int32_t outputColsCount, omniruntime::vec::VectorBatch *outputVecBatch,
        const int32_t *sourceTypes, int32_t offset, int32_t length, int32_t outputIndex = 0) const;

    void GetOutputInplaceSort(int32_t *outputCols, int32_t outputColsCount,
        omniruntime::vec::VectorBatch *outputVecBatch, const int32_t *sourceTypes, int32_t offset,
        int32_t length) const;

    void GetOutputRadixSort(int32_t *outputCols, int32_t outputColsCount, omniruntime::vec::VectorBatch *outputVecBatch,
        const int32_t *sourceTypes, int32_t offset, int32_t length) const;

    void SetSpillVecBatch(vec::VectorBatch *spillVecBatch, std::vector<int32_t> &outputCols, uint64_t rowOffset,
        bool canInplaceSort, bool canRadixSort);

    void Clear();

    ALWAYS_INLINE const DataTypes &GetTypes() const
    {
        return dataTypes;
    }

    ALWAYS_INLINE int32_t GetTypesCount() const
    {
        return typesCount;
    }

    ALWAYS_INLINE uint64_t *GetValueAddresses() const
    {
        return this->valueAddresses;
    }

    ALWAYS_INLINE int64_t GetRowCount() override
    {
        return this->rowCount;
    }

    ALWAYS_INLINE omniruntime::vec::BaseVector ***GetColumns() const
    {
        return this->columns;
    }

    ALWAYS_INLINE bool HasDictionary(uint32_t index) const
    {
        return hasDictionaries[index];
    }

    ALWAYS_INLINE bool HasNull(uint32_t index) const
    {
        return hasNulls[index];
    }

    ALWAYS_INLINE size_t GetVectorBatchSize() const
    {
        return inputVecBatches.size();
    }

    ALWAYS_INLINE uint64_t GetRetainedBytes() const { return retainedBytes; }

private:
    omniruntime::mem::OmniTaskMemoryPool *ownerPool_{nullptr};
    void ColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
        int32_t currentCol);

    template <typename RawType>
    void ColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
        int32_t currentCol);

    void VarcharColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
        int32_t currentCol);

    void ArrayColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
        int32_t currentCol);

    void StructColumnarSort(const int32_t *sortCols, const int32_t *sortAscendings, const int32_t *sortNullFirsts,
        int32_t sortColCount, int64_t *values, std::vector<uint32_t> &varcharLength, int32_t from, int32_t to,
        int32_t currentCol);

    template <DataTypeId typeId> void PartitionNonNull()
    {
        using T = typename NativeType<typeId>::type;
        auto vecBatchCount = static_cast<int32_t>(inputVecBatches.size());
        int32_t valueIndex = 0;
        for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
            VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
            auto rowCount = vecBatch->GetRowCount();
            auto *col = reinterpret_cast<Vector<T> *>(vecBatch->Get(0));
            for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
                reinterpret_cast<Vector<T> *>(inplaceSortColumn)->SetValue(valueIndex++, col->GetValue(rowIdx));
            }
            VectorHelper::FreeVecBatch(vecBatch);
        }
    }

    template <DataTypeId typeId> void PartitionNull(int32_t nullFirst)
    {
        using T = typename NativeType<typeId>::type;
        auto vecBatchCount = static_cast<int32_t>(inputVecBatches.size());
        auto values = reinterpret_cast<T *>(VectorHelper::UnsafeGetValues(inplaceSortColumn));
        auto nulls = unsafe::UnsafeBaseVector::GetNullsHelper(inplaceSortColumn);

        // init values, nulls position
        if (nullFirst) {
            values += totalNullCount;
        } else {
            *nulls += rowCount - totalNullCount;
        }

        int32_t valueIndex = 0;
        int32_t nullIndex = 0;
        for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
            VectorBatch *vecBatch = inputVecBatches[vecBatchIdx];
            auto rowCount = vecBatch->GetRowCount();
            auto *col = reinterpret_cast<Vector<T> *>(vecBatch->Get(0));
            if (!col->HasNull()) {
                for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
                    values[valueIndex++] = col->GetValue(rowIdx);
                }
            } else {
                for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
                    if (UNLIKELY(col->IsNull(rowIdx))) {
                        nulls->SetNull(nullIndex++, true);
                    } else {
                        values[valueIndex++] = col->GetValue(rowIdx);
                    }
                }
            }
            VectorHelper::FreeVecBatch(vecBatch);
        }
    }

    template <DataTypeId typeId, bool hasNull, bool hasNegative>
    ALWAYS_INLINE void FillRadixDataChunk(const int32_t sortCol, const bool nullsFirst);

    template <DataTypeId dataTypeId>
    void SortInplace(int32_t sortAscending, int32_t sortNullFirst, int32_t from, int32_t to);

    uint32_t typesCount;
    uint32_t rowCount = 0;
    uint64_t retainedBytes = 0;
    uint32_t radixValueWidth;
    uint32_t radixRowWidth;

    uint32_t radixSortingSize;

    int64_t totalNullCount = 0;

    uint64_t *valueAddresses = nullptr;

    std::vector<bool> hasDictionaries;
    std::vector<bool> hasNulls;
    std::vector<uint8_t> radixComboRow;
    std::vector<omniruntime::vec::VectorBatch *> inputVecBatches;

    const DataTypes dataTypes;
    omniruntime::vec::BaseVector ***columns = nullptr; // Vector* [columnIndex][tableIndex]
    omniruntime::vec::BaseVector *inplaceSortColumn = nullptr;

    bool pdqSortEnabled = false;
    bool inplacePdqSortEnabled = false;
    bool timSortEnabled = false;
};

constexpr uint32_t SHIFT_SIZE_32 = 32;

static ALWAYS_INLINE uint64_t EncodeSyntheticAddress(uint32_t sliceIndex, uint32_t sliceOffset)
{
    return (static_cast<uint64_t>(sliceIndex) << SHIFT_SIZE_32) | sliceOffset;
}

static ALWAYS_INLINE uint32_t DecodeSliceIndex(uint64_t sliceAddress)
{
    return static_cast<uint32_t>(sliceAddress >> SHIFT_SIZE_32);
}

static ALWAYS_INLINE uint32_t DecodePosition(uint64_t sliceAddress)
{
    return static_cast<uint32_t>(sliceAddress);
}

template <bool columnsNullFlag, int32_t sortAscendings>
static int32_t ALWAYS_INLINE Compare(const int32_t sortNullFirsts, const uint64_t *valueAddresses,
    omniruntime::vec::BaseVector **columns, int32_t leftPosition, int32_t rightPosition,
    omniruntime::op::OperatorUtil::CompareFunc compareFunc)
{
    int64_t leftValueAddress = valueAddresses[leftPosition];
    uint32_t leftColumnIndex = DecodeSliceIndex(leftValueAddress);
    uint32_t leftColumnPosition = DecodePosition(leftValueAddress);
    uint64_t rightValueAddress = valueAddresses[rightPosition];
    uint32_t rightColumnIndex = DecodeSliceIndex(rightValueAddress);
    uint32_t rightColumnPosition = DecodePosition(rightValueAddress);

    vec::BaseVector *leftColumn = columns[leftColumnIndex];
    vec::BaseVector *rightColumn = columns[rightColumnIndex];

    int32_t compare = omniruntime::op::OperatorUtil::COMPARE_STATUS_OTHER;
    if constexpr (columnsNullFlag) {
        compare = omniruntime::op::OperatorUtil::CompareNull(leftColumn, leftColumnPosition, rightColumn,
            rightColumnPosition, sortNullFirsts);
        if (compare == omniruntime::op::OperatorUtil::COMPARE_STATUS_OTHER) {
            compare = compareFunc(leftColumn, leftColumnPosition, rightColumn, rightColumnPosition);
            if constexpr (sortAscendings == 0) {
                compare = -compare;
            }
        }
    } else {
        // neither the left nor the right is NULL
        compare = compareFunc(leftColumn, leftColumnPosition, rightColumn, rightColumnPosition);
        if constexpr (sortAscendings == 0) {
            compare = -compare;
        }
    }

    return compare;
}
}
}
#endif
