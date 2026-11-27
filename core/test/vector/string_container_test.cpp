/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: string container test implementations
 */

#include "gtest/gtest.h"
#include <limits>
#include "vector/vector.h"
#include "vector/vector_batch.h"
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

    std::vector<bool> expectedNull{ false, false, false, false, false, true, false, true, false, true };
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
    DataTypeId expect = type::OMNI_VARCHAR;

    auto baseVector = VectorHelper::CreateStringVector(vectorSize);
    auto *vector = (Vector<CONTAINER> *)baseVector;

    EXPECT_EQ(vector->GetTypeId(), expect);
    EXPECT_EQ(baseVector->GetTypeId(), expect);
    delete vector;
}

template <typename CONTAINER> void string_vector_get_datatype_explicit_char()
{
    int vectorSize = 1000;
    auto *vector = new Vector<CONTAINER>(vectorSize, INITIAL_STRING_SIZE, type::OMNI_CHAR);
    EXPECT_EQ(vector->GetTypeId(), type::OMNI_CHAR);
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


TEST(vector2, vector_append_large_string)
{
    string_vector_append_value<LargeStringContainer<std::string_view>>();
}


TEST(vector2, vector_copy_positions_large_string)
{
    string_vector_copy_positions<LargeStringContainer<std::string_view>>();
}


TEST(vector2, slice_container_large_string)
{
    string_vector_slice<LargeStringContainer<std::string_view>>();
}


TEST(vector2, string_vector_get_datatype)
{
    string_vector_get_datatype<LargeStringContainer<std::string_view>>();
}

TEST(vector2, string_vector_get_datatype_explicit_char)
{
    string_vector_get_datatype_explicit_char<LargeStringContainer<std::string_view>>();
}

TEST(vector2, string_vector_get_large_size)
{
    string_vector_get_used_bytes<LargeStringContainer<std::string_view>>();
}

TEST(vector2, string_vector_zero_capacity_init)
{
    string_vector_zero_capacity_init<LargeStringContainer<std::string_view>>();
}

TEST(vector2, large_string_container_set_values_contiguous)
{
    LargeStringContainer<std::string_view> container(6, 1);
    std::string alpha = "alpha";
    std::string beta = "beta";
    std::string gamma = "gamma";
    std::string omega = "omega";
    std::string_view firstBatch[] = { alpha, std::string_view(), beta };
    std::string_view secondBatch[] = { gamma, std::string_view(), omega };

    container.SetValues(0, firstBatch, nullptr, 3);
    container.SetValues(3, secondBatch, nullptr, 3);

    // SetValues owns the copied payload; changes to the input storage must not affect it.
    alpha.assign("changed");
    omega.clear();
    EXPECT_EQ(std::string_view("alpha"), container.GetValue(0));
    EXPECT_TRUE(container.GetValue(1).empty());
    EXPECT_EQ(std::string_view("beta"), container.GetValue(2));
    EXPECT_EQ(std::string_view("gamma"), container.GetValue(3));
    EXPECT_TRUE(container.GetValue(4).empty());
    EXPECT_EQ(std::string_view("omega"), container.GetValue(5));
}

TEST(vector2, string_vector_set_values_updates_null_bitmap)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    StringVector vector(6, 1);
    std::string_view values[] = {
        "first", "ignored-null", std::string_view(), "fourth", "ignored-too", "sixth"
    };
    uint8_t nulls[] = { 0, 1, 0, 0, 1, 0 };
    BaseVector *baseVector = &vector;
    for (int32_t i = 0; i < vector.GetSize(); ++i) {
        // Seed only the bitmap so SetValues must clear non-null rows in its target range.
        baseVector->SetNull(i);
    }

    vector.SetValues(0, values, nulls, 6);

    EXPECT_TRUE(vector.HasNull());
    EXPECT_EQ(2, vector.GetNullCount());
    EXPECT_FALSE(vector.IsNull(0));
    EXPECT_TRUE(vector.IsNull(1));
    EXPECT_FALSE(vector.IsNull(2));
    EXPECT_FALSE(vector.IsNull(3));
    EXPECT_TRUE(vector.IsNull(4));
    EXPECT_FALSE(vector.IsNull(5));
    EXPECT_EQ(std::string_view("first"), vector.GetValue(0));
    EXPECT_TRUE(vector.GetValue(1).empty());
    EXPECT_TRUE(vector.GetValue(2).empty());
    EXPECT_EQ(std::string_view("fourth"), vector.GetValue(3));
    EXPECT_TRUE(vector.GetValue(4).empty());
    EXPECT_EQ(std::string_view("sixth"), vector.GetValue(5));
}

