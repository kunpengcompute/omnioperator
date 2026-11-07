/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayContains function implementation for checking if array contains a value
 */

#include "ArrayContainsFunction.h"
#include "vector/vector_helper.h"
#include "type/string_Impl.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

void ArrayContainsFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
    ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("ArrayContainsFunction Error:", "Requires exactly 2 arguments");
    }

    // Stack order: top = search value, bottom = array
    auto searchArg = args.top();
    args.pop();
    auto searchGuard = MakeVectorArgGuard(searchArg);
    auto arrayArg = args.top();
    args.pop();
    auto arrayGuard = MakeVectorArgGuard(arrayArg);

    if (arrayArg == nullptr || searchArg == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Input vector is null");
    }

    if (arrayArg->GetTypeId() != OMNI_ARRAY) {
        OMNI_THROW("ArrayContainsFunction Error:", "First argument is not an array type");
    }

    auto *arrayVec = dynamic_cast<ArrayVector *>(arrayArg);
    if (arrayVec == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Failed to cast to ArrayVector");
    }

    int32_t rowSize = context->GetResultRowSize();

    result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowSize);
    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    if (resultVec == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Failed to create result vector");
    }

    auto elementVec = arrayVec->GetElementVector();
    DataTypeId elementTypeId = OMNI_INT;
    if (elementVec != nullptr) {
        elementTypeId = elementVec->GetTypeId();
    } else {
        elementTypeId = searchArg->GetTypeId();
    }

    switch (elementTypeId) {
        case OMNI_BYTE:
            ProcessArrayContains<int8_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_SHORT:
            ProcessArrayContains<int16_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            ProcessArrayContains<int32_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            ProcessArrayContains<int64_t>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_FLOAT:
            ProcessArrayContains<float>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_DOUBLE:
            ProcessArrayContains<double>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_BOOLEAN:
            ProcessArrayContains<bool>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_DECIMAL128:
            ProcessArrayContains<Decimal128>(arrayVec, searchArg, resultVec, rowSize);
            break;
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            ProcessArrayContainsVarchar(arrayVec, searchArg, resultVec, rowSize);
            break;
        default:
            OMNI_THROW("ArrayContainsFunction Error:",
                "Unsupported element type: " + std::to_string(elementTypeId));
    }
}

template <typename T>
void ArrayContainsFunction::ProcessArrayContains(ArrayVector *arrayVec, BaseVector *searchVec,
    Vector<bool> *resultVec, int32_t rowSize) const
{
    auto elementVec = arrayVec->GetElementVector();

    // Handle case where element vector is null (all arrays empty)
    if (elementVec == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            if (arrayVec->IsNull(row)) {
                resultVec->SetNull(row);
            } else {
                resultVec->SetValue(row, false);
            }
        }
        return;
    }

    auto *typedElementVec = dynamic_cast<Vector<T> *>(elementVec.get());
    if (typedElementVec == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Element vector type mismatch");
    }

    int64_t *offsets = arrayVec->GetOffsets();

    // Get search value - support both ConstVector and FlatVector
    bool isConstSearch = (searchVec->GetEncoding() == OMNI_ENCODING_CONST);

    T constSearchValue {};
    bool constSearchIsNull = false;
    if (isConstSearch) {
        auto *constVec = dynamic_cast<ConstVector<T> *>(searchVec);
        if (constVec == nullptr) {
            OMNI_THROW("ArrayContainsFunction Error:", "Search value type mismatch");
        }
        constSearchIsNull = searchVec->IsNull(0);
        if (!constSearchIsNull) {
            constSearchValue = constVec->GetConstValue();
        }
    }

    auto *typedSearchVec = isConstSearch ? nullptr : dynamic_cast<Vector<T> *>(searchVec);
    if (!isConstSearch && typedSearchVec == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Search value type mismatch");
    }

    for (int32_t row = 0; row < rowSize; ++row) {
        // If array itself is null, result is null
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        // Get search value for this row
        bool searchIsNull = isConstSearch ? constSearchIsNull : searchVec->IsNull(row);
        T searchValue {};
        if (!searchIsNull) {
            searchValue = isConstSearch ? constSearchValue : typedSearchVec->GetValue(row);
        }

        // Spark semantics: if search value is NULL, result is NULL
        if (searchIsNull) {
            resultVec->SetNull(row);
            continue;
        }

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];
        int64_t arrayLength = endOffset - startOffset;

        // Empty array returns false
        if (arrayLength == 0) {
            resultVec->SetValue(row, false);
            continue;
        }

        bool found = false;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (typedElementVec->IsNull(static_cast<int32_t>(i))) {
                hasNull = true;
                continue;
            }

            T elementValue = typedElementVec->GetValue(static_cast<int32_t>(i));

            // Special handling for floating-point NaN comparison
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(searchValue) && std::isnan(elementValue)) {
                    found = true;
                    break;
                }
            }

            if (elementValue == searchValue) {
                found = true;
                break;
            }
        }

        if (found) {
            resultVec->SetValue(row, true);
        } else if (hasNull) {
            // Spark three-valued logic: value not found but array has nulls → NULL
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, false);
        }
    }
}

