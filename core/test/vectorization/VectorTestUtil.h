/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: shared vector builders for the lazy IF / CASE unit tests
 */

#ifndef VECTOR_TEST_UTIL_H__
#define VECTOR_TEST_UTIL_H__

#include <string>
#include <utility>
#include <vector>

#include "type/data_type.h"
#include "vector/array_vector.h"
#include "vector/large_string_container.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::VectorTestUtil {
using namespace omniruntime::vec;
using namespace omniruntime::type;

template<typename T>
inline BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(typeId, values.size());
    auto* typedVec = static_cast<Vector<T>*>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, values[i]);
    }
    return vec;
}

inline BaseVector* CreateBooleanVector(const std::vector<bool>& values)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, values.size());
    auto* typedVec = static_cast<Vector<bool>*>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, values[i]);
    }
    return vec;
}

inline BaseVector* CreateStringVector(const std::vector<std::string>& values, DataTypeId typeId = OMNI_VARCHAR)
{
    BaseVector* vec = VectorHelper::CreateFlatVector(typeId, values.size());
    auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        std::string_view sv(values[i]);
        typedVec->SetValue(i, sv);
    }
    return vec;
}

inline BaseVector* CreateArrayVector(const std::vector<std::vector<int32_t>>& arrays)
{
    int32_t totalElements = 0;
    for (const auto& arr : arrays) {
        totalElements += arr.size();
    }
    auto elemVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(OMNI_INT, totalElements));
    auto* arrayVec = new ArrayVector(arrays.size(), elemVec);

    for (size_t row = 0; row < arrays.size(); ++row) {
        const auto& arr = arrays[row];
        auto* tempElem = VectorHelper::CreateFlatVector(OMNI_INT, arr.size());
        auto* typedElem = static_cast<Vector<int32_t>*>(tempElem);
        for (size_t i = 0; i < arr.size(); ++i) {
            typedElem->SetValue(i, arr[i]);
        }
        arrayVec->SetValue(row, tempElem);
        delete tempElem;
    }
    return arrayVec;
}

inline BaseVector* CreateMapVector(const std::vector<std::vector<std::pair<std::string, int32_t>>>& maps)
{
    int32_t totalEntries = 0;
    for (const auto& m : maps) {
        totalEntries += m.size();
    }

    auto keyVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(OMNI_VARCHAR, totalEntries));
    auto valVec = std::shared_ptr<BaseVector>(
        VectorHelper::CreateFlatVector(OMNI_INT, totalEntries));
    auto* mapVec = new MapVector(maps.size(), keyVec, valVec);

    int32_t offset = 0;
    for (size_t row = 0; row < maps.size(); ++row) {
        const auto& m = maps[row];
        for (size_t i = 0; i < m.size(); ++i) {
            auto* typedKeys = static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec.get());
            auto* typedVals = static_cast<Vector<int32_t>*>(valVec.get());
            typedKeys->Expand(offset + i + 1);
            typedVals->Expand(offset + i + 1);
            std::string_view keySv(m[i].first);
            typedKeys->SetValue(offset + i, keySv);
            typedVals->SetValue(offset + i, m[i].second);
        }
        mapVec->SetSize(row, m.size());
        offset += m.size();
    }
    return mapVec;
}

inline BaseVector* CreateRowVector(const std::vector<std::pair<std::string, int32_t>>& rows)
{
    int32_t size = rows.size();
    auto* rowVec = new RowVector(size);

    auto* strChild = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto* intChild = VectorHelper::CreateFlatVector(OMNI_INT, size);
    auto* typedStr = static_cast<Vector<LargeStringContainer<std::string_view>>*>(strChild);
    auto* typedInt = static_cast<Vector<int32_t>*>(intChild);

    for (size_t i = 0; i < rows.size(); ++i) {
        std::string_view sv(rows[i].first);
        typedStr->SetValue(i, sv);
        typedInt->SetValue(i, rows[i].second);
    }

    rowVec->AddChild(strChild);
    rowVec->AddChild(intChild);
    return rowVec;
}
}
#endif // VECTOR_TEST_UTIL_H__
