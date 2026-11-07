/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Helpers for max_by/min_by when target column (col1) is OMNI_ARRAY, OMNI_MAP, or OMNI_ROW.
 *              These types do not store one value per row in a flat buffer; element access uses type-specific APIs.
 *
 * - ArrayVector: one element vector + offsets; GetValue(i) returns Slice of elements for row i; SetValue(i, slice) appends slice to element vector and sets offset/size.
 * - MapVector: key vector + value vector + offsets; Slice(i, 1) returns one-row MapVector; set by appending slice's keys/values to output's key/value vectors and setting offset/size.
 * - RowVector: child vectors per column; Slice(i, 1) returns one-row RowVector; Append(slice, rowIndex, 1) copies one row from slice into output at rowIndex.
 */
#ifndef OMNI_RUNTIME_COMPLEX_AGGREGATOR_UTIL_H
#define OMNI_RUNTIME_COMPLEX_AGGREGATOR_UTIL_H

#include <memory>
#include <set>
#include <unordered_set>

#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vector/large_string_container.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "operator/util/operator_util.h"

namespace omniruntime {
namespace op {

/**
 * True if the slice represents a null value (caller should not copy and should output null).
 * - slice == nullptr: row was null at source (GetComplexColSlice returned nullptr).
 * - Map/Row slice with one row and that row IsNull(0): slice represents null.
 */
inline bool IsComplexSliceNull(vec::BaseVector *slice, type::DataTypeId colTypeId)
{
    if (slice == nullptr) {
        return true;
    }
    if (colTypeId != type::OMNI_ARRAY && slice->GetSize() >= 1 && slice->IsNull(0)) {
        return true;
    }
    return false;
}

/**
 * True if this row should be skipped for aggregation (target is null; do not update state).
 * Use when we must only consider rows with non-null target (Spark semantics: only update with valid target).
 * - IsComplexSliceNull(slice): already null.
 * - Array/Map: slice has 0 size and colVector->IsNull(rowIndex) (null stored as 0-size without null bit).
 */
inline bool ShouldSkipRowTargetNull(vec::BaseVector *slice, type::DataTypeId colTypeId,
    vec::BaseVector *colVector, int32_t rowIndex)
{
    if (IsComplexSliceNull(slice, colTypeId)) {
        return true;
    }
    if (colTypeId == type::OMNI_ARRAY) {
        if (static_cast<int32_t>(slice->GetSize()) == 0 && colVector->IsNull(rowIndex)) {
            return true;
        }
    } else if (colTypeId == type::OMNI_MAP) {
        auto *mapSlice = static_cast<vec::MapVector *>(slice);
        if (mapSlice->GetSize(0) == 0 && colVector->IsNull(rowIndex)) {
            return true;
        }
    }
    return false;
}

/**
 * Get the value at rowIndex from a complex-type column as a slice (view).
 * The returned vector is newly allocated and owned by the caller (hold it in a ComplexSliceHolder);
 * it shares buffers with colVector, so it is only valid while colVector is alive and unmodified.
 * Returns nullptr when the row is null (so caller can treat uniformly via IsComplexSliceNull).
 *
 * - OMNI_ARRAY: ArrayVector::GetValue(rowIndex) -> BaseVector* (element Slice for that row).
 * - OMNI_MAP: MapVector::Slice(rowIndex, 1) -> MapVector* (one row).
 * - OMNI_ROW: RowVector::Slice(rowIndex, 1) -> RowVector* (one row).
 */
inline vec::BaseVector *GetComplexColSlice(vec::BaseVector *colVector, type::DataTypeId colTypeId, int32_t rowIndex)
{
    if (colVector == nullptr) {
        return nullptr;
    }
    switch (colTypeId) {
        case type::OMNI_ARRAY:
            if (colVector->IsNull(rowIndex)) {
                return nullptr;
            }
            return static_cast<vec::ArrayVector *>(colVector)->GetValue(rowIndex);
        case type::OMNI_MAP:
            if (colVector->IsNull(rowIndex)) {
                return nullptr;
            }
            return static_cast<vec::MapVector *>(colVector)->Slice(rowIndex, 1);
        case type::OMNI_ROW:
            if (colVector->IsNull(rowIndex)) {
                return nullptr;
            }
            return static_cast<vec::RowVector *>(colVector)->Slice(rowIndex, 1);
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "GetComplexColSlice: unsupported target col type " + std::to_string(static_cast<int>(colTypeId)));
    }
}

/**
 * Copy a complex-type slice to a newly allocated vector that the caller owns.
 * Used when storing slice in aggregator state so the pointer remains valid after the source batch is released.
 * Caller must delete the returned pointer (or use ReleaseComplexSliceCopy).
 * @param colDataType optional column DataType; when colTypeId is OMNI_ARRAY and slice is empty, used to create
 *        element vector via CreateEmptyComplexVector (correct for both flat and complex element types).
 */
inline vec::BaseVector *CopyComplexSliceToOwned(vec::BaseVector *slice, type::DataTypeId colTypeId,
    type::DataType *colDataType = nullptr)
{
    if (slice == nullptr) {
        return nullptr;
    }
    switch (colTypeId) {
        case type::OMNI_ARRAY: {
            // slice is element-vector slice for one row (from ArrayVector::GetValue).
            // Return a copy of the element vector; SetComplexColValue(ArrayVector::SetValue) expects this.
            int32_t n = static_cast<int32_t>(slice->GetSize());
            if (n == 0) {
                if (colDataType != nullptr && colDataType->GetId() == type::OMNI_ARRAY) {
                    type::ArrayType *arrayType = static_cast<type::ArrayType *>(colDataType);
                    return vec::VectorHelper::CreateEmptyComplexVector(arrayType->ElementType().get(), 0);
                }
                type::DataTypePtr elemType = vec::VectorHelper::GetDataType(slice);
                return vec::VectorHelper::CreateEmptyComplexVector(elemType.get(), 0);
            }
            std::vector<int> positions(static_cast<size_t>(n));
            for (int32_t i = 0; i < n; i++) {
                positions[i] = i;
            }
            return vec::VectorHelper::CopyPositionsVector(slice, positions.data(), 0, n);
        }
        case type::OMNI_MAP: {
            int pos = 0;
            return vec::VectorHelper::CopyPositionsVector(slice, &pos, 0, 1);
        }
        case type::OMNI_ROW: {
            int pos = 0;
            return vec::VectorHelper::CopyPositionsVector(slice, &pos, 0, 1);
        }
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "CopyComplexSliceToOwned: unsupported target col type " + std::to_string(static_cast<int>(colTypeId)));
    }
}

