/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Single Map Size Calculation Unit Test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vector/map_vector.h"
#include "vector/vector.h"
#include "vectorization/functions/MapSize.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::TestUtil;

class MultiMapSizeTestHelper {
public:
    static MapVector* CreateMultiMapVector(
            const std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>>& mapsData,
            const std::vector<bool>& nullMasks = {}) {

        std::vector<int64_t> offsets;
        offsets.push_back(0);  // First element's offset is 0
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

        // Build multiple rows map
        int32_t rowCount = mapsData.size();
        auto mapVector = new MapVector(rowCount);
        mapVector->SetKeyVector(std::shared_ptr<BaseVector>(keyVector));
        mapVector->SetValueVector(std::shared_ptr<BaseVector>(valueVector));

        // Set offset array
        for (int32_t i = 0; i <= rowCount; ++i) {
            mapVector->SetOffset(i, offsets[i]);
        }

        // Set NULL
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

    static std::vector<int32_t> CalculateMapSizes(MapVector* mapVector) {
        MapSizeFunction mapSizeFunc;
        std::stack<BaseVector*> args;

        int32_t rowCount = mapVector->vec::BaseVector::GetSize();

        auto boolVector = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowCount);
        auto* boolFlatVec = reinterpret_cast<Vector<bool>*>(boolVector);
        for (int32_t i = 0; i < rowCount; ++i) {
            boolFlatVec->SetValue(i, true);
        }
        args.push(mapVector);
        args.push(boolVector);

        BaseVector* resultVec = nullptr;
        auto outputType = std::make_shared<DataType>(OMNI_INT);

        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowCount);
        context.hasFilter = false;

        try {
            mapSizeFunc.Apply(args, outputType, resultVec, &context);
        } catch (const std::exception& e) {
            std::cerr << "Exception in MapSizeFunction: " << e.what() << std::endl;
            delete boolVector;
            throw;
        }

        // Collect Results
        std::vector<int32_t> results;
        if (resultVec != nullptr) {
            auto* intResultVec = reinterpret_cast<Vector<int32_t>*>(resultVec);
            for (int32_t i = 0; i < rowCount; ++i) {
                results.push_back(intResultVec->GetValue(i));
            }
            delete resultVec;
        }

        delete boolVector;

        return results;
    }
};

TEST(VectorizationTest, MultiMapSizeCalculation) {
    std::cout << "=== Testing multiple maps ===" << std::endl;

    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
            {{1, 2, 3}, {"one", "two", "three"}},
            {{}, {}},
            {{4}, {"four"}},
            {{5, 6}, {"five", "six"}}
    };

    auto* mapVector = MultiMapSizeTestHelper::CreateMultiMapVector(mapsData);

    try {
        MapVectorReader reader(mapVector);
        std::cout << "MapVectorReader test passed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "MapVectorReader failed: " << e.what() << std::endl;
        delete mapVector;
        FAIL() << "MapVectorReader failed: " << e.what();
    }

    auto results = MultiMapSizeTestHelper::CalculateMapSizes(mapVector);

    std::vector<int32_t> expected = {3, 0, 1, 2};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Map " << i << " size: " << results[i]
                  << " (expected: " << expected[i] << ")" << std::endl;
        ASSERT_EQ(results[i], expected[i])
                                    << "Map " << i << " expected size " << expected[i]
                                    << ", but got " << results[i];
    }

    delete mapVector;
}

TEST(VectorizationTest, MapWithNulls) {
    std::cout << "=== Testing maps with NULL values ===" << std::endl;

    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
            {{1}, {"a"}},
            {{}, {}},
            {{2, 3}, {"b", "c"}}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* mapVector = MultiMapSizeTestHelper::CreateMultiMapVector(mapsData, nullMasks);
    auto results = MultiMapSizeTestHelper::CalculateMapSizes(mapVector);

    std::vector<int32_t> expected = {1, -1, 2};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Map " << i << " size: " << results[i]
                  << " (expected: " << expected[i] << ")" << std::endl;
        ASSERT_EQ(results[i], expected[i])
                                    << "Map " << i << " expected size " << expected[i]
                                    << ", but got " << results[i];
    }

    delete mapVector;
}
