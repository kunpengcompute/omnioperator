/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "vector/vector.h"
#include "vector/dictionary_container.h"
#include "vector/vector_helper.h"

namespace omniruntime::vec::test {
template <typename CONTAINER> void string_vector_get_set_value()
{
    int vectorSize = 1000;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    std::string valuePrefix;
    valuePrefix = "hello_world__";

    for (int i = 0; i < vectorSize; i++) {
        std::string value = valuePrefix + std::to_string(i);
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }

    for (int i = 0; i < vectorSize; i++) {
        std::string value = valuePrefix + std::to_string(i);
        std::string_view output = vector->GetValue(i);
        EXPECT_EQ(value, output);
    }

    delete vector;
}

template <typename CONTAINER> void string_vector_get_set_empty_value()
{
    int vectorSize = 1000;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    std::string empty = "";
    for (int i = 0; i < vectorSize; i++) {
        std::string_view input(empty.data(), 0);
        vector->SetValue(i, input);
    }

    for (int i = 0; i < vectorSize; i++) {
        std::string_view output = vector->GetValue(i);
        EXPECT_EQ(output.size(), 0);
    }
    delete vector;
}

template <typename CONTAINER> void string_vector_size_invalid()
{
    int vectorSize = -1;
    EXPECT_ANY_THROW(auto container = std::make_shared<CONTAINER>(vectorSize));
}

template <typename CONTAINER>
void CompareResult(Vector<CONTAINER> *appended, std::vector<std::string> expected, std::vector<bool> expectedNull,
                   int32_t appendedVecSize)
{
    for (int32_t i = 0; i < appendedVecSize; i++) {
        // append empty vector or beyond the bound
        if (i >= 10) {
            EXPECT_FALSE(appended->IsNull(i));
            continue;
        }
        // append success for value check
        if (appended->IsNull(i)) {
            EXPECT_EQ(expectedNull[i], appended->IsNull(i));
            continue;
        }
        EXPECT_EQ(expected[i % 5], appended->GetValue(i));
    }
}

template <typename CONTAINER> void string_vector_append_value()
{
    int vectorSize = 5;

    auto v1BaseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *v1 = (Vector<CONTAINER> *)v1BaseVector;

    std::string valuePrefix;
    valuePrefix = "hello_world__";

    std::vector<std::string> expected;
    for (int32_t i = 0; i < vectorSize; i++) {
        std::string value = valuePrefix + std::to_string(i);
        std::string_view input(value.data(), value.size());
        v1->SetValue(i, input);
        expected.push_back(value);
    }

    int32_t appendedVecSize = 15;
    auto appendedBaseVector = VectorHelper::CreateStringVector(appendedVecSize);
    auto *appended = (Vector<CONTAINER> *)appendedBaseVector;
    appended->Append(v1, 0, vectorSize);

    auto v2BaseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *v2WithNull = (Vector<CONTAINER> *)v2BaseVector;
    for (int32_t i = 0; i < vectorSize; i++) {
        if (i % 2 == 0) {
            v2WithNull->SetNull(i);
            continue;
        }
        std::string_view input(expected[i].data(), expected[i].size());
        v2WithNull->SetValue(i, input);
    }
    appended->Append(v2WithNull, 5, 5);

    auto v3BaseVector = VectorHelper::CreateStringVector(0);
    auto *v3Emtpy = (Vector<CONTAINER> *)v3BaseVector;
    appended->Append(v3Emtpy, 10, 0);

    auto v4BaseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *v4OverBounds = (Vector<CONTAINER> *)v4BaseVector;
    for (int32_t i = 0; i < vectorSize; i++) {
        std::string_view input(expected[i].data(), expected[i].size());
        v4OverBounds->SetValue(i, input);
    }
    EXPECT_ANY_THROW(appended->Append(v4OverBounds, 10, vectorSize + 1));

    std::vector<bool> expectedNull{false, false, false, false, false, true, false, true, false, true};
    CompareResult(appended, expected, expectedNull, appendedVecSize);

    delete appended;
    delete v4OverBounds;
    delete v3Emtpy;
    delete v2WithNull;
    delete v1;
}

template <typename CONTAINER> void string_vector_copy_positions()
{
    int vectorSize = 10;
    std::string valuePrefix;

    valuePrefix = "hello_world__";

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    for (int i = 0; i < vectorSize; i++) {
        if (i % 2 == 0) {
            vector->SetNull(i);
        } else {
            std::string value = valuePrefix + std::to_string(i);
            std::string_view input(value.data(), value.size());
            vector->SetValue(i, input);
        }
    }

    int index[] = {2, 3, 4, 5, 6, 7};
    int offset1 = 0;
    int offset2 = 1;
    int copySize = 4;
    auto vectorOffsetZero = reinterpret_cast<Vector<CONTAINER> *>(vector->CopyPositions(index, offset1, copySize));
    auto vectorOffsetNotZero = reinterpret_cast<Vector<CONTAINER> *>(vector->CopyPositions(index, offset2, copySize));

    for (int32_t i = 0; i < copySize; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(vectorOffsetZero->IsNull(i), true);
            std::string_view originValue = vector->GetValue(index[i + offset2]);
            std::string_view offsetNotZeroValue = vectorOffsetNotZero->GetValue(i);
            EXPECT_EQ(offsetNotZeroValue, originValue);
            continue;
        }
        std::string_view originValue = vector->GetValue(index[i + offset1]);
        std::string_view offsetZeroValue = vectorOffsetZero->GetValue(i);
        EXPECT_EQ(vectorOffsetNotZero->IsNull(i), true);
        EXPECT_EQ(offsetZeroValue, originValue);
    }