/**
 * Release a complex slice copy previously returned by CopyComplexSliceToOwned.
 */
inline void ReleaseComplexSliceCopy(vec::BaseVector *ownedSlice, type::DataTypeId colTypeId)
{
    if (ownedSlice == nullptr) {
        return;
    }
    delete ownedSlice;
}

using ComplexSliceHolder = std::unique_ptr<vec::BaseVector>;

/**
 * Owns the CopyComplexSliceToOwned copies an aggregator stores in its group states.
 * States live in the operator arena and DestroyState is never invoked on them, so copies still held
 * when the aggregator is destroyed are released here.
 */
class ComplexValueOwner {
public:
    ComplexValueOwner() = default;
    ComplexValueOwner(const ComplexValueOwner &) = delete;
    ComplexValueOwner &operator=(const ComplexValueOwner &) = delete;

    ~ComplexValueOwner()
    {
        for (vec::BaseVector *value : values_) {
            delete value;
        }
    }

    vec::BaseVector *Copy(vec::BaseVector *slice, type::DataTypeId colTypeId, type::DataType *colDataType = nullptr)
    {
        ComplexSliceHolder value(CopyComplexSliceToOwned(slice, colTypeId, colDataType));
        if (value != nullptr) {
            values_.insert(value.get());
        }
        return value.release();
    }

    void Release(vec::BaseVector *value)
    {
        if (value != nullptr && values_.erase(value) > 0) {
            delete value;
        }
    }

private:
    std::unordered_set<vec::BaseVector *> values_;
};

/**
 * Write one row into the output complex-type column from a slice previously obtained by GetComplexColSlice.
 * Must be called in row order (0, 1, 2, ...) for MAP so offsets remain correct.
 *
 * - OMNI_ARRAY: ArrayVector::SetValue(rowIndex, slice) appends slice elements and sets size.
 * - OMNI_MAP: Expand key/value vectors, append slice's key/value to output, then SetOffset(rowIndex), SetSize(rowIndex, sliceLen).
 * - OMNI_ROW: RowVector::Append(slice, rowIndex, 1) copies one row from slice into output at rowIndex.
 */
