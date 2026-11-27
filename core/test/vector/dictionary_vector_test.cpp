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

#include <gtest/gtest.h>
#include "type/decimal128.h"
#include "vector/vector_common.h"

using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace DictionaryVectorTest {
TEST(DictionaryVector, appendVector)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<int64_t>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        originVec->SetValue(j, j * j);
    }

    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(dicVecPtr);
    Vector<int64_t> appendedVec{valueSize + dicSize};
    appendedVec.Append(dicVec, 0, valueSize);
    appendedVec.Append(originVec.get(), valueSize, dicSize);

    for (int32_t index = 0; index < appendedVec.GetSize(); ++index) {
        auto value = appendedVec.GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dicVec;
}

TEST(DictionaryVector, copyPositions_long)
{
    int32_t dicSize = 10;
    int32_t valueSize = 7;
    auto originVec = std::make_unique<Vector<int64_t>>(dicSize);
    for (int32_t i = 0; i < dicSize; ++i) {
        if (i % 2 == 0) {
            originVec->SetNull(i);
            continue;
        }
        originVec->SetValue(i, i * i);
    }

    int32_t values[] = {2, 3, 4, 5, 6, 8, 9};
    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(dicVecPtr);

    int32_t positions[] = {1, 3, 5, 6};
    int32_t offset = 1;
    int32_t newValueSize = 3;
    auto copyPositions = dicVec->CopyPositions(positions, offset, newValueSize);

    for (int32_t i = 0; i < newValueSize; ++i) {
        if (values[positions[i + offset]] % 2 == 0) {
            EXPECT_EQ(originVec->IsNull(values[positions[i + offset]]), copyPositions->IsNull(i));
            continue;
        }
        auto value = copyPositions->GetValue(i);
        EXPECT_EQ(originVec->GetValue(values[positions[i + offset]]), value);
    }
    delete copyPositions;
    delete dicVec;
}

TEST(DictionaryVector, copyPositions_string_view)
{
    int32_t dicSize = 10;
    int32_t valueSize = 7;
    auto originVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(dicSize);
    for (int32_t i = 0; i < dicSize; ++i) {
        if (i % 2 == 0) {
            originVec->SetNull(i);
            continue;
        }
        auto str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.length());
        originVec->SetValue(i, value);
    }

    int32_t values[] = {2, 3, 4, 5, 6, 8, 9};
    auto dicVecPtr = VectorHelper::CreateStringDictionary(
        values, valueSize, reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(originVec.get()));
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(dicVecPtr);

    int32_t positions[] = {1, 3, 5, 6};
    int32_t offset = 1;
    int32_t newValueSize = 3;
    auto copyPositions = dicVec->CopyPositions(positions, offset, newValueSize);

    std::string_view value;
    for (int32_t i = 0; i < newValueSize; ++i) {
        if (values[positions[i + offset]] % 2 == 0) {
            EXPECT_EQ(originVec->IsNull(values[positions[i + offset]]), copyPositions->IsNull(i));
            continue;
        }
        value = copyPositions->GetValue(i);
        EXPECT_EQ(originVec->GetValue(values[positions[i + offset]]), value);
    }
    delete copyPositions;
    delete dicVec;
}

TEST(DictionaryVector, ShortType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<int16_t>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        originVec->SetValue(j, (int16_t)j);
    }

    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<int16_t>> *>(dicVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dicVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dicVec;
}

TEST(DictionaryVector, IntType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<int32_t>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        originVec->SetValue(j, j + 2);
    }

    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(dicVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dicVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dicVec;
}

TEST(DictionaryVector, LongType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<int64_t>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        originVec->SetValue(j, j * j);
    }

    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(dicVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dicVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dicVec;
}

TEST(DictionaryVector, BooleanType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<bool>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        if (j % 2 == 0) {
            originVec->SetValue(j, true);
            continue;
        }
        originVec->SetValue(j, false);
    }

    auto dictVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<bool>> *>(dictVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dictVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dictVec;
}

TEST(DictionaryVector, DoubleType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<double>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        originVec->SetValue(j, (double)sqrt(j));
    }

    auto dictVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<double>> *>(dictVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dictVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dictVec;
}

TEST(DictionaryVector, VarcharType)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        auto str = "string " + std::to_string(j);
        std::string_view val(str.data(), str.length());
        originVec->SetValue(j, val);
    }

    auto dictVecPtr = VectorHelper::CreateStringDictionary(
        values, valueSize, reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(originVec.get()));
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(dictVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dictVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dictVec;
}

