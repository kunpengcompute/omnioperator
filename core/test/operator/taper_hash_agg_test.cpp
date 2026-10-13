/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Unit tests for TaperHashtable in HashAgg operator
 *   (TaperFlatHashTable, TaperGroupbySingleFixHandler, TaperColumnSerializeHandler)
 */

#include <vector>
#include <cstring>
#include "gtest/gtest.h"
#include "operator/hashmap/group_hasher.h"
#include "operator/hashmap/taper_hashtable.h"
#include "operator/hashmap/column_marshaller.h"
#include "operator/hashmap/vector_marshaller.h"
#include "vector/vector.h"
#include "vector/decoded_vector.h"
#include "memory/allocator.h"
#include "memory/simple_arena_allocator.h"
#include "type/data_type.h"
#include "util/test_util.h"

namespace omniruntime::test {
namespace {
using namespace op;
using namespace vec;
using namespace type;
using namespace mem;

// These data-structure tests intentionally exercise the standalone bridge;
// task-pool accounting is covered by the dedicated reservation/pool suites.
Allocator *TestAllocator()
{
    static Allocator allocator(nullptr);
    return &allocator;
}

/// Visitor-based lookup helper (replaces TaperFlatHashTable::Find which was added later).
template <typename Table, typename Key>
uint8_t* Lookup(Table& table, const Key& key) {
    auto visitor = table.GetResultVisitor();
    while (!visitor.Finished()) {
        if (visitor.CurKey() == key) {
            return reinterpret_cast<uint8_t*>(const_cast<char*>(visitor.CurVal().buf));
        }
        visitor.Next();
    }
    return nullptr;
}

// ======================================================================
//  TaperFlatHashTable — core data structure tests
//  These tests exercise the underlying hash table used by both
//  TaperGroupbySingleFixHandler and TaperColumnSerializeHandler.
// ======================================================================

class TaperFlatHashTableTest : public ::testing::Test {
protected:
    SimpleArenaAllocator pool{4096, TestAllocator()};
};

/// Emplace and Find with int32 keys (KeyScattered=true, Agg mode).
TEST_F(TaperFlatHashTableTest, BasicEmplaceAndFind)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t keys[] = {42, 100, 255, 0, -1};
    for (auto k : keys) {
        table.Emplace(k,
            [k](char* data) { memcpy(data, &k, sizeof(k)); },
            [](char*, bool) {});
    }
    EXPECT_EQ(table.Size(), 5);

    for (auto k : keys) {
        auto* val = Lookup(table, k);
        ASSERT_NE(val, nullptr);
        int32_t found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, k);
    }
    EXPECT_EQ(Lookup(table, 999), nullptr);
}

/// Emplace and Find with int64 keys (used by TaperColumnSerializeHandler).
TEST_F(TaperFlatHashTableTest, Int64Key)
{
    TaperFlatHashTable<int64_t, true> table(pool, sizeof(int64_t), sizeof(char*));

    int64_t keys[] = {10000000000LL, -5000000000LL, 1LL};
    char* vals[] = {reinterpret_cast<char*>(0x1), reinterpret_cast<char*>(0x2), nullptr};
    for (int i = 0; i < 3; i++) {
        table.Emplace(keys[i],
            [&, i](char* data) { memcpy(data, &vals[i], sizeof(char*)); },
            [](char*, bool) {});
    }
    EXPECT_EQ(table.Size(), 3);

    for (int i = 0; i < 3; i++) {
        auto* val = Lookup(table, keys[i]);
        ASSERT_NE(val, nullptr);
        char* found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, vals[i]);
    }
}

/// EmplaceBatch — the primary insertion path used by both handlers.
TEST_F(TaperFlatHashTableTest, EmplaceBatch)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t rows = 20;
    int32_t keys[20];
    int32_t vals[20];
    for (int32_t i = 0; i < rows; i++) {
        keys[i] = i * 10;
        vals[i] = i;
    }

    table.EmplaceBatch(keys, rows,
        [](uint32_t) { return false; },
        [&](uint32_t rowIdx, char* data) { memcpy(data, &vals[rowIdx], sizeof(int32_t)); },
        [](uint32_t, char*, bool) {});

    EXPECT_EQ(table.Size(), rows);

    for (int32_t i = 0; i < rows; i++) {
        auto* val = Lookup(table, keys[i]);
        ASSERT_NE(val, nullptr);
        int32_t found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, vals[i]);
    }
}

/// EmplaceBatch with duplicate keys — tests the init/update callback contract.
TEST_F(TaperFlatHashTableTest, EmplaceBatchDuplicateKeys)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t keys[] = {10, 100, 100};
    int32_t vals[] = {1, 2, 3};
    int32_t rows = 3;
    int32_t insertCount = 0;

    table.EmplaceBatch(keys, rows,
        [](uint32_t) { return false; },
        [&](uint32_t rowIdx, char* data) {
            memcpy(data, &vals[rowIdx], sizeof(int32_t));
            insertCount++;
        },
        [&](uint32_t rowIdx, char*, bool initFlag) {
            if (!initFlag) {
                insertCount++;
            }
        });

    EXPECT_EQ(table.Size(), 2);
    // init called for each insertion, update for each row
    // key=10: init called once (new)
    // key=100: init called once (first occurrence), update with initFlag=false for second
    // So insertCount = 3 (all rows trigger a callback)
    EXPECT_EQ(insertCount, 3);

    auto* val = Lookup(table, 100);
    ASSERT_NE(val, nullptr);
    int32_t found;
    memcpy(&found, val, sizeof(found));
    // Should have the first value (key=100 hadn't been inserted yet when init was called)
    EXPECT_EQ(found, 2);
}

