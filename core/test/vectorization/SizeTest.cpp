/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Size function test for Array and Map types
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vectorization/functions/SizeFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

class SizeTestHelper {
public:
    /// Create an ArrayVector for testing
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

        int32_t rowCount = arraysData.size();
        auto arrayVector = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elementVector));

        // Set offset array
        for (int32_t i = 0; i <= rowCount; ++i) {
            arrayVector->SetOffset(i, offsets[i]);
        }

        // Set NULL masks
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

    /// Create a MapVector for testing
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

        int32_t rowCount = mapsData.size();
        auto mapVector = new MapVector(rowCount);
        mapVector->SetKeyVector(std::shared_ptr<BaseVector>(keyVector));
        mapVector->SetValueVector(std::shared_ptr<BaseVector>(valueVector));

        // Set offset array
        for (int32_t i = 0; i <= rowCount; ++i) {
            mapVector->SetOffset(i, offsets[i]);
        }

        // Set NULL masks
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

    /// Calculate sizes using SizeFunction
    static std::vector<int32_t> CalculateSizes(BaseVector* inputVec, bool legacySizeOfNull, int32_t rowCount)
    {
        SizeFunction sizeFunc;
        std::stack<BaseVector*> args;

        // Create boolean vector for legacySizeOfNull parameter
        auto boolVector = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowCount);
        auto* boolFlatVec = reinterpret_cast<Vector<bool>*>(boolVector);
        for (int32_t i = 0; i < rowCount; ++i) {
            boolFlatVec->SetValue(i, legacySizeOfNull);
        }

        args.push(inputVec);
        args.push(boolVector);

        BaseVector* resultVec = nullptr;
        auto outputType = std::make_shared<DataType>(OMNI_INT);

        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowCount);
        context.hasFilter = false;

        try {
            sizeFunc.Apply(args, outputType, resultVec, &context);
        } catch (const std::exception& e) {
            std::cerr << "Exception in SizeFunction: " << e.what() << std::endl;
            throw;
        }

        // Collect Results
        std::vector<int32_t> results;
        if (resultVec != nullptr) {
            auto* intResultVec = reinterpret_cast<Vector<int32_t>*>(resultVec);
            for (int32_t i = 0; i < rowCount; ++i) {
                if (intResultVec->IsNull(i)) {
                    // For NULL results in non-legacy mode, we use a sentinel value
                    results.push_back(-999);  // Sentinel for NULL
                } else {
                    results.push_back(intResultVec->GetValue(i));
                }
            }
            delete resultVec;
        }

        return results;
    }
};

// Test basic array sizes
TEST(SizeTest, ArrayBasicSize) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1, 2, 3},
        {4, 5},
        {6},
        {}
    };

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, false, 4);

    std::vector<int32_t> expected = {3, 2, 1, 0};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Array " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test array with NULL values in legacy mode
TEST(SizeTest, ArrayWithNullsLegacyMode) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1, 2},
        {},
        {3, 4, 5}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData, nullMasks);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, true, 3);

    std::vector<int32_t> expected = {2, -1, 3};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Array " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test array with NULL values in non-legacy mode
TEST(SizeTest, ArrayWithNullsNonLegacyMode) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1, 2},
        {},
        {3, 4, 5}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData, nullMasks);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, false, 3);

    // In non-legacy mode, NULL input should return NULL (sentinel value -999)
    std::vector<int32_t> expected = {2, -999, 3};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Array " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test basic map sizes
TEST(SizeTest, MapBasicSize) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{1, 2, 3}, {"one", "two", "three"}},
        {{4, 5}, {"four", "five"}},
        {{6}, {"six"}},
        {{}, {}}
    };

    auto* mapVector = SizeTestHelper::CreateMapVector(mapsData);
    auto results = SizeTestHelper::CalculateSizes(mapVector, false, 4);

    std::vector<int32_t> expected = {3, 2, 1, 0};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Map " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test map with NULL values in legacy mode
TEST(SizeTest, MapWithNullsLegacyMode) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{1}, {"a"}},
        {{}, {}},
        {{2, 3}, {"b", "c"}}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* mapVector = SizeTestHelper::CreateMapVector(mapsData, nullMasks);
    auto results = SizeTestHelper::CalculateSizes(mapVector, true, 3);

    std::vector<int32_t> expected = {1, -1, 2};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Map " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test map with NULL values in non-legacy mode
TEST(SizeTest, MapWithNullsNonLegacyMode) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{1}, {"a"}},
        {{}, {}},
        {{2, 3}, {"b", "c"}}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* mapVector = SizeTestHelper::CreateMapVector(mapsData, nullMasks);
    auto results = SizeTestHelper::CalculateSizes(mapVector, false, 3);

    // In non-legacy mode, NULL input should return NULL (sentinel value -999)
    std::vector<int32_t> expected = {1, -999, 2};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Map " << i << " expected size " << expected[i]
            << ", but got " << results[i];
    }
}

// Test empty arrays
TEST(SizeTest, ArrayEmptyArrays) {
    std::vector<std::vector<int32_t>> arraysData = {
        {},
        {},
        {}
    };

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, false, 3);

    std::vector<int32_t> expected = {0, 0, 0};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Empty array " << i << " expected size 0, but got " << results[i];
    }
}

// Test empty maps
TEST(SizeTest, MapEmptyMaps) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{}, {}},
        {{}, {}},
        {{}, {}}
    };

    auto* mapVector = SizeTestHelper::CreateMapVector(mapsData);
    auto results = SizeTestHelper::CalculateSizes(mapVector, false, 3);

    std::vector<int32_t> expected = {0, 0, 0};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "Empty map " << i << " expected size 0, but got " << results[i];
    }
}

// Test single element array
TEST(SizeTest, SingleElementArray) {
    std::vector<std::vector<int32_t>> arraysData = {
        {42}
    };

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, false, 1);

    ASSERT_EQ(results.size(), 1) << "Result size mismatch";
    ASSERT_EQ(results[0], 1) << "Single element array expected size 1, but got " << results[0];
}

// Test single element map
TEST(SizeTest, SingleElementMap) {
    std::vector<std::pair<std::vector<int32_t>, std::vector<std::string>>> mapsData = {
        {{42}, {"answer"}}
    };

    auto* mapVector = SizeTestHelper::CreateMapVector(mapsData);
    auto results = SizeTestHelper::CalculateSizes(mapVector, false, 1);

    ASSERT_EQ(results.size(), 1) << "Result size mismatch";
    ASSERT_EQ(results[0], 1) << "Single element map expected size 1, but got " << results[0];
}

// Test all NULL arrays in legacy mode
TEST(SizeTest, AllNullArrays) {
    std::vector<std::vector<int32_t>> arraysData = {
        {1},
        {2},
        {3}
    };

    std::vector<bool> nullMasks = {true, true, true};

    auto* arrayVector = SizeTestHelper::CreateArrayVector(arraysData, nullMasks);
    auto results = SizeTestHelper::CalculateSizes(arrayVector, true, 3);

    std::vector<int32_t> expected = {-1, -1, -1};

    ASSERT_EQ(results.size(), expected.size()) << "Result size mismatch";

    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i], expected[i])
            << "NULL array " << i << " expected size -1, but got " << results[i];
    }
}
