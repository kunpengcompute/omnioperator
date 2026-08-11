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
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

#include <stdexcept>

#include "scan_test.h"
#include "codegen/Options.h"
#include "reader/Reader.h"
#include "reader/ReaderFactory.h"
#include "reader/ReaderOptions.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/UriInfo.h"
#include "reader/orc/OmniWriter.hh"
#include "reader/orc/OrcFileOverride.hh"
#include "type/data_type.h"
#include "util/type_util.h"
#include "vector/vector_common.h"

using omniruntime::type::Date32Type;
using omniruntime::type::ByteType;
using omniruntime::type::BooleanType;
using omniruntime::type::CharType;
using omniruntime::type::Decimal128;
using omniruntime::type::Decimal128Type;
using omniruntime::type::Decimal64Type;
using omniruntime::type::DoubleType;
using omniruntime::type::FloatType;
using omniruntime::type::IntType;
using omniruntime::type::LongType;
using omniruntime::type::OMNI_DATE32;
using omniruntime::type::OMNI_INT;
using omniruntime::type::OMNI_LONG;
using omniruntime::type::OMNI_SHORT;
using omniruntime::type::ROW;
using omniruntime::type::ShortType;
using omniruntime::type::TimestampType;
using omniruntime::type::VarBinaryType;
using omniruntime::type::VarcharType;
using omniruntime::vec::BaseVector;
using omniruntime::vec::LargeStringContainer;
using omniruntime::vec::RowVector;
using omniruntime::vec::Vector;
using omniruntime::writer::createOmniWriter;
using OmniStringVector = Vector<LargeStringContainer<std::string_view>>;
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

struct ScalarRow {
    bool isNull = false;
    std::string rawValue;
};

struct ScalarScanResult {
    std::vector<std::vector<ScalarRow>> columns;
    uint64_t totalRows = 0;
};

template <typename T>
std::string RawBytes(T value)
{
    return std::string(reinterpret_cast<const char *>(&value), sizeof(T));
}

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