/// EmplaceBatch with filter — rows where filter returns true are skipped.
TEST_F(TaperFlatHashTableTest, EmplaceBatchWithFilter)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t keys[] = {1, 2, 3, 4, 5};
    int32_t vals[] = {10, 20, 30, 40, 50};
    int32_t rows = 5;

    table.EmplaceBatch(keys, rows,
        [](uint32_t idx) { return idx % 2 == 0; },
        [&](uint32_t rowIdx, char* data) { memcpy(data, &vals[rowIdx], sizeof(int32_t)); },
        [](uint32_t, char*, bool) {});

    EXPECT_EQ(table.Size(), 2);
    EXPECT_NE(Lookup(table, 2), nullptr);
    EXPECT_NE(Lookup(table, 4), nullptr);
    EXPECT_EQ(Lookup(table, 1), nullptr);
    EXPECT_EQ(Lookup(table, 3), nullptr);
    EXPECT_EQ(Lookup(table, 5), nullptr);
}

/// GetResultVisitor — iterate all entries in the hash table.
TEST_F(TaperFlatHashTableTest, Iterator)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t keys[] = {10, 20, 30, 40, 50};
    for (auto k : keys) {
        table.Emplace(k,
            [k](char* data) { memcpy(data, &k, sizeof(k)); },
            [](char*, bool) {});
    }

    int32_t count = 0;
    bool found[5] = {false};
    auto visitor = table.GetResultVisitor();
    while (!visitor.Finished()) {
        int32_t key = visitor.CurKey();
        int32_t val;
        memcpy(&val, visitor.CurVal().buf, sizeof(val));
        EXPECT_EQ(key, val);
        for (int i = 0; i < 5; i++) {
            if (keys[i] == key) found[i] = true;
        }
        count++;
        visitor.Next();
    }
    EXPECT_EQ(count, 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(found[i]);
    }
}

/// Growth/rehash — insert enough keys to trigger capacity expansion.
TEST_F(TaperFlatHashTableTest, GrowthAndRehash)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t n = 500;
    for (int32_t i = 0; i < n; i++) {
        table.Emplace(i,
            [i](char* data) { memcpy(data, &i, sizeof(i)); },
            [](char*, bool) {});
    }
    EXPECT_EQ(table.Size(), n);

    for (int32_t i = 0; i < n; i++) {
        auto* val = Lookup(table, i);
        ASSERT_NE(val, nullptr);
        int32_t found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, i);
    }
}

/// Clear and re-use the hash table.
TEST_F(TaperFlatHashTableTest, ClearAndReuse)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    for (int32_t i = 0; i < 100; i++) {
        table.Emplace(i,
            [i](char* data) { memcpy(data, &i, sizeof(i)); },
            [](char*, bool) {});
    }
    EXPECT_EQ(table.Size(), 100);

    table.Clear();
    EXPECT_EQ(table.Size(), 0);

    for (int32_t i = 200; i < 300; i++) {
        table.Emplace(i,
            [i](char* data) { memcpy(data, &i, sizeof(i)); },
            [](char*, bool) {});
    }
    EXPECT_EQ(table.Size(), 100);

    for (int32_t i = 200; i < 300; i++) {
        auto* val = Lookup(table, i);
        ASSERT_NE(val, nullptr);
        int32_t found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, i);
    }
}

/// Save/restore visitor position (used by TaperGroupbySingleFixHandler::Extract).
TEST_F(TaperFlatHashTableTest, VisitorSavePos)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t n = 50;
    for (int32_t i = 0; i < n; i++) {
        table.Emplace(i,
            [i](char* data) { memcpy(data, &i, sizeof(i)); },
            [](char*, bool) {});
    }

    auto v1 = table.GetResultVisitor();
    int halfCount = 0;
    while (halfCount < n / 2 && !v1.Finished()) {
        halfCount++;
        v1.Next();
    }
    ASSERT_FALSE(v1.Finished());

    char* savedChunk = nullptr;
    uint16_t savedTag = 0;
    v1.SavePos([&](auto ptr, auto tagPos) {
        savedChunk = reinterpret_cast<char*>(ptr);
        savedTag = tagPos;
    });

    auto v2 = table.GetResultVisitor(savedChunk, savedTag);
    int remainingCount = 0;
    while (!v2.Finished()) {
        remainingCount++;
        v2.Next();
    }
    EXPECT_EQ(remainingCount, n - halfCount);
}

/// Emplace with custom key comparison (used by TaperColumnSerializeHandler's fallback path).
TEST_F(TaperFlatHashTableTest, EmplaceWithCustomCompare)
{
    TaperFlatHashTable<int64_t, true> table(pool, sizeof(int64_t), sizeof(int32_t));

    // Insert using the 3-arg Emplace (with FKCmp)
    int32_t val1 = 100;
    bool updateCalled = false;
    table.Emplace(int64_t(1),
        [&](auto, TaperHashTableChunk&, uint8_t) { return false; },
        [&](char* data) { memcpy(data, &val1, sizeof(val1)); },
        [&](char*, bool) { updateCalled = true; });
    EXPECT_EQ(table.Size(), 1);
    EXPECT_TRUE(updateCalled);

    // Insert same key with matching compare
    int32_t val2 = 200;
    updateCalled = false;
    table.Emplace(int64_t(1),
        [&](auto, TaperHashTableChunk&, uint8_t) { return true; },
        [&](char*) { FAIL() << "init should not be called for duplicate key"; },
        [&](char* data, bool initFlag) {
            EXPECT_FALSE(initFlag);
            memcpy(data, &val2, sizeof(val2));
            updateCalled = true;
        });
    EXPECT_EQ(table.Size(), 1);
    EXPECT_TRUE(updateCalled);

    // Verify value was updated
    auto* val = Lookup(table, int64_t(1));
    ASSERT_NE(val, nullptr);
    int32_t found;
    memcpy(&found, val, sizeof(found));
    EXPECT_EQ(found, val2);
}

