///*
// * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
// */
//
//#include "gtest/gtest.h"
//#include "util/test_util.h"
//#include "vectorization/functions/Comparisons.h"
//
//namespace omniruntime {
//using namespace omniruntime::op;
//using namespace omniruntime::vec;
//using namespace std;
//using namespace TestUtil;
//using VarcharVector = vec::Vector<vec::LargeStringContainer<std::string_view>>;
//
//const int32_t ROW_SIZE = 1000000;
//const int32_t VAR_LEN = 10;
//const int32_t ROUNDS = 10;
//
//const std::string STRING = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
//static const int32_t SIZE_OF_LONG = 8;
//string GetString(int32_t index, int32_t offset, int32_t width)
//{
//    std::string str;
//    for (int j = 0; j < width; j++) {
//        char c = STRING.at((index + offset + j) % STRING.length());
//        str.push_back(c);
//    }
//    return str;
//}
//
//int32_t CompareVarchar(BaseVector *leftColumn, int32_t leftColumnPosition, BaseVector *rightColumn,
//    int32_t rightColumnPosition)
//{
//    auto leftVarCharColumn = static_cast<VarcharVector *>(leftColumn);
//    auto rightVarCharColumn = static_cast<VarcharVector *>(rightColumn);
//
//    auto leftValue = leftVarCharColumn->GetValue(leftColumnPosition);
//    auto rightValue = rightVarCharColumn->GetValue(rightColumnPosition);
//    int32_t result = leftValue.compare(rightValue);
//    if (result != 0) {
//        return (result > 0) ? 1 : -1;
//    } else if (leftValue.size() == rightValue.size()) {
//        return 0;
//    } else {
//        return (leftValue.size() > rightValue.size()) ? 1 : -1;
//    }
//}
//
//void PrintCompareVarcharResult(BaseVector *vector1, BaseVector *vector2, Timer &timer)
//{
//    double sum = 0.0f;
//    int comp = 0;
//    for (int j = 0; j < ROUNDS; j++) {
//        timer.Reset();
//
//        for (int i = 0; i < ROW_SIZE; i++) {
//            comp = CompareVarchar(vector1, i, vector2, i);
//        }
//
//        timer.CalculateElapse();
//        double wallElapsed = timer.GetWallElapse() * 1000;
//        double cpuElapsed = timer.GetCpuElapse() * 1000;
//        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
//        sum += wallElapsed;
//    }
//    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
//    std::cout << "comp: " << comp << std::endl;
//}
//
//int64_t ReverseBytes(int64_t var0)
//{
//    auto result = static_cast<uint64_t>(var0);
//    result = ((result & 71777214294589695L) << 8) | ((result >> 8) & 71777214294589695L);
//    return (result << 48) | ((result & 4294901760L) << 16) | ((result >> 16) & 4294901760L) | (result >> 48);
//}
//
//int64_t LongBytesToLong(int64_t bytes)
//{
//    return ReverseBytes(bytes) ^ LONG_MIN;
//}
//
//int32_t CmpByLong(char *var1, char *var2, int32_t compareLength)
//{
//    auto *left = reinterpret_cast<int64_t *>(var1);
//    auto *right = reinterpret_cast<int64_t *>(var2);
//    while (compareLength >= 8) {
//        int64_t leftVal = *left;
//        int64_t rightVal = *right;
//        if (leftVal != rightVal) {
//            return (LongBytesToLong(leftVal) < LongBytesToLong(rightVal)) ? -1 : 1;
//        }
//        left++;
//        right++;
//        compareLength -= 8;
//        var1 += 8;
//        var2 += SIZE_OF_LONG;
//    }
//    while (compareLength > 0) {
//        int8_t value1 = *(var1);
//        int8_t value2 = *(var2);
//        if (value1 != value2) {
//            return value1 - value2;
//        }
//        var1++;
//        var2++;
//        compareLength--;
//    }
//    return 0;
//}
//
//int32_t CompareVarcharByLong(BaseVector *leftColumn, int32_t leftColumnPosition, BaseVector *rightColumn,
//    int32_t rightColumnPosition)
//{
//    auto leftVarCharColumn = static_cast<VarcharVector *>(leftColumn);
//    auto rightVarCharColumn = static_cast<VarcharVector *>(rightColumn);
//    auto leftValue = leftVarCharColumn->GetValue(leftColumnPosition);
//    auto rightValue = rightVarCharColumn->GetValue(rightColumnPosition);
//    int32_t result = CmpByLong((char*)leftValue.data(), (char*)rightValue.data(),
//                               std::min(leftValue.size(), rightValue.size()));
//    if (result != 0) {
//        return (result > 0) ? 1 : -1;
//    } else if (leftValue.size() == rightValue.size()) {
//        return 0;
//    } else {
//        return (leftValue.size() > rightValue.size()) ? 1 : -1;
//    }
//}
//
//void PrintCompareVarcharResultByLong(BaseVector *vector1, BaseVector *vector2, Timer &timer)
//{
//    double sum = 0.0f;
//    int comp;
//    for (int j = 0; j < ROUNDS; j++) {
//        timer.Reset();
//
//        for (int i = 0; i < ROW_SIZE; i++) {
//            comp = CompareVarcharByLong(vector1, i, vector2, i);
//        }
//
//        timer.CalculateElapse();
//        double wallElapsed = timer.GetWallElapse() * 1000;
//        double cpuElapsed = timer.GetCpuElapse() * 1000;
//        std::cout << "round: " << (j + 1) << " wall " << wallElapsed << " cpu " << cpuElapsed << std::endl;
//        sum += wallElapsed;
//    }
//    std::cout << "avg: " << sum / ROUNDS << " ms" << std::endl;
//    std::cout << "comp: " << comp << std::endl;
//}
//
//TEST(varcharType, CompareVarcharPerf)
//{
//    VarcharVector *vector1 = new VarcharVector(ROW_SIZE);
//    VarcharVector *vector2 = new VarcharVector(ROW_SIZE);
//
//    for (int i = 0; i < ROW_SIZE; i++) {
//        std::string str = GetString(i, 10, VAR_LEN);
//        std::string_view value(str);
//        vector1->SetValue(i, value);
//        vector2->SetValue(i, value);
//    }
//
//    std::cout << "Test times: " << ROW_SIZE << std::endl;
//    std::cout << "varchar length: " << VAR_LEN << std::endl;
//
//    Timer timer;
//    timer.SetStart();
//
//    std::cout << "Compare same varchar: " << std::endl;
//
//    PrintCompareVarcharResult(vector1, vector2, timer);
//
//    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
//    for (int i = 0; i < ROW_SIZE; i++) {
//        std::string str = GetString(i, 20, VAR_LEN);
//        std::string_view value(str);
//        vector3->SetValue(i, value);
//    }
//    std::cout << "Compare different varchar:" << std::endl;
//
//    PrintCompareVarcharResult(vector1, vector3, timer);
//
//    delete vector1;
//    delete vector2;
//    delete vector3;
//}
//
//TEST(varcharType, CompareVarcharByLongPerf)
//{
//    VarcharVector *vector1 = new VarcharVector(ROW_SIZE);
//    VarcharVector *vector2 = new VarcharVector(ROW_SIZE);
//
//    for (int i = 0; i < ROW_SIZE; i++) {
//        std::string str = GetString(i, 10, VAR_LEN);
//        std::string_view value(str);
//        vector1->SetValue(i, value);
//        vector2->SetValue(i, value);
//    }
//
//    // Test perf
//    std::cout << "Test times: " << ROW_SIZE << std::endl;
//    std::cout << "varchar length: " << VAR_LEN << std::endl;
//
//    Timer timer;
//    timer.SetStart();
//
//    std::cout << "Compare equal varchar" << std::endl;
//
//    PrintCompareVarcharResultByLong(vector1, vector2, timer);
//
//    VarcharVector *vector3 = new VarcharVector(ROW_SIZE);
//    for (int i = 0; i < ROW_SIZE; i++) {
//        std::string str = GetString(i, 20, VAR_LEN);
//        std::string_view value(str);
//        vector3->SetValue(i, value);
//    }
//
//    std::cout << "Compare not equal varchar" << std::endl;
//
//    PrintCompareVarcharResultByLong(vector1, vector3, timer);
//
//    delete vector1;
//    delete vector2;
//    delete vector3;
//}
//
//TEST(CompareDictionaryIntPerf, intType)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<int> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_INT,  vectorSize));
//    auto buffer = unsafe::UnsafeVector::GetValues(baseVector)->GetBuffer();
//    for (size_t i = 0; i < vectorSize; i++) {
//        buffer[i] = i;
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_INT);
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_INT);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_INT);
//    typeIds.emplace_back(OMNI_INT);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    // auto vector_function = makeLessThan("lessThan", typeIds, config::QueryConfig{});
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//
//}
//
//
//TEST(CompareDictionaryVarcharPerf, equal)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = i > 2 ? i + 1 : static_cast<int>(i);
//    }
//
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//
//}
//
//
//TEST(CompareDictionaryVarcharPerf, lessThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    auto vector_function = makeLessThan("lessThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//}
//
//TEST(CompareDictionaryVarcharPerf, greaterThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    auto vector_function = makeGreaterThan("greaterThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//}
//
//TEST(CompareDictionaryVarcharPerf, greaterThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    auto vector_function = makeGreaterThanOrEqual("greaterThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//}
//
//TEST(CompareDictionaryVarcharPerf, lessThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory2 = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(dictionaryVectory2);
//
//    auto vector_function = makeLessThanOrEqual("lessThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//
//    delete dictionaryVectory;
//    delete dictionaryVectory2;
//    delete baseVector;
//}
//
//TEST(CompareFlatWithDictionaryIntPerf, intType)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<int> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_INT,  vectorSize));
//    auto buffer = unsafe::UnsafeVector::GetValues(baseVector)->GetBuffer();
//    for (size_t i = 0; i < vectorSize; i++) {
//        buffer[i] = i;
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_INT);
//    auto flatVector = reinterpret_cast<Vector<int> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_INT, rowSize));
//    auto flatBuffer = unsafe::UnsafeVector::GetValues(flatVector)->GetBuffer();
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        flatBuffer[i] = i;
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_INT);
//    typeIds.emplace_back(OMNI_INT);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//
//}
//
//TEST(CompareFlatWithDictionaryVarcharPerf, equal)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = i > 2 ? i + 1 : static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    auto flatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_VARCHAR, rowSize));
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        flatVector->SetValue(i, sv);
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//}
//
//TEST(CompareFlatWithDictionaryVarcharPerf, lessThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    auto flatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_VARCHAR, rowSize));
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        flatVector->SetValue(i, sv);
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeLessThan("lessThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//}
//
//TEST(CompareFlatWithDictionaryVarcharPerf, greaterThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    auto flatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_VARCHAR, rowSize));
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        flatVector->SetValue(i, sv);
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeGreaterThan("greaterThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//}
//
//TEST(CompareFlatWithDictionaryVarcharPerf, greaterThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    auto flatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_VARCHAR, rowSize));
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        flatVector->SetValue(i, sv);
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeGreaterThanOrEqual("greaterThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//}
//
//TEST(CompareFlatWithDictionaryVarcharPerf, lessThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i + 2);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    auto flatVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateFlatVector(nullptr, type::OMNI_VARCHAR, rowSize));
//
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        flatVector->SetValue(i, sv);
//    }
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(flatVector);
//
//    auto vector_function = makeLessThanOrEqual("lessThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete flatVector;
//    delete baseVector;
//}
//
//TEST(CompareConstWithDictionaryIntPerf, intType)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<int> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_INT,  vectorSize));
//    auto buffer = unsafe::UnsafeVector::GetValues(baseVector)->GetBuffer();
//    for (size_t i = 0; i < vectorSize; i++) {
//        buffer[i] = i;
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_INT);
//    auto constVector = new ConstVector<int32_t>(5, OMNI_INT, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_INT);
//    typeIds.emplace_back(OMNI_INT);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//TEST(CompareConstWithDictionaryVarcharPerf, equal)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    std::string_view constValue = prefix + std::to_string(1);
//    auto constVector = new ConstVector<std::string_view>(constValue, OMNI_VARCHAR, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeEqualTo("equal", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    std::cout << "Equal" << std::endl;
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//
//TEST(CompareConstWithDictionaryVarcharPerf, lessThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    std::string_view constValue = prefix + std::to_string(1);
//    auto constVector = new ConstVector<std::string_view>(constValue, OMNI_VARCHAR, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeLessThan("lessThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//TEST(CompareConstWithDictionaryVarcharPerf, greaterThan)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    std::string_view constValue = prefix + std::to_string(1);
//    auto constVector = new ConstVector<std::string_view>(constValue, OMNI_VARCHAR, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeGreaterThan("greaterThan", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//TEST(CompareConstWithDictionaryVarcharPerf, greaterThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    std::string_view constValue = prefix + std::to_string(1);
//    auto constVector = new ConstVector<std::string_view>(constValue, OMNI_VARCHAR, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeGreaterThanOrEqual("greaterThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//
//TEST(CompareConstWithDictionaryVarcharPerf, lessThanOrEqual)
//{
//
//    int vectorSize = 100;
//    int rowSize = 10;
//    auto baseVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR,  vectorSize));
//
//    auto prefix = "compare";
//    for (size_t i = 0; i < vectorSize; i++) {
//        std::string_view sv = prefix + std::to_string(i);
//        baseVector->SetValue(i, sv);
//    }
//
//    std::vector<int> indexes(rowSize);
//    for (size_t i = 0; i < indexes.size(); ++i) {
//        indexes[i] = static_cast<int>(i);
//    }
//
//    auto dictionaryVectory = VectorHelper::CreateDictionaryVector(indexes.data(), 10, baseVector, type::OMNI_VARCHAR);
//    std::string_view constValue = prefix + std::to_string(1);
//    auto constVector = new ConstVector<std::string_view>(constValue, OMNI_VARCHAR, rowSize);
//
//    std::vector<DataTypeId> typeIds;
//    typeIds.emplace_back(OMNI_VARCHAR);
//    typeIds.emplace_back(OMNI_VARCHAR);
//
//    std::stack<BaseVector *> s;
//    s.push(dictionaryVectory);
//    s.push(constVector);
//
//    auto vector_function = makeLessThanOrEqual("lessThanOrEqual", typeIds, config::QueryConfig{});
//    auto result = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
//    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);
//    op::ExecutionContext context;
//    context.SetResultRowSize(rowSize);
//    vector_function->Apply(s, arrayType, result, &context);
//
//    auto printResult = reinterpret_cast<Vector<bool> *>(result);
//    for (size_t i = 0; i < rowSize; i++) {
//        std::cout << "result is " << printResult->GetValue(i) << std::endl;
//    }
//
//    delete dictionaryVectory;
//    delete baseVector;
//}
//
//}