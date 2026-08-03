/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RowVector  implementation
 */

#include "row_vector.h"
#include "vector_helper.h"
#include "util/debug.h"

namespace omniruntime::vec {
namespace {
    /** Flat/dictionary string columns use the same physical vector; only logical CHAR vs VARCHAR may differ. */
    bool BothOmniCharOrVarchar(type::DataTypeId a, type::DataTypeId b)
    {
        const bool aStr = (a == type::OMNI_VARCHAR || a == type::OMNI_CHAR);
        const bool bStr = (b == type::OMNI_VARCHAR || b == type::OMNI_CHAR);
        return aStr && bStr;
    }
    void SetChildNull(BaseVector *child, int32_t index)
    {
        if (child == nullptr) {
            return;
        }

        switch (child->GetTypeId()) {
            case type::OMNI_VARCHAR:
            case type::OMNI_CHAR:
            case type::OMNI_VARBINARY:
                static_cast<Vector<LargeStringContainer<std::string_view>> *>(child)->SetNull(index);
                break;
            case type::OMNI_ARRAY:
                static_cast<ArrayVector *>(child)->SetNull(index);
                break;
            case type::OMNI_MAP:
                static_cast<MapVector *>(child)->SetNull(index);
                break;
            case type::OMNI_ROW:
                static_cast<RowVector *>(child)->SetNull(index);
                break;
            default:
                child->SetNull(index);
                break;
        }
    }

} // namespace

    void RowVector::SetNull(int64_t index)
    {
        BaseVector::SetNull(static_cast<int32_t>(index));
        for (auto &child : children_) {
            SetChildNull(child.get(), static_cast<int32_t>(index));
        }
        for (auto *rawChild : rawChildren_) {
            SetChildNull(rawChild, static_cast<int32_t>(index));
        }
    }