inline void SetComplexColValue(vec::BaseVector *outputVector, type::DataTypeId colTypeId, int32_t rowIndex,
    vec::BaseVector *slice)
{
    if (IsComplexSliceNull(slice, colTypeId)) {
        outputVector->SetNull(rowIndex);
        if (colTypeId == type::OMNI_ROW) {
            auto *outRow = static_cast<vec::RowVector *>(outputVector);
            int32_t needSize = rowIndex + 1;
            if (outRow->GetSize() < needSize) {
                outRow->Expand(needSize);
            }
            for (int32_t c = 0; c < outRow->ChildSize(); c++) {
                vec::BaseVector *child = outRow->ChildAt(c).get();
                if (child->GetSize() < needSize) {
                    child->Expand(needSize);
                }
                child->SetNull(rowIndex);
            }
        } else if (colTypeId == type::OMNI_MAP) {
            auto *outMap = static_cast<vec::MapVector *>(outputVector);
            int32_t destOffset = (rowIndex == 0) ? 0 : static_cast<int32_t>(outMap->GetOffset(rowIndex));
            outMap->SetOffset(rowIndex, destOffset);
            outMap->SetSize(rowIndex, 0);
        } else if (colTypeId == type::OMNI_ARRAY) {
            // ArrayVector::SetNull already sets size to 0; set explicitly so null is not interpreted as empty.
            static_cast<vec::ArrayVector *>(outputVector)->SetSize(rowIndex, 0);
        }
        return;
    }
    switch (colTypeId) {
        case type::OMNI_ARRAY:
            static_cast<vec::ArrayVector *>(outputVector)->SetValue(rowIndex, slice);
            break;
        case type::OMNI_MAP: {
            auto *outMap = static_cast<vec::MapVector *>(outputVector);
            auto *sliceMap = static_cast<vec::MapVector *>(slice);
            int32_t sliceLen = static_cast<int32_t>(sliceMap->GetSize(0));
            int32_t destOffset = (rowIndex == 0) ? 0 : static_cast<int32_t>(outMap->GetOffset(rowIndex));
            vec::VectorHelper::ExpandElementVector(outMap->GetKeyVector().get(), outMap->GetKeyVector()->GetTypeId(),
                destOffset + sliceLen);
            vec::VectorHelper::ExpandElementVector(outMap->GetValueVector().get(), outMap->GetValueVector()->GetTypeId(),
                destOffset + sliceLen);
            vec::VectorHelper::AppendVector(outMap->GetKeyVector().get(), destOffset, sliceMap->GetKeyVector().get(),
                sliceLen);
            vec::VectorHelper::AppendVector(outMap->GetValueVector().get(), destOffset, sliceMap->GetValueVector().get(),
                sliceLen);
            outMap->SetOffset(rowIndex, destOffset);
            outMap->SetSize(rowIndex, sliceLen);
            break;
        }
        case type::OMNI_ROW:
            static_cast<vec::RowVector *>(outputVector)->Append(slice, rowIndex, 1);
            break;
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "SetComplexColValue: unsupported target col type " + std::to_string(static_cast<int>(colTypeId)));
    }
}

// Forward declare for recursive use in detail::CompareArraySlices (array<struct>, array<array>).
inline int CompareComplexSlice(vec::BaseVector *sliceA, vec::BaseVector *sliceB, type::DataTypeId colTypeId,
    type::DataType *colDataType = nullptr);

