/**
 * T0 end-to-end: same ORC + same predicate JSON, compare filterWhileDecode on vs off.
 *
 * Resource: orc_data_all_type
 *   c1 int: 10,20,30,40,50
 *   c4 bigint: 10000,20000,NULL,40000,50000
 *   c11 smallint: 11,12,13,14,15
 *   c13 date: 2021-12-01 .. 2021-12-05
 */

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include <stdexcept>

#include "scan_test.h"
#include "codegen/Options.h"
#include "reader/Reader.h"
#include "reader/ReaderFactory.h"
#include "reader/ReaderOptions.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/UriInfo.h"
#include "reader/orc/OrcFileOverride.hh"
#include "type/data_type.h"
#include "util/type_util.h"
#include "vector/vector_common.h"

using omniruntime::type::Date32Type;
using omniruntime::type::IntType;
using omniruntime::type::LongType;
using omniruntime::type::OMNI_DATE32;
using omniruntime::type::OMNI_INT;
using omniruntime::type::OMNI_LONG;
using omniruntime::type::OMNI_SHORT;
using omniruntime::type::ROW;
using omniruntime::type::ShortType;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using ::common::PredicateOperatorType;

namespace {

struct IntRow {
    bool isNull = false;
    int64_t value = 0;
};

struct ScanResult {
    std::vector<std::vector<IntRow>> columns; // columns[col][row]
    uint64_t totalRows = 0;
};

nlohmann::json Leaf(PredicateOperatorType op, int index, int dataType, const std::string &value = "0")
{
    nlohmann::json j;
    j["op"] = static_cast<int>(op);
    j["index"] = index;
    j["dataType"] = dataType;
    j["value"] = value;
    return j;
}

nlohmann::json And(nlohmann::json left, nlohmann::json right)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::AND);
    j["left"] = std::move(left);
    j["right"] = std::move(right);
    return j;
}

nlohmann::json Or(nlohmann::json left, nlohmann::json right)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::OR);
    j["left"] = std::move(left);
    j["right"] = std::move(right);
    return j;
}

nlohmann::json Not(nlohmann::json child)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::NOT);
    j["child"] = std::move(child);
    return j;
}

std::string BuildEnhanceJson(bool filterWhileDecode, const nlohmann::json &cond, const std::string &includedColumns)
{
    nlohmann::json root;
    root["filterWhileDecode"] = filterWhileDecode;
    root["allColumns"] = "c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13";
    root["includedColumns"] = includedColumns;
    root["vecPredicateCondition"] = cond.dump();
    return root.dump();
}

void ClearBatch(std::vector<BaseVector *> *batch)
{
    if (batch == nullptr) {
        return;
    }
    for (auto *v : *batch) {
        delete v;
    }
    delete batch;
}

IntRow ReadIntCell(BaseVector *v, int32_t row, int omniTypeId)
{
    IntRow cell;
    if (v->IsNull(row)) {
        cell.isNull = true;
        return cell;
    }
    switch (static_cast<omniruntime::type::DataTypeId>(omniTypeId)) {
        case OMNI_INT:
            cell.value = static_cast<Vector<int32_t> *>(v)->GetValue(row);
            break;
        case OMNI_LONG:
            cell.value = static_cast<Vector<int64_t> *>(v)->GetValue(row);
            break;
        case OMNI_SHORT:
            cell.value = static_cast<Vector<int16_t> *>(v)->GetValue(row);
            break;
        case OMNI_DATE32:
            cell.value = static_cast<Vector<int32_t> *>(v)->GetValue(row);
            break;
        default:
            throw std::runtime_error("unexpected type in FWD scan test");
    }
    return cell;
}

} // namespace