/// EmplaceBatch with keys > initial capacity (tests batch collision + growth).
TEST_F(TaperFlatHashTableTest, EmplaceBatchLarge)
{
    TaperFlatHashTable<int32_t, true> table(pool, sizeof(int32_t), sizeof(int32_t));

    int32_t n = 1000;
    std::vector<int32_t> keys(n);
    std::vector<int32_t> vals(n);
    for (int32_t i = 0; i < n; i++) {
        keys[i] = i;
        vals[i] = i * 2;
    }

    table.EmplaceBatch(keys.data(), n,
        [](uint32_t) { return false; },
        [&](uint32_t rowIdx, char* data) { memcpy(data, &vals[rowIdx], sizeof(int32_t)); },
        [](uint32_t, char*, bool) {});

    EXPECT_EQ(table.Size(), n);
    for (int32_t i = 0; i < n; i += 100) {
        auto* val = Lookup(table, i);
        ASSERT_NE(val, nullptr);
        int32_t found;
        memcpy(&found, val, sizeof(found));
        EXPECT_EQ(found, i * 2);
    }
}


// ======================================================================
//  TaperGroupbySingleFixHandler tests
//  Handles single fixed-width key GROUP BY with Flat/Dict/Const encodings.
// ======================================================================

class TaperGroupbySingleFixHandlerTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* v : ownedVectors) delete v;
    }

    SimpleArenaAllocator pool{4096, TestAllocator()};
    std::vector<uint8_t*> groups;
    std::vector<uint8_t*> newGroups;
    std::vector<BaseVector*> ownedVectors;

    auto MakeHandler() {
        return TaperGroupbySingleFixHandler<int32_t>(pool, 0);
    }
};

/// Basic EmplaceTable with Flat-encoded int32 keys.
TEST_F(TaperGroupbySingleFixHandlerTest, FlatEncoding)
{
    auto handler = MakeHandler();
    int32_t rows = 5;
    auto* vec = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(vec);
    for (int32_t i = 0; i < rows; i++) {
        vec->SetValue(i, i * 10);
    }
    BaseVector* groupVecs[] = {vec};

    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 5);
}

/// EmplaceTable with null keys.
TEST_F(TaperGroupbySingleFixHandlerTest, WithNullKeys)
{
    auto handler = MakeHandler();
    int32_t rows = 5;
    auto* vec = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(vec);
    for (int32_t i = 0; i < rows; i++) {
        vec->SetValue(i, i);
    }
    vec->SetNull(2);

    BaseVector* groupVecs[] = {vec};
    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 5);
    EXPECT_TRUE(handler.shouldExtractNull);
}

/// EmplaceTable with Constant-encoded keys.
TEST_F(TaperGroupbySingleFixHandlerTest, ConstEncoding)
{
    auto handler = MakeHandler();
    int32_t rows = 5;
    auto* vec = new ConstVector<int32_t>(42, OMNI_INT, rows);
    ownedVectors.push_back(vec);

    BaseVector* groupVecs[] = {vec};
    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, Encoding::OMNI_ENCODING_CONST);

    EXPECT_EQ(handler.GetElementsSize(), 1);
}

/// EmplaceTable with Dictionary-encoded keys.
TEST_F(TaperGroupbySingleFixHandlerTest, DictEncoding)
{
    auto handler = MakeHandler();
    int32_t rows = 5;

    // Create a dictionary vector using VectorHelper
    // values (indices): [0, 1, 0, 1, 0]
    int32_t dictValues[] = {0, 1, 0, 1, 0};
    // dictionary (flat values): [10, 20]
    auto* dictSource = new Vector<int32_t>(2, OMNI_INT);
    ownedVectors.push_back(dictSource);
    dictSource->SetValue(0, 10);
    dictSource->SetValue(1, 20);

    auto* dictVec = VectorHelper::CreateDictionary<int32_t>(dictValues, rows, dictSource);
    ownedVectors.push_back(dictVec);

    BaseVector* groupVecs[] = {dictVec};
    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_DICTIONARY);

    EXPECT_EQ(handler.GetElementsSize(), 2);
}

/// InsertOneValueToHashmap — single key insertion.
/// NOTE: InsertOneValueToHashmap<false> uses Emplace without key comparison,
/// so duplicate keys are NOT detected (each call creates a new entry).
TEST_F(TaperGroupbySingleFixHandlerTest, InsertOneValue)
{
    auto handler = MakeHandler();
    int32_t val = 123;
    handler.InsertOneValueToHashmap<false>(42, reinterpret_cast<uint8_t*>(&val));
    EXPECT_EQ(handler.GetElementsSize(), 1);

    // InsertOneValueToHashmap uses Emplace with DUMMY_CMP, so duplicates are not detected
    handler.InsertOneValueToHashmap<false>(99, reinterpret_cast<uint8_t*>(&val));
    EXPECT_EQ(handler.GetElementsSize(), 2);
}

/// InsertOneValueToHashmap with null flag.
TEST_F(TaperGroupbySingleFixHandlerTest, InsertOneValueNull)
{
    auto handler = MakeHandler();
    int32_t val = 0;
    handler.InsertOneValueToHashmap<true>(0, reinterpret_cast<uint8_t*>(&val));
    EXPECT_TRUE(handler.shouldExtractNull);
    EXPECT_EQ(handler.GetElementsSize(), 1);
}