    auto vectorEmpty = reinterpret_cast<Vector<CONTAINER> *>(vector->CopyPositions(index, offset2, 0));
    EXPECT_EQ(vectorEmpty->GetSize(), 0);
    EXPECT_ANY_THROW(vector->CopyPositions(index, offset1, -1));
    delete vectorOffsetZero;
    delete vectorOffsetNotZero;
    delete vectorEmpty;
    delete vector;
}

template <typename CONTAINER> void string_vector_slice()
{
    int vectorSize = 1000;
    int offset = 50;
    int len = 10;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *parent = (Vector<CONTAINER> *)baseVector;

    std::string valuePrefix;
    valuePrefix = "hello_world__";

    for (int i = 0; i < vectorSize; i++) {
        std::string value = valuePrefix + std::to_string(i);
        std::string_view input(value.data(), value.size());
        parent->SetValue(i, input);
    }

    auto vector = (Vector<CONTAINER> *)(parent->Slice(offset, len));
    EXPECT_EQ(vector->GetTypeId(), parent->GetTypeId());

    // WARNING:
    // setting value on encoded vector not supported
    // the following code will cause compilation error

    for (int i = 0; i < len; i++) {
        std::string value = valuePrefix + std::to_string(i + offset);
        auto output = vector->GetValue(i);
        EXPECT_EQ(value, output);
    }
    delete vector;
    delete parent;
}

template <typename CONTAINER> void string_vector_get_datatype()
{
    int vectorSize = 1000;
    DataTypeId expect = type::OMNI_CHAR;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    EXPECT_EQ(vector->GetTypeId(), expect);
    EXPECT_EQ(baseVector->GetTypeId(), expect);
    delete vector;
}

template <typename CONTAINER> void string_vector_get_used_bytes()
{
    int vectorSize = 100000;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    std::string valuePrefix;
    valuePrefix = "hello_world__";

    size_t expect = 0;
    for (int i = 0; i < vectorSize; i++) {
        std::string value = valuePrefix + std::to_string(i);
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
        expect += value.size() + 1;
    }

    size_t real =
        unsafe::UnsafeStringContainer::GetCapacityInBytes(unsafe::UnsafeStringVector::GetContainer(vector).get());
    EXPECT_LE(expect, real);
    delete vector;
}

template <typename CONTAINER> void string_vector_zero_capacity_init()
{
    int32_t vectorSize = 10;
    int32_t capacityInBytes = 0;
    auto vector = std::make_unique<Vector<CONTAINER>>(vectorSize, capacityInBytes);
    for (int i = 0; i < vectorSize; i++) {
        std::string value = "string" + std::to_string(i);
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }

    for (int i = 0; i < vectorSize; i++) {
        std::string value = "string" + std::to_string(i);
        std::string_view output(value.data(), value.size());
        EXPECT_EQ(vector->GetValue(i), output);
    }
}

TEST(vector2, vector_get_set_value_large_string)
{
    string_vector_get_set_value<LargeStringContainer<std::string_view>>();
}

TEST(vector2, vector_get_set_value_large_empty_string)
{
    string_vector_get_set_empty_value<LargeStringContainer<std::string_view>>();
}

TEST(vector2, large_string_vector_size_invalid)
{
    string_vector_size_invalid<LargeStringContainer<std::string_view>>();
}

TEST(vector2, vector_append_large_string) { string_vector_append_value<LargeStringContainer<std::string_view>>(); }

TEST(vector2, vector_copy_positions_large_string)
{
    string_vector_copy_positions<LargeStringContainer<std::string_view>>();
}

TEST(vector2, slice_container_large_string) { string_vector_slice<LargeStringContainer<std::string_view>>(); }

TEST(vector2, string_vector_get_datatype) { string_vector_get_datatype<LargeStringContainer<std::string_view>>(); }

TEST(vector2, string_vector_get_large_size) { string_vector_get_used_bytes<LargeStringContainer<std::string_view>>(); }

TEST(vector2, string_vector_zero_capacity_init)
{
    string_vector_zero_capacity_init<LargeStringContainer<std::string_view>>();
}
} // namespace omniruntime::vec::test