namespace detail {

// Compare two scalar elements at the same index in two vectors; returns <0, 0, >0.
// Null is considered less than non-null (Spark nulls first ordering).
inline int CompareScalarElementsAt(vec::BaseVector *vecA, vec::BaseVector *vecB, int32_t index,
    type::DataTypeId elemTypeId)
{
    bool nullA = vecA->IsNull(index);
    bool nullB = vecB->IsNull(index);
    if (nullA && nullB) {
        return 0;
    }
    if (nullA) {
        return -1;
    }
    if (nullB) {
        return 1;
    }
    using namespace vec;
    switch (elemTypeId) {
        case type::OMNI_BOOLEAN: {
            auto a = VectorHelper::GetFlatValue<OMNI_BOOLEAN>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_BOOLEAN>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_BYTE: {
            auto a = VectorHelper::GetFlatValue<OMNI_BYTE>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_BYTE>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_SHORT: {
            auto a = VectorHelper::GetFlatValue<OMNI_SHORT>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_SHORT>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_INT:
        case type::OMNI_DATE32:
        case type::OMNI_TIME32: {
            auto a = VectorHelper::GetFlatValue<OMNI_INT>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_INT>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_LONG:
        case type::OMNI_DATE64:
        case type::OMNI_TIME64:
        case type::OMNI_TIMESTAMP:
        case type::OMNI_DECIMAL64: {
            auto a = VectorHelper::GetFlatValue<OMNI_LONG>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_LONG>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_FLOAT: {
            auto a = VectorHelper::GetFlatValue<OMNI_FLOAT>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_FLOAT>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_DOUBLE: {
            auto a = VectorHelper::GetFlatValue<OMNI_DOUBLE>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_DOUBLE>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_DECIMAL128: {
            auto a = VectorHelper::GetFlatValue<OMNI_DECIMAL128>(vecA, index);
            auto b = VectorHelper::GetFlatValue<OMNI_DECIMAL128>(vecB, index);
            return Compare(a, b);
        }
        case type::OMNI_VARCHAR:
        case type::OMNI_CHAR:
        case type::OMNI_VARBINARY: {
            std::string_view sa = VectorHelper::GetFlatValue<OMNI_VARCHAR>(vecA, index);
            std::string_view sb = VectorHelper::GetFlatValue<OMNI_VARCHAR>(vecB, index);

            int cmp = static_cast<int>(sa.compare(sb));
            return (cmp < 0 ? -1 : (cmp > 0 ? 1 : 0));
        }
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "CompareComplexSlice: unsupported element type " + std::to_string(static_cast<int>(elemTypeId)));
    }
}

// Compare two array slices (element vectors). Lexicographic: element by element, then by length.
// Supports scalar elements and nested OMNI_ARRAY / OMNI_ROW (struct) for collect_set array<struct> etc.
inline int CompareArraySlices(vec::BaseVector *sliceA, vec::BaseVector *sliceB, type::DataType *colDataType = nullptr)
{
    size_t sizeA = static_cast<size_t>(sliceA->GetSize());
    size_t sizeB = static_cast<size_t>(sliceB->GetSize());
    size_t minSize = (sizeA < sizeB) ? sizeA : sizeB;
    type::DataTypeId elemTypeId = static_cast<type::DataTypeId>(sliceA->GetTypeId());
    type::DataType *elementDataType = nullptr;
    if (colDataType != nullptr && colDataType->GetId() == type::OMNI_ARRAY) {
        elementDataType = static_cast<type::ArrayType *>(colDataType)->ElementType().get();
    }
    for (size_t i = 0; i < minSize; i++) {
        int cmp;
        if (elemTypeId == type::OMNI_ROW) {
            vec::BaseVector *subA = static_cast<vec::RowVector *>(sliceA)->Slice(static_cast<int32_t>(i), 1);
            vec::BaseVector *subB = static_cast<vec::RowVector *>(sliceB)->Slice(static_cast<int32_t>(i), 1);
            cmp = CompareComplexSlice(subA, subB, type::OMNI_ROW, elementDataType);
            delete subA;
            delete subB;
        } else if (elemTypeId == type::OMNI_ARRAY) {
            vec::BaseVector *subA = static_cast<vec::ArrayVector *>(sliceA)->GetValue(static_cast<int32_t>(i));
            vec::BaseVector *subB = static_cast<vec::ArrayVector *>(sliceB)->GetValue(static_cast<int32_t>(i));
            cmp = CompareComplexSlice(subA, subB, type::OMNI_ARRAY, elementDataType);
            delete subA;
            delete subB;
        } else {
            cmp = CompareScalarElementsAt(sliceA, sliceB, static_cast<int32_t>(i), elemTypeId);
        }
        if (cmp != 0) {
            return cmp;
        }
    }
    if (sizeA != sizeB) {
        return (sizeA < sizeB) ? -1 : 1;
    }
    return 0;
}

// Compare two row slices (RowVectors with one row each). Field-by-field. Recursive for nested ARRAY/ROW.
inline int CompareRowSlices(vec::BaseVector *sliceA, vec::BaseVector *sliceB, type::DataType *colDataType);

} // namespace detail

/**
 * Compare two complex-type slices for min/max and collect_set.
 * Returns: <0 if sliceA < sliceB, 0 if equal, >0 if sliceA > sliceB.
 * (Same as COMPARE_STATUS_LESS_THAN / EQUAL / GREATER_THAN for int32_t.)
 * Supports OMNI_ARRAY (element-by-element) and OMNI_ROW (field-by-field, recursive).
 * OMNI_MAP is not supported (Spark does not order MapType); throws if colTypeId is OMNI_MAP.
 */
inline int CompareComplexSlice(vec::BaseVector *sliceA, vec::BaseVector *sliceB, type::DataTypeId colTypeId,
    type::DataType *colDataType = nullptr)
{
    if (sliceA == nullptr || sliceB == nullptr) {
        if (sliceA == sliceB) {
            return 0;
        }
        return (sliceA == nullptr) ? -1 : 1;
    }
    switch (colTypeId) {
        case type::OMNI_ARRAY:
            return detail::CompareArraySlices(sliceA, sliceB, colDataType);
        case type::OMNI_ROW:
            return detail::CompareRowSlices(sliceA, sliceB, colDataType);
        case type::OMNI_MAP:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "CompareComplexSlice: Map type is not orderable in Spark; min/max on map not supported");
        default:
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "CompareComplexSlice: unsupported col type " + std::to_string(static_cast<int>(colTypeId)));
    }
}

namespace detail {

inline int CompareRowSlices(vec::BaseVector *sliceA, vec::BaseVector *sliceB, type::DataType *colDataType)
{
    auto *rowA = static_cast<vec::RowVector *>(sliceA);
    auto *rowB = static_cast<vec::RowVector *>(sliceB);
    int32_t numFields = rowA->ChildSize();
    if (numFields != rowB->ChildSize()) {
        return (numFields < rowB->ChildSize()) ? -1 : 1;
    }
    int32_t rowIndex = 0;
    for (int32_t c = 0; c < numFields; c++) {
        vec::BaseVector *childA = rowA->ChildAt(c).get();
        vec::BaseVector *childB = rowB->ChildAt(c).get();
        type::DataTypeId fieldTypeId = static_cast<type::DataTypeId>(childA->GetTypeId());
        if (fieldTypeId == type::OMNI_ARRAY || fieldTypeId == type::OMNI_ROW) {
            vec::BaseVector *subA = (fieldTypeId == type::OMNI_ARRAY)
                ? static_cast<vec::ArrayVector *>(childA)->GetValue(rowIndex)
                : static_cast<vec::RowVector *>(childA)->Slice(rowIndex, 1);
            vec::BaseVector *subB = (fieldTypeId == type::OMNI_ARRAY)
                ? static_cast<vec::ArrayVector *>(childB)->GetValue(rowIndex)
                : static_cast<vec::RowVector *>(childB)->Slice(rowIndex, 1);
            type::DataType *fieldDataType = (colDataType != nullptr && colDataType->GetId() == type::OMNI_ROW)
                ? static_cast<const type::RowType *>(colDataType)->Type(c).get()
                : nullptr;
            int cmp = CompareComplexSlice(subA, subB, fieldTypeId, fieldDataType);
            delete subA;
            delete subB;
            if (cmp != 0) {
                return cmp;
            }
        } else {
            int cmp = CompareScalarElementsAt(childA, childB, rowIndex, fieldTypeId);
            if (cmp != 0) {
                return cmp;
            }
        }
    }
    return 0;
}

} // namespace detail

/**
 * Compare two complex-type slices. Returns COMPARE_STATUS_LESS_THAN, COMPARE_STATUS_EQUAL, or COMPARE_STATUS_GREATER_THAN.
 * Used by collect_set with std::set for deduplication. Unified with CompareComplexSlice (same semantics).
 */
inline int32_t ComplexSliceCompare(vec::BaseVector *leftSlice, vec::BaseVector *rightSlice,
    type::DataTypeId colTypeId, type::DataType *colDataType)
{
    return static_cast<int32_t>(CompareComplexSlice(leftSlice, rightSlice, colTypeId, colDataType));
}

/**
 * Compare two complex-type slices for equality. Used by collect_set for deduplication.
 * Unified with CompareComplexSlice (same semantics).
 */
inline bool ComplexSliceEquals(vec::BaseVector *leftSlice, vec::BaseVector *rightSlice,
    type::DataTypeId colTypeId, type::DataType *colDataType)
{
    return CompareComplexSlice(leftSlice, rightSlice, colTypeId, colDataType) == 0;
}

/**
 * Comparator for std::set<BaseVector*> used by collect_set_complex.
 * Provides strict weak ordering for array and struct types (avoids Omni defaultHashMap).
 */
struct ComplexSliceSetComparator {
    type::DataTypeId colTypeId;
    type::DataType *colDataType;

    ComplexSliceSetComparator(type::DataTypeId id, type::DataType *dt) : colTypeId(id), colDataType(dt) {}

    bool operator()(vec::BaseVector *a, vec::BaseVector *b) const
    {
        return ComplexSliceCompare(a, b, colTypeId, colDataType) == OperatorUtil::COMPARE_STATUS_LESS_THAN;
    }
};

using ComplexSetType = std::set<vec::BaseVector *, ComplexSliceSetComparator>;

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_COMPLEX_AGGREGATOR_UTIL_H