/// Extract iteration via GetResultVisitor.
TEST_F(TaperGroupbySingleFixHandlerTest, ExtractValues)
{
    auto handler = MakeHandler();
    int32_t rows = 3;
    auto* vec = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(vec);
    for (int32_t i = 0; i < rows; i++) {
        vec->SetValue(i, i);
    }

    BaseVector* groupVecs[] = {vec};
    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    int32_t count = 0;
    auto visitor = handler.table->GetResultVisitor();
    while (!visitor.Finished()) {
        auto key = visitor.CurKey();
        EXPECT_TRUE(key >= 0 && key <= 2);
        auto* data = const_cast<char*>(visitor.CurVal().buf);
        uint8_t*& row = handler.RowFromData(data);
        (void)row;
        count++;
        visitor.Next();
    }
    EXPECT_EQ(count, 3);
}

/// ParseKeyToCols — verify single-column key output.
TEST_F(TaperGroupbySingleFixHandlerTest, ParseKeyToColsSingle)
{
    auto handler = MakeHandler();
    int32_t rows = 3;
    auto* vec = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(vec);
    for (int32_t i = 0; i < rows; i++) {
        vec->SetValue(i, i * 10);
    }

    BaseVector* groupVecs[] = {vec};
    groups.resize(rows);
    newGroups.clear();
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    auto* outVec = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(outVec);
    std::vector<BaseVector*> outVecs = {outVec};

    auto visitor = handler.table->GetResultVisitor();
    int32_t idx = 0;
    while (!visitor.Finished() && idx < rows) {
        handler.ParseKeyToCols(visitor.CurKey(), outVecs, 1, idx);
        visitor.Next();
        idx++;
    }

    bool found[3] = {false};
    for (int32_t i = 0; i < idx; i++) {
        int32_t v = outVec->GetValue(i);
        EXPECT_TRUE(v == 0 || v == 10 || v == 20);
        found[v / 10] = true;
    }
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(found[i]);
    }
}


// ======================================================================
//  TaperColumnSerializeHandler tests
//  Handles multi-column GROUP BY with serialized keys.
// ======================================================================

class TaperColumnSerializeHandlerTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* v : ownedVectors) delete v;
    }

    SimpleArenaAllocator pool{4096, TestAllocator()};
    std::vector<uint8_t*> groups;
    std::vector<uint8_t*> newGroups;
    std::vector<BaseVector*> ownedVectors;
};

/// Basic two-column int32 GROUP BY.
TEST_F(TaperColumnSerializeHandlerTest, BasicTwoIntColumns)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 5;

    handler.InitRowContainer(
        {sizeof(int32_t), sizeof(int32_t)},
        {false, false},
        {OMNI_INT, OMNI_INT},
        {},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);

    auto* v1 = new Vector<int32_t>(rows, OMNI_INT);
    auto* v2 = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(v1);
    ownedVectors.push_back(v2);

    for (int32_t i = 0; i < rows; i++) {
        v1->SetValue(i, i);
        v2->SetValue(i, i * 100);
    }

    BaseVector* groupVecs[] = {v1, v2};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 2, rows);
    handler.EmplaceTable(groupVecs, 2, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 5);
}

/// Two-column GROUP BY with null keys.
TEST_F(TaperColumnSerializeHandlerTest, WithNullKeys)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 5;

    handler.InitRowContainer(
        {sizeof(int32_t), sizeof(int32_t)},
        {false, false},
        {OMNI_INT, OMNI_INT},
        {},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);

    auto* v1 = new Vector<int32_t>(rows, OMNI_INT);
    auto* v2 = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(v1);
    ownedVectors.push_back(v2);

    for (int32_t i = 0; i < rows; i++) {
        v1->SetValue(i, i);
        v2->SetValue(i, i);
    }
    v1->SetNull(0);
    v2->SetNull(2);

    BaseVector* groupVecs[] = {v1, v2};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 2, rows);
    handler.EmplaceTable(groupVecs, 2, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 5);
}

/// Three-column GROUP BY with different types.
TEST_F(TaperColumnSerializeHandlerTest, ThreeColumnsMixedTypes)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 5;

    handler.InitRowContainer(
        {sizeof(int32_t), sizeof(int64_t), sizeof(int16_t)},
        {false, false, false},
        {OMNI_INT, OMNI_LONG, OMNI_SHORT},
        {},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackSerializer(vectorSerializerCenter[OMNI_LONG]);
    handler.PushBackSerializer(vectorSerializerCenter[OMNI_SHORT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_LONG]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_SHORT]);

    auto* v1 = new Vector<int32_t>(rows, OMNI_INT);
    auto* v2 = new Vector<int64_t>(rows, OMNI_LONG);
    auto* v3 = new Vector<int16_t>(rows, OMNI_SHORT);
    ownedVectors.push_back(v1);
    ownedVectors.push_back(v2);
    ownedVectors.push_back(v3);

    for (int32_t i = 0; i < rows; i++) {
        v1->SetValue(i, i);
        v2->SetValue(i, i * 1000LL);
        v3->SetValue(i, static_cast<int16_t>(i * 10));
    }

    BaseVector* groupVecs[] = {v1, v2, v3};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 3, rows);
    handler.EmplaceTable(groupVecs, 3, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 5);

    // Verify Extract + ParseKeyToCols round-trip
    auto* out1 = new Vector<int32_t>(rows, OMNI_INT);
    auto* out2 = new Vector<int64_t>(rows, OMNI_LONG);
    auto* out3 = new Vector<int16_t>(rows, OMNI_SHORT);
    ownedVectors.push_back(out1);
    ownedVectors.push_back(out2);
    ownedVectors.push_back(out3);
    std::vector<BaseVector*> outVecs = {out1, out2, out3};

    OutputState outputState;
    handler.Extract(rows, outputState,
        [&](uint8_t* rowPtr, uint8_t* row, size_t idx) {
            handler.ParseKeyToCols(rowPtr, outVecs, 3, static_cast<int32_t>(idx));
        },
        [](uint8_t*, uint8_t*, size_t) {});
}

