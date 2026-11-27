/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Cardinality function test for Array and Map types
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/CardinalityFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;

class CardinalityTestHelper {
public:
    static ArrayVector* CreateArrayVector(
        const std::vector<std::vector<int32_t>>& arraysData,
        const std::vector<bool>& nullMasks = {})
    {
        std::vector<int64_t> offsets;
        offsets.push_back(0);
        size_t totalElements = 0;
        std::vector<int32_t> allElements;

        for (const auto& arrayData : arraysData) {
            allElements.insert(allElements.end(), arrayData.begin(), arrayData.end());
            totalElements += arrayData.size();
            offsets.push_back(totalElements);
        }

        auto elementVector = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, allElements.size());
        auto* elementFlatVec = reinterpret_cast<Vector<int32_t>*>(elementVector);
        for (size_t i = 0; i < allElements.size(); ++i) {
            elementFlatVec->SetValue(i, allElements[i]);
        }

        int32_t rowCount = static_cast<int32_t>(arraysData.size());
        auto arrayVector = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elementVector));

        for (int32_t i = 0; i <= rowCount; ++i) {
            arrayVector->SetOffset(i, offsets[i]);
        }

        for (int32_t i = 0; i < rowCount; ++i) {
            bool isNull = (i < static_cast<int32_t>(nullMasks.size())) ? nullMasks[i] : false;
            if (isNull) {
                arrayVector->SetNull(i);
            } else {
                arrayVector->SetNotNull(i);
            }
        }

        return arrayVector;
    }

    static MapVector* CreateMapVector(
        const std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>>& mapsData,
        const std::vector<bool>& nullMasks = {})
    {
        std::vector<int64_t> offsets;
        offsets.push_back(0);
        size_t totalKeys = 0;
        std::vector<int32_t> allKeys;
        std::vector<std::string> allValues;

        for (const auto& mapData : mapsData) {
            const auto& keys = mapData.first;
            const auto& values = mapData.second;

            allKeys.insert(allKeys.end(), keys.begin(), keys.end());
            allValues.insert(allValues.end(), values.begin(), values.end());

            totalKeys += keys.size();
            offsets.push_back(totalKeys);
        }

        auto keyVector = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, allKeys.size());
        auto* keyFlatVec = reinterpret_cast<Vector<int32_t>*>(keyVector);
        for (size_t i = 0; i < allKeys.size(); ++i) {
            keyFlatVec->SetValue(i, allKeys[i]);
        }

        std::vector<std::string_view> valueViews;
        for (const auto& val : allValues) {
            valueViews.emplace_back(val);
        }
        auto valueVector = VectorHelper::CreateStringVector(valueViews.size());
        auto* valueStrVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>>*>(valueVector);
        for (size_t i = 0; i < valueViews.size(); ++i) {
            valueStrVec->SetValue(i, valueViews[i]);
        }

        int32_t rowCount = static_cast<int32_t>(mapsData.size());
        auto mapVector = new MapVector(rowCount);
        mapVector->SetKeyVector(std::shared_ptr<BaseVector>(keyVector));
        mapVector->SetValueVector(std::shared_ptr<BaseVector>(valueVector));

        for (int32_t i = 0; i <= rowCount; ++i) {
            mapVector->SetOffset(i, offsets[i]);
        }

        for (int32_t i = 0; i < rowCount; ++i) {
            bool isNull = (i < static_cast<int32_t>(nullMasks.size())) ? nullMasks[i] : false;
            if (isNull) {
                mapVector->SetNull(i);
            } else {
                mapVector->SetNotNull(i);
            }
        }

        return mapVector;
    }

    static std::vector<int64_t> CalculateCardinality(BaseVector* inputVec, int32_t rowCount)
    {
        CardinalityFunction cardinalityFunc;
        std::stack<BaseVector*> args;
        args.push(inputVec);

        BaseVector* resultVec = nullptr;
        auto outputType = std::make_shared<DataType>(OMNI_LONG);

        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowCount);
        context.hasFilter = false;

        cardinalityFunc.Apply(args, outputType, resultVec, &context);

        std::vector<int64_t> results;
        if (resultVec != nullptr) {
            auto* longResultVec = reinterpret_cast<Vector<int64_t>*>(resultVec);
            for (int32_t i = 0; i < rowCount; ++i) {
                if (longResultVec->IsNull(i)) {
                    results.push_back(-999);
                } else {
                    results.push_back(longResultVec->GetValue(i));
                }
            }
            delete resultVec;
        }
        return results;
    }
};