TEST(vector2, large_string_container_set_values_fast_gap_append)
{
    LargeStringContainer<std::string_view> container(7, 1);
    std::string prefix(1024, 'p');
    container.SetValue(0, prefix);
    std::string_view values[] = { "after-gap", "ignored-null", std::string_view(), "tail" };
    uint8_t nulls[] = { 0, 1, 0, 0 };
    const uint64_t totalBytes = values[0].size() + values[2].size() + values[3].size();

    container.SetValues(3, values, nulls, 4, totalBytes);

    EXPECT_GE(container.GetCapacityInBytes(), prefix.size() + totalBytes);
    EXPECT_EQ(std::string_view(prefix), container.GetValue(0));
    EXPECT_TRUE(container.GetValue(1).empty());
    EXPECT_TRUE(container.GetValue(2).empty());
    EXPECT_EQ(std::string_view("after-gap"), container.GetValue(3));
    EXPECT_TRUE(container.GetValue(4).empty());
    EXPECT_TRUE(container.GetValue(5).empty());
    EXPECT_EQ(std::string_view("tail"), container.GetValue(6));
}

TEST(vector2, string_vector_set_values_fast_gap_updates_nulls_and_owns_payload)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    StringVector vector(8, 1);
    vector.SetValue(0, "prefix");

    // Seed the target bitmap so the fast overload must clear non-null rows before
    // applying the supplied null map. Row 1 is an unmaterialized gap.
    auto *baseVector = static_cast<BaseVector *>(&vector);
    for (int32_t row = 2; row <= 6; ++row) {
        baseVector->SetNull(row);
    }
    std::string copiedPayload = "copied-payload";
    std::string_view values[] = {
        "alpha", "ignored-null", std::string_view(), copiedPayload, std::string_view()
    };
    uint8_t nulls[] = {0, 1, 0, 0, 0};
    const uint64_t totalBytes = values[0].size() + values[2].size() + values[3].size() + values[4].size();

    vector.SetValues(2, values, nulls, 5, totalBytes);
    copiedPayload.assign("changed-after-copy");
    vector.SetValue(7, "suffix");

    EXPECT_EQ(vector.GetNullCount(), 1);
    EXPECT_EQ(vector.GetValue(0), std::string_view("prefix"));
    EXPECT_FALSE(vector.IsNull(1));
    EXPECT_TRUE(vector.GetValue(1).empty());
    EXPECT_FALSE(vector.IsNull(2));
    EXPECT_EQ(vector.GetValue(2), std::string_view("alpha"));
    EXPECT_TRUE(vector.IsNull(3));
    EXPECT_TRUE(vector.GetValue(3).empty());
    EXPECT_FALSE(vector.IsNull(4));
    EXPECT_TRUE(vector.GetValue(4).empty());
    EXPECT_FALSE(vector.IsNull(5));
    EXPECT_EQ(vector.GetValue(5), std::string_view("copied-payload"));
    EXPECT_FALSE(vector.IsNull(6));
    EXPECT_TRUE(vector.GetValue(6).empty());
    EXPECT_FALSE(vector.IsNull(7));
    EXPECT_EQ(vector.GetValue(7), std::string_view("suffix"));
}

TEST(vector2, large_string_container_set_values_validating_gap_append)
{
    LargeStringContainer<std::string_view> container(5, 1);
    container.SetValue(0, "head");
    std::string_view values[] = { "middle", std::string_view() };

    container.SetValues(3, values, nullptr, 2);

    EXPECT_EQ(std::string_view("head"), container.GetValue(0));
    EXPECT_TRUE(container.GetValue(1).empty());
    EXPECT_TRUE(container.GetValue(2).empty());
    EXPECT_EQ(std::string_view("middle"), container.GetValue(3));
    EXPECT_TRUE(container.GetValue(4).empty());
}

TEST(vector2, large_string_container_set_values_rejects_overwrite_and_alias)
{
    LargeStringContainer<std::string_view> container(4, 1);
    std::string_view initial[] = { "left", "right" };
    container.SetValues(0, initial, nullptr, 2);

    std::string_view replacement = "replacement";
    EXPECT_ANY_THROW(container.SetValues(1, &replacement, nullptr, 1));

    const std::string_view aliasedValue = container.GetValue(0);
    EXPECT_ANY_THROW(container.SetValues(2, &aliasedValue, nullptr, 1));

    // Rejected batches must leave the append position and existing payload intact.
    container.SetValues(2, &replacement, nullptr, 1);
    EXPECT_EQ(std::string_view("left"), container.GetValue(0));
    EXPECT_EQ(std::string_view("right"), container.GetValue(1));
    EXPECT_EQ(replacement, container.GetValue(2));
}