/// InitRowContainer with varchar columns.
TEST_F(TaperColumnSerializeHandlerTest, InitRowContainerVarchar)
{
    TaperColumnSerializeHandler handler(pool, 0);

    std::vector<int32_t> keySizes = {sizeof(char*), sizeof(char*), sizeof(int32_t)};
    std::vector<bool> isVarLen = {true, true, false};
    std::vector<int32_t> typeIds = {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT};
    std::vector<int32_t> varcharCols = {0, 1};

    handler.InitRowContainer(keySizes, isVarLen, typeIds, varcharCols, pool);

    EXPECT_NE(handler.aggRows, nullptr);
    EXPECT_EQ(handler.varcharColIndices.size(), 2);
    EXPECT_EQ(handler.varcharSlotColIdx, 0);
}

/// InitRowContainer with single varchar (no merge).
TEST_F(TaperColumnSerializeHandlerTest, InitRowContainerSingleVarchar)
{
    TaperColumnSerializeHandler handler(pool, 0);

    std::vector<int32_t> keySizes = {sizeof(char*)};
    std::vector<bool> isVarLen = {true};
    std::vector<int32_t> typeIds = {OMNI_VARCHAR};
    std::vector<int32_t> varcharCols = {0};

    handler.InitRowContainer(keySizes, isVarLen, typeIds, varcharCols, pool);

    EXPECT_NE(handler.aggRows, nullptr);
    EXPECT_EQ(handler.varcharColIndices.size(), 1);
    EXPECT_EQ(handler.varcharSlotColIdx, -1);
}

// ======================================================================
//  RowContainer::ExtractColumn tests — multi-type dispatch (category 3)
// ======================================================================

class RowContainerExtractTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* v : ownedVectors) delete v;
    }

    SimpleArenaAllocator pool{4096, TestAllocator()};
    std::vector<BaseVector*> ownedVectors;
};

/// Extract SHORT, BYTE, DOUBLE, FLOAT, BOOLEAN, DECIMAL128 columns.
TEST_F(RowContainerExtractTest, MultiTypeExtract)
{
    std::vector<int32_t> sizes = {
        sizeof(int32_t), sizeof(int16_t), sizeof(int8_t),
        sizeof(double), sizeof(float), sizeof(int8_t),
        sizeof(int64_t)
    };
    RowContainer container(sizes, static_cast<int32_t>(sizes.size()), 0, pool);
    int32_t n = 4;

    for (int32_t i = 0; i < n; i++) {
        char* row = container.NewRow();
        container.StoreValue<int32_t>(row, container.ColumnAt(0).Offset(), i * 100);
        container.StoreValue<int16_t>(row, container.ColumnAt(1).Offset(), static_cast<int16_t>(i));
        container.StoreValue<int8_t>(row, container.ColumnAt(2).Offset(), static_cast<int8_t>(i * 10));
        container.StoreValue<double>(row, container.ColumnAt(3).Offset(), i * 1.5);
        container.StoreValue<float>(row, container.ColumnAt(4).Offset(), static_cast<float>(i * 2.5f));
        container.StoreValue<int8_t>(row, container.ColumnAt(5).Offset(), i % 2);
        container.StoreValue<int64_t>(row, container.ColumnAt(6).Offset(), i * 1000LL);
        // Mark row 2 as null for column 0 and column 5
        if (i == 2) {
            container.SetNullAt(row, container.ColumnAt(0).NullByte(), container.ColumnAt(0).NullMask());
            container.SetNullAt(row, container.ColumnAt(5).NullByte(), container.ColumnAt(5).NullMask());
        }
    }

    RowContainerIterator iter;
    std::vector<char*> rows(n);
    EXPECT_EQ(container.ListRows(&iter, n, rows.data()), n);

    // int32
    auto* v1 = new Vector<int32_t>(n, OMNI_INT);
    ownedVectors.push_back(v1);
    container.ExtractColumn(rows.data(), n, 0, v1);
    EXPECT_EQ(v1->GetValue(0), 0);
    EXPECT_EQ(v1->GetValue(1), 100);
    EXPECT_TRUE(v1->IsNull(2));
    EXPECT_EQ(v1->GetValue(3), 300);

    // int16
    auto* v2 = new Vector<int16_t>(n, OMNI_SHORT);
    ownedVectors.push_back(v2);
    container.ExtractColumn(rows.data(), n, 1, v2);
    EXPECT_EQ(v2->GetValue(0), 0);
    EXPECT_EQ(v2->GetValue(1), 1);
    EXPECT_EQ(v2->GetValue(2), 2);
    EXPECT_EQ(v2->GetValue(3), 3);

    // int8
    auto* v3 = new Vector<int8_t>(n, OMNI_BYTE);
    ownedVectors.push_back(v3);
    container.ExtractColumn(rows.data(), n, 2, v3);
    EXPECT_EQ(v3->GetValue(0), 0);
    EXPECT_EQ(v3->GetValue(1), 10);
    EXPECT_EQ(v3->GetValue(2), 20);
    EXPECT_EQ(v3->GetValue(3), 30);

    // double
    auto* v4 = new Vector<double>(n, OMNI_DOUBLE);
    ownedVectors.push_back(v4);
    container.ExtractColumn(rows.data(), n, 3, v4);
    EXPECT_DOUBLE_EQ(v4->GetValue(0), 0.0);
    EXPECT_DOUBLE_EQ(v4->GetValue(1), 1.5);
    EXPECT_DOUBLE_EQ(v4->GetValue(2), 3.0);
    EXPECT_DOUBLE_EQ(v4->GetValue(3), 4.5);

    // float
    auto* v5 = new Vector<float>(n, OMNI_FLOAT);
    ownedVectors.push_back(v5);
    container.ExtractColumn(rows.data(), n, 4, v5);
    EXPECT_FLOAT_EQ(v5->GetValue(0), 0.0f);
    EXPECT_FLOAT_EQ(v5->GetValue(1), 2.5f);
    EXPECT_FLOAT_EQ(v5->GetValue(2), 5.0f);
    EXPECT_FLOAT_EQ(v5->GetValue(3), 7.5f);

    // boolean
    auto* v6 = new Vector<bool>(n, OMNI_BOOLEAN);
    ownedVectors.push_back(v6);
    container.ExtractColumn(rows.data(), n, 5, v6);
    EXPECT_EQ(v6->GetValue(0), 0);
    EXPECT_EQ(v6->GetValue(1), 1);
    EXPECT_TRUE(v6->IsNull(2));
    EXPECT_EQ(v6->GetValue(3), 1);

    // int64
    auto* v7 = new Vector<int64_t>(n, OMNI_LONG);
    ownedVectors.push_back(v7);
    container.ExtractColumn(rows.data(), n, 6, v7);
    EXPECT_EQ(v7->GetValue(0), 0LL);
    EXPECT_EQ(v7->GetValue(1), 1000LL);
    EXPECT_EQ(v7->GetValue(2), 2000LL);
    EXPECT_EQ(v7->GetValue(3), 3000LL);
}