TEST(CardinalityTest, ArrayBasic) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1, 2, 3},
        {4, 5},
        {6},
        {}
    };

    auto* arrayVector = CardinalityTestHelper::CreateArrayVector(arraysData);
    auto results = CardinalityTestHelper::CalculateCardinality(arrayVector, 4);

    std::vector<int64_t> expected = {3, 2, 1, 0};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, ArrayWithNulls) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1, 2},
        {},
        {3, 4, 5}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* arrayVector = CardinalityTestHelper::CreateArrayVector(arraysData, nullMasks);
    auto results = CardinalityTestHelper::CalculateCardinality(arrayVector, 3);

    std::vector<int64_t> expected = {2, -999, 3};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, MapBasic) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{1, 2, 3}, {"one", "two", "three"}},
        {{4, 5}, {"four", "five"}},
        {{6}, {"six"}},
        {{}, {}}
    };

    auto* mapVector = CardinalityTestHelper::CreateMapVector(mapsData);
    auto results = CardinalityTestHelper::CalculateCardinality(mapVector, 4);

    std::vector<int64_t> expected = {3, 2, 1, 0};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, MapWithNulls) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{1}, {"a"}},
        {{}, {}},
        {{2, 3}, {"b", "c"}}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* mapVector = CardinalityTestHelper::CreateMapVector(mapsData, nullMasks);
    auto results = CardinalityTestHelper::CalculateCardinality(mapVector, 3);

    std::vector<int64_t> expected = {1, -999, 2};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, ArrayEmpty) {
    std::vector<std::vector<int32_t>> arraysData = {{}, {}, {}};

    auto* arrayVector = CardinalityTestHelper::CreateArrayVector(arraysData);
    auto results = CardinalityTestHelper::CalculateCardinality(arrayVector, 3);

    std::vector<int64_t> expected = {0, 0, 0};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, MapEmpty) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{}, {}},
        {{}, {}},
        {{}, {}}
    };

    auto* mapVector = CardinalityTestHelper::CreateMapVector(mapsData);
    auto results = CardinalityTestHelper::CalculateCardinality(mapVector, 3);

    std::vector<int64_t> expected = {0, 0, 0};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }
}

TEST(CardinalityTest, SingleElementArray) {
    std::vector<std::vector<int32_t>> arraysData = {{42}};

    auto* arrayVector = CardinalityTestHelper::CreateArrayVector(arraysData);
    auto results = CardinalityTestHelper::CalculateCardinality(arrayVector, 1);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0], 1);

}

TEST(CardinalityTest, SingleElementMap) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{42}, {"answer"}}
    };

    auto* mapVector = CardinalityTestHelper::CreateMapVector(mapsData);
    auto results = CardinalityTestHelper::CalculateCardinality(mapVector, 1);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0], 1);

}

TEST(CardinalityTest, AllNullArrays) {
    std::vector<std::vector<int32_t>> arraysData = {{1}, {2}, {3}};
    std::vector<bool> nullMasks = {true, true, true};

    auto* arrayVector = CardinalityTestHelper::CreateArrayVector(arraysData, nullMasks);
    auto results = CardinalityTestHelper::CalculateCardinality(arrayVector, 3);

    std::vector<int64_t> expected = {-999, -999, -999};

    ASSERT_EQ(results.size(), expected.size());

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i]);
    }

}