class FilterWhileDecodeScanTest : public testing::Test {
protected:
    ScanResult RunScan(bool filterWhileDecode, const nlohmann::json &cond, uint64_t batchLen,
                       const std::string &includedColumns, const omniruntime::type::RowTypePtr &rowType,
                       const std::vector<int> &omniTypeIds)
    {
        auto readerOpts = std::make_shared<omniruntime::reader::ReaderOptions>();
        // Use FileFormat overload so allColumns/includedColumns parse (gate + predicate col indices need them)
        readerOpts->ParseEnhanceJson(BuildEnhanceJson(filterWhileDecode, cond, includedColumns),
                                     omniruntime::codegen::FileFormat::ORC);
        readerOpts->SetBatchLen(static_cast<int32_t>(batchLen));

        auto orcReaderOptions = std::make_shared<::orc::ReaderOptions>();
        ::orc::MemoryPool *pool = ::orc::getDefaultPool();
        orcReaderOptions->setMemoryPool(*pool);
        orcReaderOptions->setTailLocation(std::numeric_limits<uint64_t>::max());
        orcReaderOptions->setSerializedFileTail("");
        readerOpts->SetOrcReaderOptions(std::move(orcReaderOptions));
        readerOpts->SetOrcRowReaderOptions(std::make_shared<::orc::RowReaderOptions>());

        std::string filename = PROJECT_PATH + std::string("/../resources/orc_data_all_type");
        readerOpts->SetUri(std::make_shared<UriInfo>("file", filename, "", "-1"));

        readerOpts->SetRowType(rowType);
        readerOpts->SetFileRowType(rowType);

        auto reader =
            omniruntime::reader::GetReaderFactory(omniruntime::codegen::FileFormat::ORC)->CreateReader(readerOpts);
        auto included = readerOpts->GetIncludedColumnsList();
        EXPECT_FALSE(included.empty()) << "includedColumns mapping failed; check allColumns/includedColumns";
        if (included.empty()) {
            return {};
        }
        readerOpts->GetOrcRowReaderOptions().include(included);

        auto rowReader = reader->CreateRowReader();
        const int nCols = static_cast<int>(omniTypeIds.size());
        std::vector<int> typeIds = omniTypeIds;

        ScanResult result;
        result.columns.resize(static_cast<size_t>(nCols));
        std::vector<BaseVector *> *batch = nullptr;
        uint64_t n = rowReader->Next(&batch, typeIds.data(), batchLen);
        while (n > 0) {
            EXPECT_NE(batch, nullptr);
            EXPECT_EQ(static_cast<int>(batch->size()), nCols);
            if (batch == nullptr || static_cast<int>(batch->size()) != nCols) {
                ClearBatch(batch);
                batch = nullptr;
                break;
            }
            result.totalRows += n;
            for (int c = 0; c < nCols; ++c) {
                for (uint64_t r = 0; r < n; ++r) {
                    result.columns[c].push_back(ReadIntCell((*batch)[c], static_cast<int32_t>(r), typeIds[c]));
                }
            }
            ClearBatch(batch);
            batch = nullptr;
            n = rowReader->Next(&batch, typeIds.data(), batchLen);
        }
        ClearBatch(batch);
        return result;
    }

    // Default project c1/c4/c11 (int/bigint/smallint); covers multi int-family projection cols.
    ScanResult RunScan(bool filterWhileDecode, const nlohmann::json &cond, uint64_t batchLen)
    {
        auto rowType = ROW({"c1", "c4", "c11"}, {IntType(), LongType(), ShortType()});
        return RunScan(filterWhileDecode, cond, batchLen, "c1,c4,c11", rowType, {OMNI_INT, OMNI_LONG, OMNI_SHORT});
    }

    void ExpectEqual(const ScanResult &off, const ScanResult &on)
    {
        ASSERT_EQ(off.totalRows, on.totalRows);
        ASSERT_EQ(off.columns.size(), on.columns.size());
        for (size_t c = 0; c < off.columns.size(); ++c) {
            ASSERT_EQ(off.columns[c].size(), on.columns[c].size()) << "col " << c;
            for (size_t r = 0; r < off.columns[c].size(); ++r) {
                EXPECT_EQ(off.columns[c][r].isNull, on.columns[c][r].isNull) << "col " << c << " row " << r;
                if (!off.columns[c][r].isNull) {
                    EXPECT_EQ(off.columns[c][r].value, on.columns[c][r].value) << "col " << c << " row " << r;
                }
            }
        }
    }
};

TEST_F(FilterWhileDecodeScanTest, RangePredicateOnOffMatch)
{
    // c1 > 25 → rows 30,40,50
    auto cond = Leaf(::common::GREATER_THAN, 0, OMNI_INT, "25");
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 3u);
    ASSERT_GE(on.columns[0].size(), 3u);
    EXPECT_EQ(on.columns[0][0].value, 30);
    EXPECT_EQ(on.columns[0][1].value, 40);
    EXPECT_EQ(on.columns[0][2].value, 50);
}

TEST_F(FilterWhileDecodeScanTest, EqualPredicateOnOffMatch)
{
    // c1 = 30 → 1 row; also check multi-projection c1/c4/c11 alignment
    auto cond = Leaf(::common::EQUAL_TO, 0, OMNI_INT, "30");
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 1u);
    ASSERT_EQ(on.columns[0].size(), 1u);
    EXPECT_EQ(on.columns[0][0].value, 30);
    EXPECT_TRUE(on.columns[1][0].isNull); // c4 of c1=30 is NULL
    EXPECT_EQ(on.columns[2][0].value, 13);
}