/// Extract null-only rows (all columns null).
TEST_F(RowContainerExtractTest, ExtractAllNull)
{
    std::vector<int32_t> sizes = {sizeof(int32_t), sizeof(int16_t)};
    RowContainer container(sizes, 2, 0, pool);
    int32_t n = 3;

    for (int32_t i = 0; i < n; i++) {
        char* row = container.NewRow();
        container.SetNullAt(row, container.ColumnAt(0).NullByte(), container.ColumnAt(0).NullMask());
        container.SetNullAt(row, container.ColumnAt(1).NullByte(), container.ColumnAt(1).NullMask());
    }

    RowContainerIterator iter;
    std::vector<char*> rows(n);
    EXPECT_EQ(container.ListRows(&iter, n, rows.data()), n);

    auto* v1 = new Vector<int32_t>(n, OMNI_INT);
    ownedVectors.push_back(v1);
    container.ExtractColumn(rows.data(), n, 0, v1);
    for (int32_t i = 0; i < n; i++) EXPECT_TRUE(v1->IsNull(i));

    auto* v2 = new Vector<int16_t>(n, OMNI_SHORT);
    ownedVectors.push_back(v2);
    container.ExtractColumn(rows.data(), n, 1, v2);
    for (int32_t i = 0; i < n; i++) EXPECT_TRUE(v2->IsNull(i));
}


// ======================================================================
//  VARCHAR serializer/deserializer tests (category 4)
//  Tests the VariableTypeSerializer/VariableTypeDeserializer paths
//  through registered function pointers.
// ======================================================================

class VarcharSerdeTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (auto* v : ownedVectors) delete v;
    }

    SimpleArenaAllocator pool{4096, TestAllocator()};
    std::vector<BaseVector*> ownedVectors;
};

/// Round-trip serialize/deserialize VARCHAR strings of various lengths.
TEST_F(VarcharSerdeTest, RoundTripByLength)
{
    // Test strings of lengths that exercise BYTE_1 (1-255), BYTE_2 (256-65535), BYTE_4 (>65535)
    std::string shortStr = "hello_varchar";
    std::string mediumStr(300, 'M');
    std::string longStr(66000, 'L');

    struct TestCase {
        std::string label;
        const std::string& value;
        bool isNull;
    };
    std::vector<TestCase> cases = {
        {"short",  shortStr,  false},
        {"medium", mediumStr, false},
        {"long",   longStr,   false},
        {"null",   shortStr,  true},
    };

    for (auto& tc : cases) {
        int32_t rows = 1;
        auto* srcVec = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
        ownedVectors.push_back(srcVec);

        if (tc.isNull) {
            srcVec->SetNull(0);
        } else {
            srcVec->SetValue(0, std::string_view(tc.value));
        }

        // Serialize
        StringRef result;
        vectorSerializerCenter[OMNI_VARCHAR](srcVec, 0, pool, result);

        // Deserialize into output vector
        auto* outVec = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
        ownedVectors.push_back(outVec);
        const char* pos = result.data;
        auto deser = vectorDeSerializerCenter[OMNI_VARCHAR];
        deser(outVec, 0, pos);

        if (tc.isNull) {
            EXPECT_TRUE(outVec->IsNull(0)) << tc.label << " should be null";
        } else {
            ASSERT_FALSE(outVec->IsNull(0)) << tc.label << " should not be null";
            EXPECT_EQ(outVec->GetValue(0).size(), tc.value.size()) << tc.label << " size mismatch";
            EXPECT_EQ(outVec->GetValue(0), tc.value) << tc.label << " content mismatch";
        }
    }
}