void ArrayContainsFunction::ProcessArrayContainsVarchar(ArrayVector *arrayVec, BaseVector *searchVec,
    Vector<bool> *resultVec, int32_t rowSize) const
{
    auto elementVec = arrayVec->GetElementVector();

    if (elementVec == nullptr) {
        for (int32_t row = 0; row < rowSize; ++row) {
            if (arrayVec->IsNull(row)) {
                resultVec->SetNull(row);
            } else {
                resultVec->SetValue(row, false);
            }
        }
        return;
    }

    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
    auto *typedElementVec = dynamic_cast<VarcharVector *>(elementVec.get());
    if (typedElementVec == nullptr) {
        OMNI_THROW("ArrayContainsFunction Error:", "Element vector type mismatch for varchar");
    }

    int64_t *offsets = arrayVec->GetOffsets();
    bool isConstSearch = (searchVec->GetEncoding() == OMNI_ENCODING_CONST);

    std::string_view constSearchValue;
    bool constSearchIsNull = false;
    if (isConstSearch) {
        constSearchIsNull = searchVec->IsNull(0);
        if (!constSearchIsNull) {
            constSearchValue = VectorHelper::GetStringValueFromVector(searchVec, 0);
        }
    }

    for (int32_t row = 0; row < rowSize; ++row) {
        if (arrayVec->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        bool searchIsNull = isConstSearch ? constSearchIsNull : searchVec->IsNull(row);
        if (searchIsNull) {
            resultVec->SetNull(row);
            continue;
        }

        std::string_view searchValue = isConstSearch ? constSearchValue
            : VectorHelper::GetStringValueFromVector(searchVec, row);

        int64_t startOffset = offsets[row];
        int64_t endOffset = offsets[row + 1];
        int64_t arrayLength = endOffset - startOffset;

        if (arrayLength == 0) {
            resultVec->SetValue(row, false);
            continue;
        }

        bool found = false;
        bool hasNull = false;

        for (int64_t i = startOffset; i < endOffset; ++i) {
            if (typedElementVec->IsNull(static_cast<int32_t>(i))) {
                hasNull = true;
                continue;
            }

            std::string_view elementValue = typedElementVec->GetValue(static_cast<int32_t>(i));
            if (elementValue == searchValue) {
                found = true;
                break;
            }
        }

        if (found) {
            resultVec->SetValue(row, true);
        } else if (hasNull) {
            resultVec->SetNull(row);
        } else {
            resultVec->SetValue(row, false);
        }
    }
}

// Explicit template instantiations
template void ArrayContainsFunction::ProcessArrayContains<int8_t>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<int16_t>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<int32_t>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<int64_t>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<float>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<double>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<bool>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;
template void ArrayContainsFunction::ProcessArrayContains<Decimal128>(ArrayVector *, BaseVector *,
    Vector<bool> *, int32_t) const;

} // namespace omniruntime::vectorization