TEST_F(FilterWhileDecodeScanTest, NegatedNotEqualOnOffMatch)
{
    // NOT (c1 = 30) → 10,20,40,50
    auto cond = Not(Leaf(::common::EQUAL_TO, 0, OMNI_INT, "30"));
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 4u);
    ASSERT_EQ(on.columns[0].size(), 4u);
    EXPECT_EQ(on.columns[0][0].value, 10);
    EXPECT_EQ(on.columns[0][1].value, 20);
    EXPECT_EQ(on.columns[0][2].value, 40);
    EXPECT_EQ(on.columns[0][3].value, 50);
}

TEST_F(FilterWhileDecodeScanTest, EqualAndNullAwareOnOffMatch)
{
    // c1 > 15 AND c4 IS NOT NULL → 20,40,50 (c4 is null for c1=30)
    auto cond = And(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "15"), Leaf(::common::IS_NOT_NULL, 1, OMNI_LONG));
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 3u);
    for (auto &cell : on.columns[1]) {
        EXPECT_FALSE(cell.isNull);
    }
}

TEST_F(FilterWhileDecodeScanTest, SingleColumnOrOnOffMatch)
{
    // c1 < 15 OR c1 > 45 → 10,50
    auto cond = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "15"), Leaf(::common::GREATER_THAN, 0, OMNI_INT, "45"));
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 2u);
    ASSERT_GE(on.columns[0].size(), 2u);
    EXPECT_EQ(on.columns[0][0].value, 10);
    EXPECT_EQ(on.columns[0][1].value, 50);
}

TEST_F(FilterWhileDecodeScanTest, ResidualCrossColumnOrOnOffMatch)
{
    // c1 >= 0 AND (c1 < 25 OR c11 > 13)
    // All c1 >= 0; cross-column OR → residual. Expect: 10,20 + 40,50 (c11=14,15) → 10,20,40,50; c1=30 has c11=13 rejected
    auto cross = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "25"), Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "13"));
    auto cond = And(Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_INT, "0"), cross);
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 4u);
}

TEST_F(FilterWhileDecodeScanTest, DateProjectionWithIntFilterOnOffMatch)
{
    // Projection includes DATE32 (c13), filter col still int: covers multi-project + date on selective path
    auto rowType = ROW({"c1", "c13"}, {IntType(), Date32Type()});
    auto cond = Leaf(::common::GREATER_THAN, 0, OMNI_INT, "25");
    auto off = RunScan(false, cond, 4096, "c1,c13", rowType, {OMNI_INT, OMNI_DATE32});
    auto on = RunScan(true, cond, 4096, "c1,c13", rowType, {OMNI_INT, OMNI_DATE32});
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 3u);
    ASSERT_EQ(on.columns[0].size(), 3u);
    EXPECT_EQ(on.columns[0][0].value, 30);
    EXPECT_EQ(on.columns[0][1].value, 40);
    EXPECT_EQ(on.columns[0][2].value, 50);
    // Date projection non-null; three values should increase (2021-12-03/04/05)
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_FALSE(on.columns[1][i].isNull);
    }
    EXPECT_LT(on.columns[1][0].value, on.columns[1][1].value);
    EXPECT_LT(on.columns[1][1].value, on.columns[1][2].value);
}

TEST_F(FilterWhileDecodeScanTest, DateIsNotNullFilterOnOffMatch)
{
    // DATE32 as filter col (IS NOT NULL); resource c13 all non-null → all 5 rows
    auto rowType = ROW({"c1", "c13"}, {IntType(), Date32Type()});
    auto cond = Leaf(::common::IS_NOT_NULL, 1, OMNI_DATE32);
    auto off = RunScan(false, cond, 4096, "c1,c13", rowType, {OMNI_INT, OMNI_DATE32});
    auto on = RunScan(true, cond, 4096, "c1,c13", rowType, {OMNI_INT, OMNI_DATE32});
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 5u);
}

TEST_F(FilterWhileDecodeScanTest, SmallBatchMultiNextOnOffMatch)
{
    // Small batch forces multiple Next; covers cross-batch alignment (incl. null col c4)
    auto cond = And(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "15"), Leaf(::common::IS_NOT_NULL, 1, OMNI_LONG));
    auto off = RunScan(false, cond, 2);
    auto on = RunScan(true, cond, 2);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 3u);
}

TEST_F(FilterWhileDecodeScanTest, EmptyResultOnOffMatch)
{
    // Same-column contradictory ranges → AlwaysFalse / empty result
    auto cond = And(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "40"), Leaf(::common::LESS_THAN, 0, OMNI_INT, "20"));
    auto off = RunScan(false, cond, 4096);
    auto on = RunScan(true, cond, 4096);
    ExpectEqual(off, on);
    EXPECT_EQ(on.totalRows, 0u);
}