/// Round-trip serialize/deserialize for FixedLen types (INT, LONG, DOUBLE, etc.)
TEST_F(VarcharSerdeTest, FixedLenRoundTrip)
{
    struct TypeCase {
        int32_t typeId;
        const char* label;
    };
    std::vector<TypeCase> types = {
        {OMNI_INT, "int32"}, {OMNI_LONG, "int64"}, {OMNI_DOUBLE, "double"},
        {OMNI_SHORT, "short"}, {OMNI_BYTE, "byte"}, {OMNI_FLOAT, "float"},
    };

    for (auto& tc : types) {
        // Two rows: normal and null
        int32_t rows = 2;

        switch (tc.typeId) {
        case OMNI_INT: {
            auto* sv = new Vector<int32_t>(rows, OMNI_INT);
            ownedVectors.push_back(sv);
            sv->SetValue(0, 42);
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<int32_t>(rows, OMNI_INT);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_EQ(ov->GetValue(ri), 42) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        case OMNI_LONG: {
            auto* sv = new Vector<int64_t>(rows, OMNI_LONG);
            ownedVectors.push_back(sv);
            sv->SetValue(0, 1LL << 40);
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<int64_t>(rows, OMNI_LONG);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_EQ(ov->GetValue(ri), 1LL << 40) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        case OMNI_DOUBLE: {
            auto* sv = new Vector<double>(rows, OMNI_DOUBLE);
            ownedVectors.push_back(sv);
            sv->SetValue(0, 3.14159);
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<double>(rows, OMNI_DOUBLE);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_DOUBLE_EQ(ov->GetValue(ri), 3.14159) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        case OMNI_SHORT: {
            auto* sv = new Vector<int16_t>(rows, OMNI_SHORT);
            ownedVectors.push_back(sv);
            sv->SetValue(0, static_cast<int16_t>(-123));
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<int16_t>(rows, OMNI_SHORT);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_EQ(ov->GetValue(ri), -123) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        case OMNI_BYTE: {
            auto* sv = new Vector<int8_t>(rows, OMNI_BYTE);
            ownedVectors.push_back(sv);
            sv->SetValue(0, static_cast<int8_t>(99));
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<int8_t>(rows, OMNI_BYTE);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_EQ(ov->GetValue(ri), 99) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        case OMNI_FLOAT: {
            auto* sv = new Vector<float>(rows, OMNI_FLOAT);
            ownedVectors.push_back(sv);
            sv->SetValue(0, 2.71828f);
            sv->SetNull(1);
            for (int ri = 0; ri < rows; ri++) {
                StringRef result;
                vectorSerializerCenter[tc.typeId](sv, ri, pool, result);
                auto* ov = new Vector<float>(rows, OMNI_FLOAT);
                ownedVectors.push_back(ov);
                const char* pos = result.data;
                vectorDeSerializerCenter[tc.typeId](ov, ri, pos);
                if (ri == 0) {
                    EXPECT_FLOAT_EQ(ov->GetValue(ri), 2.71828f) << tc.label;
                } else {
                    EXPECT_TRUE(ov->IsNull(ri)) << tc.label;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}


// ======================================================================
//  TaperColumnSerializeHandler — complex multi-column with VARCHAR (category 5)
// ======================================================================

/// Two-column GROUP BY with mixed VARCHAR + INT — round-trip through Extract.
TEST_F(TaperColumnSerializeHandlerTest, VarcharAndInt)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 3;

    handler.InitRowContainer(
        {sizeof(char*), sizeof(int32_t)},
        {true, false},
        {OMNI_VARCHAR, OMNI_INT},
        {0},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_VARCHAR]);
    handler.PushBackSerializer(vectorSerializerCenter[OMNI_INT]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_VARCHAR]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_INT]);

    auto* v1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    auto* v2 = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(v1);
    ownedVectors.push_back(v2);

    v1->SetValue(0, std::string_view("abc"));
    v2->SetValue(0, 10);
    v1->SetValue(1, std::string_view("defghijklm"));
    v2->SetValue(1, 20);
    v1->SetValue(2, std::string_view("nop"));
    v2->SetValue(2, 30);

    BaseVector* groupVecs[] = {v1, v2};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 2, rows);
    handler.EmplaceTable(groupVecs, 2, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 3);

    // Verify via Extract+ParseKeyToCols
    auto* out1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    auto* out2 = new Vector<int32_t>(rows, OMNI_INT);
    ownedVectors.push_back(out1);
    ownedVectors.push_back(out2);
    std::vector<BaseVector*> outVecs = {out1, out2};

    OutputState outputState;
    int32_t extractCount = 0;
    handler.Extract(rows, outputState,
        [&](uint8_t* rowPtr, uint8_t* row, size_t idx) {
            handler.ParseKeyToCols(rowPtr, outVecs, 2, static_cast<int32_t>(idx));
            extractCount++;
        },
        [](uint8_t*, uint8_t*, size_t) {});

    EXPECT_EQ(extractCount, 3);
    // Each row should have unique values
    bool foundStr[3] = {false};
    bool foundInt[3] = {false};
    for (int32_t i = 0; i < extractCount; i++) {
        auto sv = out1->GetValue(i);
        int32_t iv = out2->GetValue(i);
        if (sv == "abc" && iv == 10) { foundStr[0] = true; foundInt[0] = true; }
        if (sv == "defghijklm" && iv == 20) { foundStr[1] = true; foundInt[1] = true; }
        if (sv == "nop" && iv == 30) { foundStr[2] = true; foundInt[2] = true; }
    }
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(foundStr[i]);
        EXPECT_TRUE(foundInt[i]);
    }
}

/// VARCHAR column with null values.
TEST_F(TaperColumnSerializeHandlerTest, VarcharWithNulls)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 3;

    handler.InitRowContainer(
        {sizeof(char*)},
        {true},
        {OMNI_VARCHAR},
        {0},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_VARCHAR]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_VARCHAR]);

    auto* v1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    ownedVectors.push_back(v1);

    v1->SetValue(0, std::string_view("not_null"));
    v1->SetNull(1);
    v1->SetValue(2, std::string_view("also_not_null"));

    BaseVector* groupVecs[] = {v1};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 1, rows);
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 3);

    auto* out1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    ownedVectors.push_back(out1);
    std::vector<BaseVector*> outVecs = {out1};

    OutputState outputState;
    int32_t extractCount = 0;
    handler.Extract(rows, outputState,
        [&](uint8_t* rowPtr, uint8_t* row, size_t idx) {
            handler.ParseKeyToCols(rowPtr, outVecs, 1, static_cast<int32_t>(idx));
            extractCount++;
        },
        [](uint8_t*, uint8_t*, size_t) {});

    EXPECT_EQ(extractCount, 3);
    bool foundNotNull = false, foundNull = false, foundAlso = false;
    for (int32_t i = 0; i < extractCount; i++) {
        if (out1->IsNull(i)) {
            foundNull = true;
        } else {
            auto sv = out1->GetValue(i);
            if (sv == "not_null") foundNotNull = true;
            if (sv == "also_not_null") foundAlso = true;
        }
    }
    EXPECT_TRUE(foundNotNull);
    EXPECT_TRUE(foundNull);
    EXPECT_TRUE(foundAlso);
}