    RowVector *RowVector::Slice(int positionOffset, int length, bool isCopy)
    {
        if (UNLIKELY(positionOffset + length > size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", positionOffset + length,
                                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto sliced = new RowVector(length);
        sliced->offset = offset + positionOffset; // update offset
        sliced->isSliced = true;
        for (int i = 0; i < length; ++i) {
            if (IsNull(positionOffset + i)) {
                sliced->SetNull(i);
            }
        }
        for (int i = 0; i < children_.size(); ++i) {
            sliced->AddChild(std::shared_ptr<BaseVector>(ChildAt(i)->Slice(positionOffset, length, isCopy)));
        }
        return sliced;
    }

    void RowVector::Expand(int32_t needCapacity)
    {
        if (needCapacity <= size) {
            return;
        }

        if (needCapacity <= capacity) {
            size = needCapacity;
            for (auto &child : children_) {
                if (child) {
                    child->Expand(needCapacity);
                }
            }
            return;
        }

        int32_t newCapacity = std::max(capacity * 2, needCapacity);
        int32_t oldSize = size;

        auto oldNullsBuffer = nullsBuffer;
        nullsBuffer = std::make_shared<NullsBuffer>(newCapacity);
        if (oldNullsBuffer != nullptr) {
            nullsBuffer->SetNulls(0, oldNullsBuffer.get(), oldSize);
        } else {
            nullsBuffer->SetNulls(0, false, newCapacity);
        }

        capacity = newCapacity;
        size = needCapacity;

        for (auto &child : children_) {
            if (child) {
                child->Expand(needCapacity);
            }
        }
    }

    RowVector *RowVector::CopyPositions(const int *positions, int positionOffset, int length)
    {
        if (UNLIKELY((positions == nullptr) || (length < 0))) {
            std::string message = "StructVector positions is null or the input length is incorrect: " + std::to_string(length) + ".";
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        RowVector* newRowVector = new RowVector(static_cast<int32_t>(length));
        const int* startPositions = positions + positionOffset;
        for (int32_t i = 0; i < length; i++) {
            int position = startPositions[i];
            if (UNLIKELY(IsNull(position))) {
                newRowVector->SetNull(i);
            }
        }

        for (int i = 0; i < children_.size(); i++) {
            newRowVector->AddChild(children_[i]->CopyPositions(positions, positionOffset, length));
        }
        return newRowVector;
    }

    void RowVector::Append(BaseVector *other, int positionOffset, int length) {
        if (UNLIKELY(other == nullptr || positionOffset < 0 || length <= 0)) {
            std::string message = "Invalid input for RowVector::Append";
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        auto *otherRowVector = static_cast<RowVector *>(other);
        if (UNLIKELY(otherRowVector == nullptr)) {
            LogError("RowVector::Append: other is not a RowVector (cast failed), positionOffset=%d length=%d",
                positionOffset, length);
            std::string message = "RowVector::Append expects another RowVector";
            throw OmniException("TYPE_MISMATCH_ERROR", message);
        }
        if (UNLIKELY(children_.size() != otherRowVector->ChildSize())) {
            LogError(
                "RowVector::Append: child count mismatch: dstChildCount=%zu srcChildCount=%zu positionOffset=%d length=%d",
                children_.size(), static_cast<size_t>(otherRowVector->ChildSize()), positionOffset, length);
            std::string message = "RowVector child count mismatch: " +
                                  std::to_string(children_.size()) + " vs " +
                                  std::to_string(otherRowVector->ChildSize());
            throw OmniException("TYPE_MISMATCH_ERROR", message);
        }

        int32_t newSize = positionOffset + length;
        Expand(newSize);

        for (int i = 0; i < length; i++) {
            int destIndex = positionOffset + i;
            if (otherRowVector->IsNull(i)) {
                SetNull(destIndex);
            }
        }

        for (int i = 0 ; i < children_.size(); i++) {
            BaseVector* child = children_[i].get();
            BaseVector* otherChild = otherRowVector->ChildAt(i).get();
            type::DataTypeId dti = child->GetTypeId();
            type::DataTypeId sti = otherChild->GetTypeId();
            if (UNLIKELY(dti != sti)) {
                if (BothOmniCharOrVarchar(dti, sti)) {
                    // Expected when plan uses VARCHAR but scan/struct child is CHAR (or vice versa); avoid LogWarn per row.
                    LogDebug(
                        "RowVector::Append: CHAR/VARCHAR compatible append at struct field index=%d: "
                        "dstTypeId=%d dstEncoding=%d srcTypeId=%d srcEncoding=%d",
                        i, static_cast<int>(dti), static_cast<int>(child->GetEncoding()),
                        static_cast<int>(sti), static_cast<int>(otherChild->GetEncoding()));
                } else {
                    LogError(
                        "RowVector::Append: child type mismatch at struct field index=%d: "
                        "dstTypeId=%d dstEncoding=%d srcTypeId=%d srcEncoding=%d "
                        "positionOffset=%d length=%d dstRowSize=%d srcRowSize=%d",
                        i, static_cast<int>(dti), static_cast<int>(child->GetEncoding()),
                        static_cast<int>(sti), static_cast<int>(otherChild->GetEncoding()),
                        positionOffset, length, GetSize(), otherRowVector->GetSize());
                    std::string message = "RowVector child type mismatch at index " + std::to_string(i);
                    throw OmniException("TYPE_MISMATCH_ERROR", message);
                }
            }

            child->Expand(newSize);
            VectorHelper::AppendVector(child, positionOffset, otherChild, length);
        }
        // When source row is null, parent was SetNull but children got copied; clear children at null rows to avoid residual data.
        for (int i = 0; i < length; i++) {
            if (otherRowVector->IsNull(i)) {
                int destIndex = positionOffset + i;
                for (int c = 0; c < static_cast<int>(children_.size()); c++) {
                    SetChildNull(children_[c].get(), destIndex);
                }
            }
        }
    }

    std::vector<BaseVector*> RowVector::GetValue(int index)
    {
        if (UNLIKELY(index < 0 || index >= size)) {
            std::string message("slice vector out of range(needed size:%d, real size:%d).", index,
                size);
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }
        auto childSize = ChildSize();
        std::vector<BaseVector*> childs(childSize);
        for (auto i = 0; i < childSize; i++) {
            auto child = ChildAt(i);
            BaseVector* baseVector = child.get()->Slice(index, 1, false);
            childs[i] = baseVector;
        }
        return childs;
    }

    void RowVector::SetValue(int index, RowVector* value)
    {
        if (value == nullptr) {
            SetNull(index);
            return;
        }

        auto childSize = ChildSize();
        if (childSize != value->ChildSize()) {
            std::string message("RowVector size not match.");
            throw OmniException("OPERATOR_RUNTIME_ERROR", message);
        }

        if (value->IsNull(0)) {
            SetNull(index);
            return;
        }

        SetNotNull(index);
        for (auto i = 0; i < childSize; i++) {
            auto child = ChildAt(i).get();
            auto valueChild = value->ChildAt(i).get();
            VectorHelper::AppendVector(child, index, valueChild, 1);
        }
    }
}