TEST(vector2, large_string_container_set_values_validates_ranges)
{
    LargeStringContainer<std::string_view> container(3, 1);
    std::string_view value = "value";

    EXPECT_ANY_THROW(container.SetValues(-1, &value, nullptr, 1));
    EXPECT_ANY_THROW(container.SetValues(0, &value, nullptr, -1));
    EXPECT_ANY_THROW(container.SetValues(3, &value, nullptr, 1));
    EXPECT_ANY_THROW(container.SetValues(2, &value, nullptr, 2));
    EXPECT_ANY_THROW(container.SetValues(0, nullptr, nullptr, 1));
    EXPECT_NO_THROW(container.SetValues(0, nullptr, nullptr, 0));
    EXPECT_NO_THROW(container.SetValues(3, nullptr, nullptr, 0));
}

TEST(vector2, large_string_container_set_values_fast_rejects_offset_overflow)
{
    LargeStringContainer<std::string_view> container(2, 1);
    container.SetValue(0, "x");
    std::string_view value = "y";

    EXPECT_ANY_THROW(container.SetValues(
        1, &value, nullptr, 1, static_cast<uint64_t>(std::numeric_limits<int32_t>::max())));

    // The failed reservation must not consume the row.
    container.SetValues(1, &value, nullptr, 1, value.size());
    EXPECT_EQ(std::string_view("x"), container.GetValue(0));
    EXPECT_EQ(value, container.GetValue(1));
}

TEST(vector2, string_vector_set_values_rejects_invalid_range_and_slice)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    StringVector vector(3, 1);
    std::string_view value = "value";

    EXPECT_ANY_THROW(vector.SetValues(-1, &value, nullptr, 1));
    EXPECT_ANY_THROW(vector.SetValues(0, &value, nullptr, -1));
    EXPECT_ANY_THROW(vector.SetValues(3, &value, nullptr, 1));
    EXPECT_ANY_THROW(vector.SetValues(2, &value, nullptr, 2));
    EXPECT_ANY_THROW(vector.SetValues(0, nullptr, nullptr, 1));
    EXPECT_NO_THROW(vector.SetValues(0, nullptr, nullptr, 0));
    EXPECT_NO_THROW(vector.SetValues(3, nullptr, nullptr, 0));

    std::unique_ptr<BaseVector> sliced(vector.Slice(0, 2));
    auto *stringSlice = static_cast<StringVector *>(sliced.get());
    EXPECT_ANY_THROW(stringSlice->SetValues(0, &value, nullptr, 1));
    EXPECT_TRUE(vector.GetValue(0).empty());

    sliced.reset();
    std::string_view parentValues[] = { "zero", "one", "two" };
    vector.SetValues(0, parentValues, nullptr, 3);
    EXPECT_EQ(std::string_view("zero"), vector.GetValue(0));
    EXPECT_EQ(std::string_view("one"), vector.GetValue(1));
}

TEST(vector2, string_vector_reset_for_reuse_clears_offsets_and_nulls)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    StringVector vector(5, 1);
    std::string largeValue(2048, 'a');
    std::string_view firstValues[] = { largeValue, "ignored-null", std::string_view(), "also-null", "old-tail" };
    uint8_t nulls[] = { 0, 1, 0, 1, 0 };
    vector.SetValues(0, firstValues, nulls, 5);
    ASSERT_TRUE(vector.HasNull());

    vector.ResetForReuse();

    EXPECT_FALSE(vector.HasNull());
    EXPECT_EQ(0, vector.GetNullCount());
    for (int32_t i = 0; i < vector.GetSize(); ++i) {
        EXPECT_FALSE(vector.IsNull(i));
        EXPECT_TRUE(vector.GetValue(i).empty());
    }

    std::string_view reusedValues[] = { "new", std::string_view(), "payload", "is", "short" };
    vector.SetValues(0, reusedValues, nullptr, 5);
    EXPECT_EQ(std::string_view("new"), vector.GetValue(0));
    EXPECT_TRUE(vector.GetValue(1).empty());
    EXPECT_EQ(std::string_view("payload"), vector.GetValue(2));
    EXPECT_EQ(std::string_view("is"), vector.GetValue(3));
    EXPECT_EQ(std::string_view("short"), vector.GetValue(4));
    EXPECT_FALSE(vector.HasNull());
}

TEST(vector2, string_vector_reset_for_reuse_clears_nulls_to_capacity)
{
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    auto *vector = new StringVector(5, 1);
    VectorBatch batch(5);
    batch.Append(vector);
    std::string_view values[] = { "zero", "one", "two", "three", "ignored-null" };
    uint8_t nulls[] = { 0, 0, 0, 0, 1 };
    vector->SetValues(0, values, nulls, 5);
    ASSERT_TRUE(vector->IsNull(4));

    // Resize only clears the new logical range; row 4 remains set in the retained bitmap.
    batch.Resize(2);
    ASSERT_EQ(2, vector->GetSize());
    vector->ResetForReuse();
    batch.Expand(5);

    ASSERT_EQ(5, vector->GetSize());
    EXPECT_FALSE(vector->IsNull(4));
    EXPECT_FALSE(vector->HasNull());
}
}
