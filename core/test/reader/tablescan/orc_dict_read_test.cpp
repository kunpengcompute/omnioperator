/**
 * Copyright (C) 2023-2023. Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Tests for ORC dictionary-encoded string columns read as DictionaryVector.

#include "reader/orc/OmniWriter.hh"
#include "reader/orc/OmniRowReaderImpl.hh"
#include "reader/orc/OrcFileOverride.hh"
#include <vector/vector_common.h>
#include "vector/dictionary_container.h"
#include "orc/OrcFile.hh"
#include "scan_test.h"
#include <memory>
#include "memory/omni_task_memory_pool.h"
#include "reader/ReaderFactory.h"
#include "reader/ReaderOptions.h"
#include <orc/Type.hh>
#include <gtest/gtest.h>

using namespace omniruntime::vec;
using namespace omniruntime::writer;
using namespace omniruntime::reader;

using OmniStringVector = Vector<LargeStringContainer<std::string_view>>;
using DictionaryStringVector = Vector<DictionaryContainer<std::string_view>>;

class DictReadTest : public testing::Test {
protected:
    std::string filename;
    std::vector<BaseVector*> readBatch;

    virtual void SetUp() override {
        setenv("TZ", "Asia/Shanghai", 1);
        tzset();
        filename = "/tmp/omni_test/dict_read_test_" + std::to_string(std::time(nullptr)) + ".orc";
    }

    virtual void TearDown() override {
        for (auto v : readBatch) delete v;
        readBatch.clear();
        remove(filename.c_str());
    }

    // Read ORC file; pass omniTypeIds array (nullptr = auto detect type from ORC schema)
    void ScanFile(int numRows, int* omniTypeIds) {
        orc::ReaderOptions readerOpts;
        std::unique_ptr<orc::Reader> reader = omniruntime::reader::omniCreateReader(
                readFileOverride(UriInfo("file", filename, "", "-1")), readerOpts);

        orc::RowReaderOptions rowOpts;
        std::unique_ptr<common::JulianGregorianRebase> julian;
        std::unique_ptr<common::PredicateCondition> pred;

        auto readerImpl = dynamic_cast<OmniReaderImpl*>(reader.get());
        ASSERT_NE(readerImpl, nullptr) << "Failed to create OmniReaderImpl";

        auto rowReader = readerImpl->createRowReader(rowOpts, julian, pred);
        auto omniRowReader = dynamic_cast<OmniRowReaderImpl*>(rowReader.get());
        ASSERT_NE(omniRowReader, nullptr) << "Failed to create OmniRowReaderImpl";

        omniRowReader->next(&readBatch, omniTypeIds, numRows);
    }

    // Helper: write a single STRING/VARCHAR/CHAR column ORC file
    void WriteStringOrcFile(const std::vector<std::string>& data,
                            const std::vector<bool>& isNulls,
                            std::unique_ptr<orc::Type> colType)
    {
        int numRows = static_cast<int>(data.size());

        UriInfo uri("file", filename, "", "-1");
        std::unique_ptr<orc::OutputStream> outStream = writeFileOverride(uri);
        std::unique_ptr<orc::Type> schema = orc::createPrimitiveType(orc::TypeKind::STRUCT);
        schema->addStructField("c0", std::move(colType));

        orc::WriterOptions options;
        options.setMemoryPool(orc::getDefaultPool());
        options.setStripeSize(67108864);
        options.setTimezoneName("GMT");
        options.setDictionaryKeySizeThreshold(1.0);
        std::unique_ptr<OmniWriter> writer = createOmniWriter(*schema, outStream.get(), options);

        auto valVec = std::make_unique<OmniStringVector>(numRows);
        for (int i = 0; i < numRows; ++i) {
            if (!isNulls.empty() && isNulls[i]) {
                valVec->SetNull(i);
            } else {
                std::string_view sv(data[i]);
                valVec->SetValue(i, sv);
                valVec->SetNotNull(i);
            }
        }

        std::vector<BaseVector *> cols;
        cols.push_back(valVec.get());
        auto rowVec = std::make_unique<RowVector>(numRows, cols);
        for (int i = 0; i < numRows; ++i) {
            rowVec->SetNotNull(i);
        }

        writer->add(rowVec.get(), 0, numRows);
        writer->close();
    }
};

TEST_F(DictReadTest, ReadStringAsDictVector)
{
    int32_t dictSize = 5;
    int32_t numRows = 100;

    std::vector<std::string> dictValues = {"apple", "banana", "cherry", "date", "elderberry"};
    std::vector<std::string> expectedData(numRows);
    for (int i = 0; i < numRows; ++i) {
        expectedData[i] = dictValues[i % dictSize];
    }

    WriteStringOrcFile(expectedData, {}, orc::createPrimitiveType(orc::TypeKind::STRING));

    ScanFile(numRows, nullptr);
    ASSERT_EQ(readBatch.size(), 1);

    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_DICTIONARY)
        << "Expected DictionaryVector for dictionary-encoded string column";

    auto *dictVec = dynamic_cast<DictionaryStringVector *>(readBatch[0]);
    ASSERT_NE(dictVec, nullptr) << "Failed to cast to DictionaryStringVector";

    for (int i = 0; i < numRows; ++i) {
        std::string_view actual = dictVec->GetValue(i);
        ASSERT_EQ(actual, expectedData[i]) << "Mismatch at row " << i;
    }
}

TEST_F(DictReadTest, ReadStringAsDictVectorWithNulls)
{
    int32_t dictSize = 5;
    int32_t numRows = 100;

    std::vector<std::string> dictValues = {"alpha", "bravo", "charlie", "delta", "echo"};
    std::vector<std::string> expectedData(numRows);
    std::vector<bool> isNulls(numRows, false);
    for (int i = 0; i < numRows; ++i) {
        if (i % 7 == 0) {
            isNulls[i] = true;
        } else {
            expectedData[i] = dictValues[i % dictSize];
        }
    }

    WriteStringOrcFile(expectedData, isNulls, orc::createPrimitiveType(orc::TypeKind::STRING));

    ScanFile(numRows, nullptr);
    ASSERT_EQ(readBatch.size(), 1);

    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_DICTIONARY)
        << "Expected DictionaryVector for dictionary-encoded string column";

    auto *dictVec = dynamic_cast<DictionaryStringVector *>(readBatch[0]);
    ASSERT_NE(dictVec, nullptr) << "Failed to cast to DictionaryStringVector";

    for (int i = 0; i < numRows; ++i) {
        if (isNulls[i]) {
            ASSERT_TRUE(dictVec->IsNull(i)) << "Row " << i << " should be NULL";
        } else {
            ASSERT_FALSE(dictVec->IsNull(i)) << "Row " << i << " should NOT be NULL";
            std::string_view actual = dictVec->GetValue(i);
            ASSERT_EQ(actual, expectedData[i]) << "Mismatch at row " << i;
        }
    }
}

TEST_F(DictReadTest, ReadVarcharAsDictVector)
{
    int32_t dictSize = 4;
    int32_t numRows = 80;

    std::vector<std::string> dictValues = {"var_one", "var_two", "var_three", "var_four"};
    std::vector<std::string> expectedData(numRows);
    for (int i = 0; i < numRows; ++i) {
        expectedData[i] = dictValues[i % dictSize];
    }

    WriteStringOrcFile(expectedData, {}, orc::createCharType(orc::TypeKind::VARCHAR, 65535));

    ScanFile(numRows, nullptr);
    ASSERT_EQ(readBatch.size(), 1);

    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_DICTIONARY)
        << "Expected DictionaryVector for dictionary-encoded varchar column";

    auto *dictVec = dynamic_cast<DictionaryStringVector *>(readBatch[0]);
    ASSERT_NE(dictVec, nullptr) << "Failed to cast to DictionaryStringVector";

    for (int i = 0; i < numRows; ++i) {
        std::string_view actual = dictVec->GetValue(i);
        ASSERT_EQ(actual, expectedData[i]) << "Mismatch at row " << i;
    }
}

TEST_F(DictReadTest, ReadCharAsDictVector)
{
    int32_t dictSize = 3;
    int32_t numRows = 60;

    // CHAR type: ORC pads values to fixed length; OmniReader trims trailing spaces
    std::vector<std::string> dictValues = {"cat", "dog", "fox"};
    std::vector<std::string> expectedData(numRows);
    for (int i = 0; i < numRows; ++i) {
        expectedData[i] = dictValues[i % dictSize];
    }

    WriteStringOrcFile(expectedData, {}, orc::createCharType(orc::TypeKind::CHAR, 20));

    int charTypeId = omniruntime::type::OMNI_CHAR;
    ScanFile(numRows, &charTypeId);
    ASSERT_EQ(readBatch.size(), 1);

    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_DICTIONARY)
        << "Expected DictionaryVector for dictionary-encoded char column";

    auto *dictVec = dynamic_cast<DictionaryStringVector *>(readBatch[0]);
    ASSERT_NE(dictVec, nullptr) << "Failed to cast to DictionaryStringVector";

    // CHAR type: trailing spaces should be trimmed by the reader
    for (int i = 0; i < numRows; ++i) {
        std::string_view actual = dictVec->GetValue(i);
        ASSERT_EQ(actual, expectedData[i]) << "Mismatch at row " << i;
    }
}

TEST_F(DictReadTest, ReadMultiColWithDictVector)
{
    // Test mixed schema: INT column (flat) + STRING column (dictionary)
    int32_t dictSize = 4;
    int32_t numRows = 80;

    std::vector<int32_t> intData(numRows);
    std::vector<std::string> dictValues = {"red", "green", "blue", "yellow"};
    std::vector<std::string> strData(numRows);
    for (int i = 0; i < numRows; ++i) {
        intData[i] = i * 10;
        strData[i] = dictValues[i % dictSize];
    }

    UriInfo uri("file", filename, "", "-1");
    std::unique_ptr<orc::OutputStream> outStream = writeFileOverride(uri);
    std::unique_ptr<orc::Type> schema = orc::createPrimitiveType(orc::TypeKind::STRUCT);
    schema->addStructField("c0", orc::createPrimitiveType(orc::TypeKind::INT));
    schema->addStructField("c1", orc::createPrimitiveType(orc::TypeKind::STRING));

    orc::WriterOptions options;
    options.setMemoryPool(orc::getDefaultPool());
    options.setStripeSize(67108864);
    options.setTimezoneName("GMT");
    options.setDictionaryKeySizeThreshold(1.0);
    std::unique_ptr<OmniWriter> writer = createOmniWriter(*schema, outStream.get(), options);

    auto intVec = std::make_unique<Vector<int32_t>>(numRows);
    auto strVec = std::make_unique<OmniStringVector>(numRows);
    for (int i = 0; i < numRows; ++i) {
        intVec->SetValue(i, intData[i]);
        intVec->SetNotNull(i);
        std::string_view sv(strData[i]);
        strVec->SetValue(i, sv);
        strVec->SetNotNull(i);
    }

    std::vector<BaseVector *> cols;
    cols.push_back(intVec.get());
    cols.push_back(strVec.get());
    auto rowVec = std::make_unique<RowVector>(numRows, cols);
    for (int i = 0; i < numRows; ++i) {
        rowVec->SetNotNull(i);
    }

    writer->add(rowVec.get(), 0, numRows);
    writer->close();
    writer.reset();
    outStream.reset();

    ScanFile(numRows, nullptr);
    ASSERT_EQ(readBatch.size(), 2);

    // Column 0: INT - should be flat
    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_FLAT);
    auto *resIntVec = dynamic_cast<Vector<int32_t> *>(readBatch[0]);
    ASSERT_NE(resIntVec, nullptr);
    for (int i = 0; i < numRows; ++i) {
        ASSERT_EQ(resIntVec->GetValue(i), intData[i]) << "INT mismatch at row " << i;
    }

    // Column 1: STRING - should be dictionary-encoded
    ASSERT_EQ(readBatch[1]->GetEncoding(), OMNI_DICTIONARY)
        << "Expected DictionaryVector for dictionary-encoded string column";

    auto *dictVec = dynamic_cast<DictionaryStringVector *>(readBatch[1]);
    ASSERT_NE(dictVec, nullptr) << "Failed to cast to DictionaryStringVector";
    for (int i = 0; i < numRows; ++i) {
        std::string_view actual = dictVec->GetValue(i);
        ASSERT_EQ(actual, strData[i]) << "STRING mismatch at row " << i;
    }
}

TEST_F(DictReadTest, DictionaryPayloadStaysAccountedAfterReaderDestruction)
{
    const std::string value(65536, 'x');
    WriteStringOrcFile(std::vector<std::string>(100, value), {},
        orc::createPrimitiveType(orc::TypeKind::STRING));
    omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
    struct Cleanup {
        std::vector<BaseVector*>& vectors;
        ~Cleanup() { for (auto *v : vectors) delete v; vectors.clear(); }
    } cleanup{readBatch};
    std::vector<BaseVector*> *batch = nullptr;
    {
        auto options = std::make_shared<omniruntime::reader::ReaderOptions>();
        options->ParseEnhanceJson("{}");
        options->SetTaskPool(&pool);
        auto orcOptions = std::make_shared<orc::ReaderOptions>();
        orcOptions->setMemoryPool(*orc::getDefaultPool());
        options->SetOrcReaderOptions(orcOptions);
        options->SetOrcRowReaderOptions(std::make_shared<orc::RowReaderOptions>());
        options->SetUri(std::make_shared<UriInfo>("file", filename, "", "-1"));
        auto rowType = omniruntime::type::ROW({}, {});
        options->SetRowType(rowType);
        options->SetFileRowType(rowType);
        auto reader = GetReaderFactory(omniruntime::codegen::FileFormat::ORC)->CreateReader(options);
        options->GetOrcRowReaderOptions().include(std::list<std::string>{"c0"});
        auto rowReader = reader->CreateRowReader();
        ASSERT_EQ(rowReader->Next(&batch, nullptr, 100), 100);
        ASSERT_NE(batch, nullptr);
        ASSERT_EQ(batch->size(), 1);
        // Move vector ownership out of the reader's returned batch.
        readBatch.swap(*batch);
        delete batch;
        batch = nullptr;
    }
    ASSERT_EQ(readBatch[0]->GetEncoding(), OMNI_DICTIONARY);
    auto *dict = dynamic_cast<DictionaryStringVector*>(readBatch[0]);
    ASSERT_NE(dict, nullptr);
    EXPECT_EQ(dict->GetValue(99), value);
    EXPECT_GE(pool.currentBytes(), static_cast<int64_t>(value.size()));
    for (auto *vector : readBatch) delete vector;
    readBatch.clear();
    EXPECT_EQ(pool.currentBytes(), 0);
}