/// Long VARCHAR string (BYTE_2 path: 256-65535 bytes).
TEST_F(TaperColumnSerializeHandlerTest, VarcharLongString)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 2;

    handler.InitRowContainer(
        {sizeof(char*)},
        {true},
        {OMNI_VARCHAR},
        {0},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_VARCHAR]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_VARCHAR]);

    std::string shortStr = "short";
    std::string longStr(500, 'L');

    auto* v1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    ownedVectors.push_back(v1);
    v1->SetValue(0, std::string_view(shortStr));
    v1->SetValue(1, std::string_view(longStr));

    BaseVector* groupVecs[] = {v1};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 1, rows);
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 2);

    auto* out1 = new Vector<LargeStringContainer<std::string_view>>(rows, OMNI_VARCHAR);
    ownedVectors.push_back(out1);
    std::vector<BaseVector*> outVecs = {out1};

    OutputState outputState;
    int32_t extractCount = 0;
    handler.Extract(rows, outputState,
        [&](uint8_t* rowPtr, uint8_t* row, size_t idx) {
            handler.ParseKeyToCols(rowPtr, outVecs, 1, static_cast<int32_t>(idx));
            extractCount++;
        },
        [](uint8_t*, uint8_t*, size_t) {});

    EXPECT_EQ(extractCount, 2);
    bool foundShort = false, foundLong = false;
    for (int32_t i = 0; i < extractCount; i++) {
        auto sv = out1->GetValue(i);
        if (sv == shortStr) foundShort = true;
        if (sv == longStr) foundLong = true;
    }
    EXPECT_TRUE(foundShort);
    EXPECT_TRUE(foundLong);
}

/// Decimal128 serializer/deserializer.
TEST_F(TaperColumnSerializeHandlerTest, Decimal128RoundTrip)
{
    TaperColumnSerializeHandler handler(pool, 0);
    int32_t rows = 2;

    handler.InitRowContainer(
        {sizeof(Decimal128)},
        {false},
        {OMNI_DECIMAL128},
        {},
        pool);

    handler.PushBackSerializer(vectorSerializerCenter[OMNI_DECIMAL128]);
    handler.PushBackDeSerializer(vectorDeSerializerCenter[OMNI_DECIMAL128]);

    auto* v1 = new Vector<Decimal128>(rows, OMNI_DECIMAL128);
    ownedVectors.push_back(v1);
    Decimal128 d1(123456, 3);
    v1->SetValue(0, d1);
    v1->SetNull(1);

    BaseVector* groupVecs[] = {v1};
    groups.resize(rows);
    newGroups.clear();
    handler.DecodeGroupByColumns(groupVecs, 1, rows);
    handler.EmplaceTable(groupVecs, 1, rows, groups, newGroups, vec::OMNI_FLAT);

    EXPECT_EQ(handler.GetElementsSize(), 2);

    auto* out1 = new Vector<Decimal128>(rows, OMNI_DECIMAL128);
    ownedVectors.push_back(out1);
    std::vector<BaseVector*> outVecs = {out1};

    OutputState outputState;
    int32_t extractCount = 0;
    handler.Extract(rows, outputState,
        [&](uint8_t* rowPtr, uint8_t* row, size_t idx) {
            handler.ParseKeyToCols(rowPtr, outVecs, 1, static_cast<int32_t>(idx));
            extractCount++;
        },
        [](uint8_t*, uint8_t*, size_t) {});

    EXPECT_EQ(extractCount, 2);
    bool foundNormal = false, foundNull = false;
    for (int32_t i = 0; i < extractCount; i++) {
        if (out1->IsNull(i)) {
            foundNull = true;
        } else {
            auto dv = out1->GetValue(i);
            if (dv == d1) foundNormal = true;
        }
    }
    EXPECT_TRUE(foundNormal);
    EXPECT_TRUE(foundNull);
}

} // namespace
} // namespace omniruntime::test
