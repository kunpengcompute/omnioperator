/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "gtest/gtest.h"
#include "vector/vector_batch.h"
#include "vector_test_util.h"
#include "vector/dictionary_container.h"

namespace omniruntime::vec::test {
namespace {
class DestructionCountingVector : public BaseVector {
public:
    DestructionCountingVector(int32_t rowCount, int32_t &destructionCount)
        : BaseVector(rowCount), destructionCount(destructionCount)
    {}

    ~DestructionCountingVector() override
    {
        ++destructionCount;
    }

private:
    int32_t &destructionCount;
};
} // namespace

TEST(vector2, vec_batch)
{
    int32_t rowCnt = 32;
    auto intVec = std::make_unique<Vector<int32_t>>(rowCnt);
    auto stringVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        intVec->SetValue(i, i * 2);
        std::string str("hello" + std::to_string(i));
        std::string_view value(str.data(), str.size());
        stringVec->SetValue(i, value);
    }

    int32_t dictSize = 4;
    int32_t *values = new int32_t[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        values[i] = i % dictSize;
    }
    using DICTIONARY_DATA_TYPE = typename TYPE_UTIL<int32_t>::DICTIONARY_TYPE;
    auto dictionary = CreateDictionary<DICTIONARY_DATA_TYPE>(dictSize);
    auto container = std::make_shared<DictionaryContainer<int32_t>>(values, rowCnt, dictionary, dictSize, 0);
    std::unique_ptr<NullsBuffer> nullsBuffer = std::make_unique<NullsBuffer>(rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        nullsBuffer->SetNull(i, false);
    }
    auto intDictVec = std::make_unique<Vector<DictionaryContainer<int32_t>>>(rowCnt, container, nullsBuffer.get());

    VectorBatch vectorBatch(rowCnt);
    vectorBatch.Append(intVec.release());
    vectorBatch.Append(stringVec.release());
    vectorBatch.Append(intDictVec.release());

    auto intCol0 = reinterpret_cast<Vector<int32_t> *>(vectorBatch.Get(0));
    auto stringCol1 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vectorBatch.Get(1));
    auto intDictCol2 = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vectorBatch.Get(2));
    for (int32_t i = 0; i < rowCnt; i++) {
        EXPECT_EQ(intCol0->GetValue(i), i * 2);
        EXPECT_EQ(stringCol1->GetValue(i), "hello" + std::to_string(i));
        EXPECT_EQ(intDictCol2->GetValue(i), (i % dictSize) * 2 / 3);
    }
    delete[] values;
    vectorBatch.FreeAllVectors();
}

TEST(vector2, vec_batch_resize)
{
    int rowCount = 100;
    int resizeCount = 50;
    auto *pVec1 = new Vector<NativeType<OMNI_LONG>::type>(rowCount, OMNI_LONG);
    auto *pVec2= new Vector<NativeType<OMNI_DOUBLE>::type>(rowCount, OMNI_DOUBLE);
    auto *pVec3= new Vector<NativeType<OMNI_DOUBLE>::type>(rowCount, OMNI_DOUBLE);

    auto *pVb = new VectorBatch(rowCount);
    pVb->Append(pVec1);
    pVb->Append(pVec2);
    pVb->Append(pVec3);

    ASSERT_EQ(pVb->GetRowCount(), rowCount);
    ASSERT_EQ(pVb->GetRowCount(), pVec1->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec2->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec3->GetSize());

    pVb->Resize(resizeCount);
    ASSERT_EQ(pVb->GetRowCount(), resizeCount);
    ASSERT_EQ(pVb->GetRowCount(), pVec1->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec2->GetSize());
    ASSERT_EQ(pVb->GetRowCount(), pVec3->GetSize());

    VectorHelper::PrintVecBatch(pVb);
    VectorHelper::FreeVecBatch(pVb);
}

// FreeAllVectors must delete a pointer shared by several columns exactly once.
TEST(vector2, vec_batch_free_all_with_duplicate_and_null_vectors)
{
    int32_t rowCnt = 8;
    auto *sharedVec = new Vector<int32_t>(rowCnt);
    auto *otherVec = new Vector<int64_t>(rowCnt);

    VectorBatch vectorBatch(rowCnt);
    vectorBatch.Append(sharedVec);
    vectorBatch.Append(nullptr);
    vectorBatch.Append(otherVec);
    vectorBatch.Append(sharedVec); // duplicate of index 0
    vectorBatch.Append(nullptr);   // duplicate nullptr
    vectorBatch.Append(otherVec);  // duplicate of index 2

    // Must not double-free under ASAN/valgrind.
    vectorBatch.FreeAllVectors();
    EXPECT_EQ(vectorBatch.GetVectorCount(), 0);
}