std::string BuildEnhanceJson(bool filterWhileDecode, const nlohmann::json &cond, const std::string &includedColumns,
                             const std::string &allColumns = "c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13")
{
    nlohmann::json root;
    root["filterWhileDecode"] = filterWhileDecode;
    root["allColumns"] = allColumns;
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

ScalarRow ReadScalarCell(BaseVector *v, int32_t row, int omniTypeId)
{
    ScalarRow cell;
    if (v->IsNull(row)) {
        cell.isNull = true;
        return cell;
    }
    switch (static_cast<omniruntime::type::DataTypeId>(omniTypeId)) {
        case OMNI_INT:
            cell.rawValue = RawBytes(static_cast<Vector<int32_t> *>(v)->GetValue(row));
            break;
        case OMNI_SHORT:
            cell.rawValue = RawBytes(static_cast<Vector<int16_t> *>(v)->GetValue(row));
            break;
        case OMNI_LONG:
            cell.rawValue = RawBytes(static_cast<Vector<int64_t> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_FLOAT:
            cell.rawValue = RawBytes(static_cast<Vector<float> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_DOUBLE:
            cell.rawValue = RawBytes(static_cast<Vector<double> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_DECIMAL64:
        case omniruntime::type::OMNI_TIMESTAMP:
            cell.rawValue = RawBytes(static_cast<Vector<int64_t> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_BOOLEAN:
            cell.rawValue = RawBytes(static_cast<Vector<bool> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_BYTE:
            cell.rawValue = RawBytes(static_cast<Vector<int8_t> *>(v)->GetValue(row));
            break;
        case omniruntime::type::OMNI_DECIMAL128:
            cell.rawValue = static_cast<Vector<Decimal128> *>(v)->GetValue(row).ToString();
            break;
        case omniruntime::type::OMNI_VARBINARY: {
            const auto value = static_cast<OmniStringVector *>(v)->GetValue(row);
            cell.rawValue.assign(value.data(), value.size());
            break;
        }
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR: {
            const auto value = static_cast<OmniStringVector *>(v)->GetValue(row);
            cell.rawValue.assign(value.data(), value.size());
            break;
        }
        default:
            throw std::runtime_error("unexpected type in scalar FWD scan test");
    }
    return cell;
}

class PrimitiveOrcFixture {
public:
    explicit PrimitiveOrcFixture(const std::string &testName)
        : filename_("/tmp/omni_scan_primitive_" + std::to_string(getpid()) + "_" + testName + ".orc")
    {
        constexpr int32_t numRows = 8;
        const std::vector<int32_t> ids = {0, 1, 2, 3, 4, 5, 6, 7};
        const std::vector<int8_t> bytes = {-128, -7, -1, 0, 1, 7, 42, 127};
        const std::vector<bool> booleans = {false, false, true, true, false, true, false, true};
        const std::vector<double> doubles = {-4.0, -2.0, -1.0, 0.0, 0.5, 1.0, 2.0, 4.0};
        const std::vector<float> floats = {-4.5F, -2.5F, -1.5F, 0.0F, 0.5F, 1.5F, 2.5F, 4.5F};
        const std::vector<int64_t> decimals64 = {
            -123456789012345678L, -10000L, -1L, 0L, 1L, 10000L, 123456789012345677L, 123456789012345678L};
        const std::vector<Decimal128> decimals = {
            Decimal128("-99999999999999999999999999999999999999"),
            Decimal128("-123456789012345678901234567890"),
            Decimal128("-1"),
            Decimal128("0"),
            Decimal128("1"),
            Decimal128("123456789012345678901234567890"),
            Decimal128("99999999999999999999999999999999999998"),
            Decimal128("99999999999999999999999999999999999999")};
        const std::vector<std::string> binaries = {
            "", "text", std::string("a\0b", 3), std::string("\x00\x01\x02\xff", 4),
            "payload-4", "payload-5", "payload-6", "payload-7"};
        const std::vector<int64_t> timestamps = {
            1609459200000L, 1609459201000L, 1609459202000L, 1609459203000L,
            1609459204000L, 1609459205000L, 1609459206000L, 1609459207000L};

        UriInfo uri("file", filename_, "", "-1");
        auto outStream = omniruntime::reader::writeFileOverride(uri);
        auto schema = ::orc::createPrimitiveType(::orc::TypeKind::STRUCT);
        schema->addStructField("id", ::orc::createPrimitiveType(::orc::TypeKind::INT));
        schema->addStructField("c_byte", ::orc::createPrimitiveType(::orc::TypeKind::BYTE));
        schema->addStructField("c_bool", ::orc::createPrimitiveType(::orc::TypeKind::BOOLEAN));
        schema->addStructField("c_double", ::orc::createPrimitiveType(::orc::TypeKind::DOUBLE));
        schema->addStructField("c_float", ::orc::createPrimitiveType(::orc::TypeKind::FLOAT));
        schema->addStructField("c_decimal64", ::orc::createDecimalType(18, 4));
        schema->addStructField("c_decimal128", ::orc::createDecimalType(38, 4));
        schema->addStructField("c_binary", ::orc::createPrimitiveType(::orc::TypeKind::BINARY));
        schema->addStructField("c_timestamp", ::orc::createPrimitiveType(::orc::TypeKind::TIMESTAMP));

        ::orc::WriterOptions options;
        options.setMemoryPool(::orc::getDefaultPool());
        options.setStripeSize(67108864);
        options.setTimezoneName("GMT");
        options.setDictionaryKeySizeThreshold(0.0);
        auto writer = createOmniWriter(*schema, outStream.get(), options);

        auto idVector = std::make_unique<Vector<int32_t>>(numRows);
        auto byteVector = std::make_unique<Vector<int8_t>>(numRows);
        auto boolVector = std::make_unique<Vector<bool>>(numRows);
        auto doubleVector = std::make_unique<Vector<double>>(numRows);
        auto floatVector = std::make_unique<Vector<float>>(numRows);
        auto decimal64Vector = std::make_unique<Vector<int64_t>>(numRows);
        auto decimalVector = std::make_unique<Vector<Decimal128>>(numRows);
        auto binaryVector = std::make_unique<OmniStringVector>(numRows);
        auto timestampVector = std::make_unique<Vector<int64_t>>(numRows);
        for (int32_t row = 0; row < numRows; ++row) {
            idVector->SetValue(row, ids[row]);
            byteVector->SetValue(row, bytes[row]);
            boolVector->SetValue(row, booleans[row]);
            doubleVector->SetValue(row, doubles[row]);
            floatVector->SetValue(row, floats[row]);
            decimal64Vector->SetValue(row, decimals64[row]);
            decimalVector->SetValue(row, decimals[row]);
            binaryVector->SetValue(row, std::string_view(binaries[row]));
            timestampVector->SetValue(row, timestamps[row]);
            idVector->SetNotNull(row);
            byteVector->SetNotNull(row);
            boolVector->SetNotNull(row);
            doubleVector->SetNotNull(row);
            floatVector->SetNotNull(row);
            decimal64Vector->SetNotNull(row);
            decimalVector->SetNotNull(row);
            binaryVector->SetNotNull(row);
            timestampVector->SetNotNull(row);
        }
        byteVector->SetNull(1);
        boolVector->SetNull(2);
        doubleVector->SetNull(3);
        floatVector->SetNull(4);
        decimal64Vector->SetNull(5);
        decimalVector->SetNull(5);
        binaryVector->SetNull(6);
        timestampVector->SetNull(7);

        std::vector<BaseVector *> columns = {
            idVector.get(), byteVector.get(), boolVector.get(), doubleVector.get(),
            floatVector.get(), decimal64Vector.get(), decimalVector.get(), binaryVector.get(), timestampVector.get()};
        auto rowVector = std::make_unique<RowVector>(numRows, columns);
        for (int32_t row = 0; row < numRows; ++row) {
            rowVector->SetNotNull(row);
        }
        writer->add(rowVector.get(), 0, numRows);
        writer->close();
    }

    ~PrimitiveOrcFixture() { std::remove(filename_.c_str()); }

    const std::string &filename() const { return filename_; }

private:
    std::string filename_;
};

// Mirrors the nullable columns used by docs/scripts/setup_scan_filter_db.sql.
// The raw value in NULL slots is deliberately zero so the legacy evaluator's
// former "compare the value slot and ignore nulls" behavior is deterministic.
class NullablePredicateOrcFixture {
public:
    explicit NullablePredicateOrcFixture(const std::string &testName)
        : filename_("/tmp/omni_scan_nullable_predicate_" + std::to_string(getpid()) + "_" + testName + ".orc")
    {
        constexpr int32_t numRows = 8;
        const std::vector<int64_t> ids = {1, 2, 3, 4, 5, 6, 0, 100};
        const std::vector<int32_t> ints = {1, 2, 3, 6, 11, 0, 1, -5};
        const std::vector<int16_t> shorts = {1, 12, 3, 20, -1, 0, 1, 15};
        const std::vector<std::string> strings = {"x", "alpha", "", "", "text", "", "x", "omega"};
        const std::vector<std::string> chars = {"x", "alpha", "x", "", "text", "", "x", "omega"};

        UriInfo uri("file", filename_, "", "-1");
        auto outStream = omniruntime::reader::writeFileOverride(uri);
        auto schema = ::orc::createPrimitiveType(::orc::TypeKind::STRUCT);
        schema->addStructField("id", ::orc::createPrimitiveType(::orc::TypeKind::LONG));
        schema->addStructField("c_int", ::orc::createPrimitiveType(::orc::TypeKind::INT));
        schema->addStructField("c_short", ::orc::createPrimitiveType(::orc::TypeKind::SHORT));
        schema->addStructField("c_string", ::orc::createPrimitiveType(::orc::TypeKind::STRING));
        schema->addStructField("c_char", ::orc::createCharType(::orc::TypeKind::CHAR, 16));

        ::orc::WriterOptions options;
        options.setMemoryPool(::orc::getDefaultPool());
        options.setStripeSize(67108864);
        options.setTimezoneName("GMT");
        options.setDictionaryKeySizeThreshold(0.0);
        auto writer = createOmniWriter(*schema, outStream.get(), options);

        auto idVector = std::make_unique<Vector<int64_t>>(numRows);
        auto intVector = std::make_unique<Vector<int32_t>>(numRows);
        auto shortVector = std::make_unique<Vector<int16_t>>(numRows);
        auto stringVector = std::make_unique<OmniStringVector>(numRows);
        auto charVector = std::make_unique<OmniStringVector>(numRows);
        for (int32_t row = 0; row < numRows; ++row) {
            idVector->SetValue(row, ids[row]);
            intVector->SetValue(row, ints[row]);
            shortVector->SetValue(row, shorts[row]);
            stringVector->SetValue(row, std::string_view(strings[row]));
            charVector->SetValue(row, std::string_view(chars[row]));
            idVector->SetNotNull(row);
            intVector->SetNotNull(row);
            shortVector->SetNotNull(row);
            stringVector->SetNotNull(row);
            charVector->SetNotNull(row);
        }
        idVector->SetNull(6);
        intVector->SetNull(5);
        shortVector->SetNull(5);
        stringVector->SetNull(2);
        stringVector->SetNull(5);
        charVector->SetNull(3);
        charVector->SetNull(5);

        std::vector<BaseVector *> columns = {
            idVector.get(), intVector.get(), shortVector.get(), stringVector.get(), charVector.get()};
        auto rowVector = std::make_unique<RowVector>(numRows, columns);
        for (int32_t row = 0; row < numRows; ++row) {
            rowVector->SetNotNull(row);
        }
        writer->add(rowVector.get(), 0, numRows);
        writer->close();
    }

    ~NullablePredicateOrcFixture() { std::remove(filename_.c_str()); }

    const std::string &filename() const { return filename_; }

private:
    std::string filename_;
};

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

    ScalarScanResult RunScalarScanFile(bool filterWhileDecode, const nlohmann::json &cond, uint64_t batchLen,
                                       const std::string &filename, const std::string &allColumns,
                                       const std::string &includedColumns,
                                       const omniruntime::type::RowTypePtr &rowType,
                                       std::vector<int> typeIds)
    {
        auto readerOpts = std::make_shared<omniruntime::reader::ReaderOptions>();
        readerOpts->ParseEnhanceJson(
            BuildEnhanceJson(filterWhileDecode, cond, includedColumns, allColumns),
            omniruntime::codegen::FileFormat::ORC);
        readerOpts->SetBatchLen(static_cast<int32_t>(batchLen));
        auto orcReaderOptions = std::make_shared<::orc::ReaderOptions>();
        orcReaderOptions->setMemoryPool(*::orc::getDefaultPool());
        orcReaderOptions->setTailLocation(std::numeric_limits<uint64_t>::max());
        orcReaderOptions->setSerializedFileTail("");
        readerOpts->SetOrcReaderOptions(std::move(orcReaderOptions));
        readerOpts->SetOrcRowReaderOptions(std::make_shared<::orc::RowReaderOptions>());
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

        ScalarScanResult result;
        result.columns.resize(typeIds.size());
        std::vector<BaseVector *> *batch = nullptr;
        uint64_t n = rowReader->Next(&batch, typeIds.data(), batchLen);
        while (n > 0) {
            EXPECT_NE(batch, nullptr);
            EXPECT_EQ(batch == nullptr ? 0 : batch->size(), typeIds.size());
            if (batch == nullptr || batch->size() != typeIds.size()) {
                ClearBatch(batch);
                break;
            }
            result.totalRows += n;
            for (size_t column = 0; column < typeIds.size(); ++column) {
                for (uint64_t row = 0; row < n; ++row) {
                    result.columns[column].push_back(
                        ReadScalarCell((*batch)[column], static_cast<int32_t>(row), typeIds[column]));
                }
            }
            ClearBatch(batch);
            batch = nullptr;
            n = rowReader->Next(&batch, typeIds.data(), batchLen);
        }
        ClearBatch(batch);
        return result;
    }

    ScalarScanResult RunScalarScan(bool filterWhileDecode, const nlohmann::json &cond, uint64_t batchLen)
    {
        auto rowType = ROW(
            {"c1", "c6", "c7", "c8", "c9", "c10", "c12"},
            {IntType(), FloatType(), DoubleType(), Decimal64Type(9, 8), Decimal64Type(18, 5), BooleanType(),
             TimestampType()});
        std::vector<int> typeIds = {
            OMNI_INT,
            omniruntime::type::OMNI_FLOAT,
            omniruntime::type::OMNI_DOUBLE,
            omniruntime::type::OMNI_DECIMAL64,
            omniruntime::type::OMNI_DECIMAL64,
            omniruntime::type::OMNI_BOOLEAN,
            omniruntime::type::OMNI_TIMESTAMP};
        return RunScalarScanFile(
            filterWhileDecode, cond, batchLen,
            PROJECT_PATH + std::string("/../resources/orc_data_all_type"),
            "c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13", "c1,c6,c7,c8,c9,c10,c12", rowType, typeIds);
    }

    ScalarScanResult RunGeneratedPrimitiveScan(bool filterWhileDecode, const nlohmann::json &cond,
                                               const std::string &filename, uint64_t batchLen = 3)
    {
        auto rowType = ROW(
            {"id", "c_byte", "c_bool", "c_double", "c_float", "c_decimal64", "c_decimal128", "c_binary",
             "c_timestamp"},
            {IntType(), ByteType(), BooleanType(), DoubleType(), FloatType(), Decimal64Type(18, 4),
             Decimal128Type(38, 4), VarBinaryType(), TimestampType()});
        std::vector<int> typeIds = {
            OMNI_INT,
            omniruntime::type::OMNI_BYTE,
            omniruntime::type::OMNI_BOOLEAN,
            omniruntime::type::OMNI_DOUBLE,
            omniruntime::type::OMNI_FLOAT,
            omniruntime::type::OMNI_DECIMAL64,
            omniruntime::type::OMNI_DECIMAL128,
            omniruntime::type::OMNI_VARBINARY,
            omniruntime::type::OMNI_TIMESTAMP};
        return RunScalarScanFile(
            filterWhileDecode, cond, batchLen, filename,
            "id,c_byte,c_bool,c_double,c_float,c_decimal64,c_decimal128,c_binary,c_timestamp",
            "id,c_byte,c_bool,c_double,c_float,c_decimal64,c_decimal128,c_binary,c_timestamp", rowType, typeIds);
    }

    ScalarScanResult RunNullablePredicateScan(bool filterWhileDecode, const nlohmann::json &cond,
                                              const std::string &filename, bool projectStrings = false,
                                              uint64_t batchLen = 3)
    {
        const std::string allColumns = "id,c_int,c_short,c_string,c_char";
        if (projectStrings) {
            auto rowType = ROW(
                {"id", "c_int", "c_short", "c_string", "c_char"},
                {LongType(), IntType(), ShortType(), VarcharType(), CharType(16)});
            return RunScalarScanFile(
                filterWhileDecode, cond, batchLen, filename, allColumns, allColumns, rowType,
                {OMNI_LONG, OMNI_INT, OMNI_SHORT, omniruntime::type::OMNI_VARCHAR,
                 omniruntime::type::OMNI_CHAR});
        }

        auto rowType = ROW({"id", "c_int", "c_short"}, {LongType(), IntType(), ShortType()});
        return RunScalarScanFile(
            filterWhileDecode, cond, batchLen, filename, allColumns, "id,c_int,c_short", rowType,
            {OMNI_LONG, OMNI_INT, OMNI_SHORT});
    }

    void ExpectScalarEqual(const ScalarScanResult &off, const ScalarScanResult &on)
    {
        ASSERT_EQ(off.totalRows, on.totalRows);
        ASSERT_EQ(off.columns.size(), on.columns.size());
        for (size_t column = 0; column < off.columns.size(); ++column) {
            ASSERT_EQ(off.columns[column].size(), on.columns[column].size()) << "column " << column;
            for (size_t row = 0; row < off.columns[column].size(); ++row) {
                EXPECT_EQ(off.columns[column][row].isNull, on.columns[column][row].isNull)
                    << "column " << column << " row " << row;
                EXPECT_EQ(off.columns[column][row].rawValue, on.columns[column][row].rawValue)
                    << "column " << column << " row " << row;
            }
        }
    }

    void ExpectScalarIds(const ScalarScanResult &result, const std::vector<int32_t> &expectedIds)
    {
        ASSERT_EQ(result.totalRows, expectedIds.size());
        ASSERT_FALSE(result.columns.empty());
        ASSERT_EQ(result.columns[0].size(), expectedIds.size());
        for (size_t row = 0; row < expectedIds.size(); ++row) {
            EXPECT_FALSE(result.columns[0][row].isNull) << "row " << row;
            EXPECT_EQ(result.columns[0][row].rawValue, RawBytes(expectedIds[row])) << "row " << row;
        }
    }

    void ExpectNullableLongIds(
        const ScalarScanResult &result, std::initializer_list<std::optional<int64_t>> expectedIds)
    {
        ASSERT_EQ(result.totalRows, expectedIds.size());
        ASSERT_FALSE(result.columns.empty());
        ASSERT_EQ(result.columns[0].size(), expectedIds.size());
        size_t row = 0;
        for (const auto &expectedId : expectedIds) {
            EXPECT_EQ(result.columns[0][row].isNull, !expectedId.has_value()) << "row " << row;
            if (expectedId.has_value()) {
                EXPECT_EQ(result.columns[0][row].rawValue, RawBytes(*expectedId)) << "row " << row;
            }
            ++row;
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

TEST_F(FilterWhileDecodeScanTest, OtherScalarProjectionOnOffMatch)
{
    // c1 > 25 leaves rows 30/40/50. The projected columns exercise FLOAT, DOUBLE,
    // DECIMAL64, BOOLEAN and TIMESTAMP, including nulls in c7/c10.
    auto cond = Leaf(::common::GREATER_THAN, 0, OMNI_INT, "25");
    auto off = RunScalarScan(false, cond, 2);
    auto on = RunScalarScan(true, cond, 2);

    ASSERT_EQ(off.totalRows, on.totalRows);
    ASSERT_EQ(off.columns.size(), on.columns.size());
    EXPECT_EQ(on.totalRows, 3u);
    for (size_t column = 0; column < off.columns.size(); ++column) {
        ASSERT_EQ(off.columns[column].size(), on.columns[column].size());
        for (size_t row = 0; row < off.columns[column].size(); ++row) {
            EXPECT_EQ(off.columns[column][row].isNull, on.columns[column][row].isNull);
            EXPECT_EQ(off.columns[column][row].rawValue, on.columns[column][row].rawValue);
        }
    }
}

TEST_F(FilterWhileDecodeScanTest, GeneratedPrimitiveProjectionOnOffMatch)
{
    PrimitiveOrcFixture fixture("projection_on_off");
    // Filter on a legacy-supported INT column while projecting every newly
    // supported primitive type. This verifies that selective decoding does not
    // require logical DataTypeId metadata in the underlying physical vectors.
    auto cond = Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_INT, "2");
    auto off = RunGeneratedPrimitiveScan(false, cond, fixture.filename(), 2);
    auto on = RunGeneratedPrimitiveScan(true, cond, fixture.filename(), 2);

    ExpectScalarEqual(off, on);
    ExpectScalarIds(on, {2, 3, 4, 5, 6, 7});
}

TEST_F(FilterWhileDecodeScanTest, PrimitiveValueFilters)
{
    PrimitiveOrcFixture fixture("value_filters");
    auto check = [&](const nlohmann::json &cond, const std::vector<int32_t> &expectedIds) {
        auto on = RunGeneratedPrimitiveScan(true, cond, fixture.filename());
        ExpectScalarIds(on, expectedIds);
    };

    // These predicates enter their type-specific selective readers rather than
    // merely projecting the corresponding columns.
    check(Leaf(::common::EQUAL_TO, 2, omniruntime::type::OMNI_BOOLEAN, "true"), {3, 5, 7});
    check(And(Leaf(::common::GREATER_THAN_OR_EQUAL, 1, omniruntime::type::OMNI_BYTE, "-7"),
              Leaf(::common::LESS_THAN_OR_EQUAL, 1, omniruntime::type::OMNI_BYTE, "7")),
          {2, 3, 4, 5});
    check(And(Leaf(::common::GREATER_THAN, 3, omniruntime::type::OMNI_DOUBLE, "-1.0"),
              Leaf(::common::LESS_THAN_OR_EQUAL, 3, omniruntime::type::OMNI_DOUBLE, "2.0")),
          {4, 5, 6});
}

TEST_F(FilterWhileDecodeScanTest, ProjectionOnlyTypeNullFilters)
{
    PrimitiveOrcFixture fixture("null_filters");
    const std::vector<std::pair<int, int32_t>> nullFilterColumns = {
        {4, 4}, // FLOAT
        {5, 5}, // DECIMAL64
        {6, 5}, // DECIMAL128
        {7, 6}, // BINARY
        {8, 7}  // TIMESTAMP
    };

    for (const auto &[column, nullRow] : nullFilterColumns) {
        // Gluten uses OMNI_INT as the type-independent null-filter sentinel.
        auto isNull = Leaf(::common::IS_NULL, column, OMNI_INT, "-1");
        auto nullOn = RunGeneratedPrimitiveScan(true, isNull, fixture.filename());
        ExpectScalarIds(nullOn, {nullRow});
        EXPECT_TRUE(nullOn.columns[column][0].isNull) << "column " << column;

        auto isNotNull = Leaf(::common::IS_NOT_NULL, column, OMNI_INT, "-1");
        auto notNullOn = RunGeneratedPrimitiveScan(true, isNotNull, fixture.filename());
        std::vector<int32_t> expectedIds;
        for (int32_t id = 0; id < 8; ++id) {
            if (id != nullRow) {
                expectedIds.push_back(id);
            }
        }
        ExpectScalarIds(notNullOn, expectedIds);
        for (const auto &cell : notNullOn.columns[column]) {
            EXPECT_FALSE(cell.isNull) << "column " << column;
        }
    }
}

TEST_F(FilterWhileDecodeScanTest, A11AndA21NullableSameColumnRangesMatchSqlWhere)
{
    NullablePredicateOrcFixture fixture("a11_a21");
    auto firstRange = Or(
        Leaf(::common::LESS_THAN, 1, OMNI_INT, "3"),
        Leaf(::common::GREATER_THAN, 1, OMNI_INT, "10"));

    auto a11Off = RunNullablePredicateScan(false, firstRange, fixture.filename());
    auto a11On = RunNullablePredicateScan(true, firstRange, fixture.filename());
    ExpectScalarEqual(a11Off, a11On);
    ExpectNullableLongIds(a11On, {1, 2, 5, std::nullopt, 100});

    auto secondRange = Or(
        Leaf(::common::LESS_THAN, 1, OMNI_INT, "5"),
        Leaf(::common::GREATER_THAN, 1, OMNI_INT, "8"));
    auto a21 = And(firstRange, secondRange);
    auto a21Off = RunNullablePredicateScan(false, a21, fixture.filename());
    auto a21On = RunNullablePredicateScan(true, a21, fixture.filename());
    ExpectScalarEqual(a21Off, a21On);
    ExpectNullableLongIds(a21On, {1, 2, 5, std::nullopt, 100});
}

TEST_F(FilterWhileDecodeScanTest, B01ResidualCrossColumnOrRejectsUnknown)
{
    NullablePredicateOrcFixture fixture("b01");
    auto cond = And(
        Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_LONG, "0"),
        Or(Leaf(::common::LESS_THAN, 1, OMNI_INT, "3"),
           Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "10")));

    auto off = RunNullablePredicateScan(false, cond, fixture.filename());
    auto on = RunNullablePredicateScan(true, cond, fixture.filename());
    ExpectScalarEqual(off, on);
    ExpectNullableLongIds(on, {1, 2, 4, 100});
}

TEST_F(FilterWhileDecodeScanTest, B02ResidualNotAndPreservesUnknown)
{
    NullablePredicateOrcFixture fixture("b02");
    auto cond = And(
        Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_LONG, "0"),
        Not(And(Leaf(::common::GREATER_THAN, 1, OMNI_INT, "6"),
                Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "10"))));

    auto off = RunNullablePredicateScan(false, cond, fixture.filename());
    auto on = RunNullablePredicateScan(true, cond, fixture.filename());
    ExpectScalarEqual(off, on);
    ExpectNullableLongIds(on, {1, 2, 3, 4, 5, 100});
}

TEST_F(FilterWhileDecodeScanTest, B04ResidualOrWithStringProjectionRejectsUnknown)
{
    NullablePredicateOrcFixture fixture("b04");
    auto cond = And(
        Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_LONG, "0"),
        Or(Leaf(::common::LESS_THAN, 1, OMNI_INT, "3"),
           Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "10")));

    auto off = RunNullablePredicateScan(false, cond, fixture.filename(), true);
    auto on = RunNullablePredicateScan(true, cond, fixture.filename(), true);
    ExpectScalarEqual(off, on);
    ExpectNullableLongIds(on, {1, 2, 4, 100});
}

TEST_F(FilterWhileDecodeScanTest, C14LegacyFallbackCrossColumnOrRejectsUnknown)
{
    NullablePredicateOrcFixture fixture("c14");
    auto cond = Or(
        Leaf(::common::LESS_THAN, 1, OMNI_INT, "3"),
        Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "10"));

    auto off = RunNullablePredicateScan(false, cond, fixture.filename());
    auto on = RunNullablePredicateScan(true, cond, fixture.filename());
    ExpectScalarEqual(off, on);
    ExpectNullableLongIds(on, {1, 2, 4, std::nullopt, 100});
}

TEST_F(FilterWhileDecodeScanTest, C15LegacyFallbackNotAndPreservesUnknown)
{
    NullablePredicateOrcFixture fixture("c15");
    auto cond = Not(And(
        Leaf(::common::GREATER_THAN, 1, OMNI_INT, "6"),
        Leaf(::common::GREATER_THAN, 2, OMNI_SHORT, "10")));

    auto off = RunNullablePredicateScan(false, cond, fixture.filename());
    auto on = RunNullablePredicateScan(true, cond, fixture.filename());
    ExpectScalarEqual(off, on);
    ExpectNullableLongIds(on, {1, 2, 3, 4, 5, std::nullopt, 100});
}