TEST(DictionaryVector, Decimal128)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<Decimal128>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        Decimal128 val = Decimal128(j * j);
        originVec->SetValue(j, val);
    }

    auto dictVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(dictVecPtr);

    for (int32_t index = 0; index < valueSize; ++index) {
        auto value = dictVec->GetValue(index);
        EXPECT_EQ(value, originVec->GetValue(index % dicSize));
    }
    delete[] values;
    delete dictVec;
}

TEST(DictionaryVector, testNullFlag)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<Decimal128>>(dicSize);
    for (int32_t j = 0; j < dicSize; ++j) {
        if (j % 2 == 0) {
            originVec->SetNull(j);
        } else {
            Decimal128 val = Decimal128(j * j);
            originVec->SetValue(j, val);
        }
    }

    auto dicVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dicVec = reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(dicVecPtr);
    EXPECT_TRUE(dicVec->HasNull());
    auto sliceVec = dicVec->Slice(0, valueSize);

    for (int32_t index = 0; index < valueSize; ++index) {
        if (index % 2 == 0) {
            EXPECT_TRUE(sliceVec->IsNull(index));
            continue;
        } else {
            auto value = sliceVec->GetValue(index);
            EXPECT_EQ(value, originVec->GetValue(index % dicSize));
        }
    }
    delete[] values;
    delete sliceVec;
    delete dicVec;
}

TEST(DictionaryVector, getValue_with_null_Decimal128)
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<Decimal128>>(dicSize);
    for (int32_t i = 0; i < dicSize; ++i) {
        if (i % 2 == 0) {
            originVec->SetNull(i);
            continue;
        }
        Decimal128 value = (Decimal128)(i * 2 / 3);
        originVec->SetValue(i, value);
    }

    auto dictVecPtr = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(dictVecPtr);

    Decimal128 value;
    for (int32_t i = 0; i < valueSize; ++i) {
        if (values[i] % 2 == 0) {
            EXPECT_EQ(originVec.get()->IsNull(values[i]), dictVec->IsNull(i));
            continue;
        }
        value = dictVec->GetValue(i);
        EXPECT_EQ(originVec->GetValue(i % dicSize), value);
    }
    delete[] values;
    delete dictVec;
}

template <typename T, template <typename> typename CONTAINER> void getValue_with_null_container()
{
    int32_t dicSize = 10;
    int32_t valueSize = 100;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<CONTAINER<T>>>(dicSize);

    std::string valuePrefix;
    valuePrefix = "hello_world__";

    for (int32_t i = 0; i < dicSize; ++i) {
        if (i % 2 == 0) {
            originVec->SetNull(i);
            continue;
        }
        std::string value = valuePrefix + std::to_string(i);
        std::string_view input(value.data(), value.length());
        originVec->SetValue(i, input);
    }

    auto dictVecPtr = VectorHelper::CreateStringDictionary(values, valueSize, originVec.get());
    auto *dictVec = reinterpret_cast<Vector<DictionaryContainer<T, CONTAINER>> *>(dictVecPtr);

    for (int32_t i = 0; i < valueSize; ++i) {
        if (values[i] % 2 == 0) {
            EXPECT_EQ(originVec->IsNull(values[i]), dictVec->IsNull(i));
            continue;
        }
        std::string_view output = dictVec->GetValue(i);
        EXPECT_EQ(originVec->GetValue(i % dicSize), output);
    }
    delete[] values;
    delete dictVec;
}

TEST(DictionaryVector, getValue_with_null_LargeString)
{
    getValue_with_null_container<std::string_view, LargeStringContainer>();
}

TEST(DictionaryVector, appendDictionaryStringVector)
{
    int32_t vecSize = 10;
    int32_t valueSize = 6;
    auto originVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(vecSize);

    for (int32_t i = 0; i < valueSize; ++i) {
        auto str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.length());
        originVec->SetValue(i, value);
    }

    int32_t otherValueSize = 4;
    int32_t otherValues[] = {0, 1, 2, 3};
    auto otherOriginVec = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(otherValueSize);
    for (int32_t i = 0; i < otherValueSize; ++i) {
        auto str = "string " + std::to_string(i);
        std::string_view value(str.data(), str.length());
        otherOriginVec->SetValue(i, value);
    }
    auto otherDicVecPtr = VectorHelper::CreateStringDictionary(
        otherValues, otherValueSize,
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(otherOriginVec.get()));
    auto *otherDicVec =
        reinterpret_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(otherDicVecPtr);

    originVec->Append(otherDicVec, 6, 4);
    for (int i = valueSize; i < valueSize + otherValueSize; i++) {
        EXPECT_EQ(originVec->GetValue(i), otherOriginVec->GetValue(i - valueSize));
    }
    delete otherDicVec;
}
} // namespace DictionaryVectorTest