TEST(vector2, vec_batch_free_all_empty)
{
    VectorBatch vectorBatch(8);
    vectorBatch.FreeAllVectors();
    EXPECT_EQ(vectorBatch.GetVectorCount(), 0);
}

// Wide schema (> linearDedupLimit) exercises the sort+unique fallback path.
TEST(vector2, vec_batch_free_all_wide_with_duplicates)
{
    constexpr int32_t vecCnt = 64; // > 32, forces the wide-schema path
    int32_t rowCnt = 4;
    VectorBatch vectorBatch(rowCnt);
    auto *sharedVec = new Vector<int32_t>(rowCnt);
    for (int32_t i = 0; i < vecCnt; ++i) {
        if (i == 0 || i == vecCnt / 2 || i == vecCnt - 1) {
            vectorBatch.Append(sharedVec);
        } else {
            vectorBatch.Append(new Vector<int32_t>(rowCnt));
        }
    }

    vectorBatch.FreeAllVectors();
    EXPECT_EQ(vectorBatch.GetVectorCount(), 0);
}

TEST(vector2, vec_batch_free_all_linear_boundary_deletes_each_unique_vector_once)
{
    constexpr int32_t vectorCount = 32;
    int32_t destructionCount = 0;
    {
        VectorBatch vectorBatch(4);
        vectorBatch.ResizeVectorCount(vectorCount);
        auto *first = new DestructionCountingVector(4, destructionCount);
        auto *second = new DestructionCountingVector(4, destructionCount);
        auto *third = new DestructionCountingVector(4, destructionCount);

        for (int32_t column = 0; column < vectorCount; ++column) {
            switch (column % 5) {
                case 0:
                case 2:
                    vectorBatch.SetVector(column, first);
                    break;
                case 1:
                    vectorBatch.SetVector(column, second);
                    break;
                case 3:
                    vectorBatch.SetVector(column, nullptr);
                    break;
                default:
                    vectorBatch.SetVector(column, third);
                    break;
            }
        }

        vectorBatch.FreeAllVectors();
        EXPECT_EQ(destructionCount, 3);
        EXPECT_EQ(vectorBatch.GetVectorCount(), 0);

        // A second call and the batch destructor must remain no-ops after storage is cleared.
        vectorBatch.FreeAllVectors();
        EXPECT_EQ(destructionCount, 3);
    }
    EXPECT_EQ(destructionCount, 3);
}

TEST(vector2, vec_batch_destructor_wide_boundary_deletes_each_unique_vector_once)
{
    constexpr int32_t vectorCount = 33;
    int32_t destructionCount = 0;
    {
        VectorBatch vectorBatch(4);
        vectorBatch.ResizeVectorCount(vectorCount);
        auto *first = new DestructionCountingVector(4, destructionCount);
        auto *second = new DestructionCountingVector(4, destructionCount);
        auto *third = new DestructionCountingVector(4, destructionCount);
        auto *fourth = new DestructionCountingVector(4, destructionCount);
        BaseVector *values[] = {first, nullptr, second, first, third, fourth, nullptr, second};
        constexpr int32_t valueCount = static_cast<int32_t>(sizeof(values) / sizeof(values[0]));
        for (int32_t column = 0; column < vectorCount; ++column) {
            vectorBatch.SetVector(column, values[column % valueCount]);
        }
        EXPECT_EQ(destructionCount, 0);
    }

    // 33 columns select the sort+unique path. Repeated pointers and nullptr are not deleted twice.
    EXPECT_EQ(destructionCount, 4);
}

TEST(vector2, vec_batch_can_be_reused_after_free_all_vectors)
{
    int32_t destructionCount = 0;
    {
        VectorBatch vectorBatch(2);
        auto *first = new DestructionCountingVector(2, destructionCount);
        vectorBatch.Append(first);
        vectorBatch.Append(first);
        vectorBatch.FreeAllVectors();
        ASSERT_EQ(destructionCount, 1);

        auto *second = new DestructionCountingVector(2, destructionCount);
        vectorBatch.Append(nullptr);
        vectorBatch.Append(second);
        vectorBatch.Append(second);
        EXPECT_EQ(vectorBatch.GetVectorCount(), 3);
    }
    EXPECT_EQ(destructionCount, 2);
}
}
