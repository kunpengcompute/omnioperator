/**
 * Filter-while-decode string/binary projection: int filter + projection on/off parity;
 * value filters on string/binary columns throw.
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

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
#include "util/omni_exception.h"
#include "util/type_util.h"
#include "vector/dictionary_container.h"
#include "vector/vector_common.h"

using omniruntime::type::DataTypePtr;
using omniruntime::type::IntType;
using omniruntime::type::OMNI_INT;
using omniruntime::type::OMNI_VARBINARY;
using omniruntime::type::OMNI_VARCHAR;
using omniruntime::type::ROW;
using omniruntime::type::VarBinaryType;
using omniruntime::type::VarcharType;
using omniruntime::vec::BaseVector;
using omniruntime::vec::DictionaryContainer;
using omniruntime::vec::LargeStringContainer;
using omniruntime::vec::RowVector;
using omniruntime::vec::Vector;
using omniruntime::reader::writeFileOverride;
using omniruntime::writer::createOmniWriter;
using OmniStringVector = Vector<LargeStringContainer<std::string_view>>;
using DictionaryStringVector = Vector<DictionaryContainer<std::string_view>>;
namespace {

nlohmann::json Leaf(::common::PredicateOperatorType op, int index, int dataType, const std::string &value = "0")
{
    nlohmann::json j;
    j["op"] = static_cast<int>(op);
    j["index"] = index;
    j["dataType"] = dataType;
    j["value"] = value;
    return j;
}

std::string BuildEnhanceJson(bool filterWhileDecode, const nlohmann::json &cond, const std::string &includedColumns,
                             const std::string &allColumns)
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

std::string CellString(BaseVector *v, int32_t row)
{
    if (v->IsNull(row)) {
        return std::string("<NULL>");
    }
    if (v->GetEncoding() == omniruntime::vec::OMNI_DICTIONARY) {
        auto sv = static_cast<DictionaryStringVector *>(v)->GetValue(row);
        return std::string(sv.data(), sv.size());
    }
    auto sv = static_cast<OmniStringVector *>(v)->GetValue(row);
    return std::string(sv.data(), sv.size());
}

struct StringProjResult {
    std::vector<int32_t> ids;
    std::vector<std::string> names;
    uint64_t totalRows = 0;
};

class StringProjectionFixture {
public:
    explicit StringProjectionFixture(const std::string &tag)
        : filename_("/tmp/omni_string_proj_" + std::to_string(getpid()) + "_" + tag + ".orc")
    {}

    ~StringProjectionFixture() { std::remove(filename_.c_str()); }

    const std::string &filename() const { return filename_; }

    // id = 0..n-1; name cycles a small dictionary.
    void WriteDictPair(int numRows, bool forceDictionary)
    {
        WritePair(numRows, forceDictionary, ::orc::TypeKind::STRING);
    }

    void WriteDirectPair(int numRows)
    {
        // Unique strings → Direct encoding with dictionary threshold 0.
        UriInfo uri("file", filename_, "", "-1");
        auto outStream = writeFileOverride(uri);
        auto schema = orc::createPrimitiveType(orc::TypeKind::STRUCT);
        schema->addStructField("id", orc::createPrimitiveType(orc::TypeKind::INT));
        schema->addStructField("name", orc::createPrimitiveType(orc::TypeKind::STRING));

        orc::WriterOptions options;
        options.setMemoryPool(orc::getDefaultPool());
        options.setStripeSize(67108864);
        options.setTimezoneName("GMT");
        options.setDictionaryKeySizeThreshold(0.0);
        auto writer = createOmniWriter(*schema, outStream.get(), options);

        auto idVec = std::make_unique<Vector<int32_t>>(numRows);
        auto nameVec = std::make_unique<OmniStringVector>(numRows);
        for (int i = 0; i < numRows; ++i) {
            idVec->SetValue(i, i);
            idVec->SetNotNull(i);
            std::string s = "payload_" + std::to_string(i) + "_" + std::string(32, 'x');
            nameVec->SetValue(i, std::string_view(s));
            nameVec->SetNotNull(i);
        }
        std::vector<BaseVector *> cols{idVec.get(), nameVec.get()};
        auto rowVec = std::make_unique<RowVector>(numRows, cols);
        for (int i = 0; i < numRows; ++i) {
            rowVec->SetNotNull(i);
        }
        writer->add(rowVec.get(), 0, numRows);
        writer->close();
    }

    // Same payload as WriteDictPair, but ORC BINARY / OMNI_VARBINARY.
    void WriteBinaryDictPair(int numRows, bool forceDictionary)
    {
        WritePair(numRows, forceDictionary, ::orc::TypeKind::BINARY);
    }

private:
    void WritePair(int numRows, bool forceDictionary, ::orc::TypeKind payloadKind)
    {
        UriInfo uri("file", filename_, "", "-1");
        auto outStream = writeFileOverride(uri);
        auto schema = orc::createPrimitiveType(orc::TypeKind::STRUCT);
        schema->addStructField("id", orc::createPrimitiveType(orc::TypeKind::INT));
        schema->addStructField("name", orc::createPrimitiveType(payloadKind));

        orc::WriterOptions options;
        options.setMemoryPool(orc::getDefaultPool());
        options.setStripeSize(67108864);
        options.setTimezoneName("GMT");
        options.setDictionaryKeySizeThreshold(forceDictionary ? 1.0 : 0.0);
        auto writer = createOmniWriter(*schema, outStream.get(), options);

        auto idVec = std::make_unique<Vector<int32_t>>(numRows);
        auto nameVec = std::make_unique<OmniStringVector>(numRows);
        static const char *kDict[] = {"alpha", "bravo", "charlie", "delta", "echo"};
        for (int i = 0; i < numRows; ++i) {
            idVec->SetValue(i, i);
            idVec->SetNotNull(i);
            std::string_view sv(kDict[i % 5]);
            nameVec->SetValue(i, sv);
            nameVec->SetNotNull(i);
        }
        std::vector<BaseVector *> cols{idVec.get(), nameVec.get()};
        auto rowVec = std::make_unique<RowVector>(numRows, cols);
        for (int i = 0; i < numRows; ++i) {
            rowVec->SetNotNull(i);
        }
        writer->add(rowVec.get(), 0, numRows);
        writer->close();
    }

    std::string filename_;
};

StringProjResult RunScan(bool filterWhileDecode, const nlohmann::json &cond, const std::string &filename,
                         uint64_t batchLen, const DataTypePtr &payloadType, int payloadOmniTypeId)
{
    auto rowType = ROW({"id", "name"}, {IntType(), payloadType});
    auto readerOpts = std::make_shared<omniruntime::reader::ReaderOptions>();
    readerOpts->ParseEnhanceJson(BuildEnhanceJson(filterWhileDecode, cond, "id,name", "id,name"),
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
    EXPECT_FALSE(included.empty());
    readerOpts->GetOrcRowReaderOptions().include(included);

    auto rowReader = reader->CreateRowReader();
    std::vector<int> typeIds{OMNI_INT, payloadOmniTypeId};

    StringProjResult result;
    std::vector<BaseVector *> *batch = nullptr;
    uint64_t n = rowReader->Next(&batch, typeIds.data(), batchLen);
    while (n > 0) {
        EXPECT_NE(batch, nullptr);
        EXPECT_EQ(batch->size(), 2u);
        if (batch == nullptr || batch->size() != 2u) {
            ClearBatch(batch);
            batch = nullptr;
            break;
        }
        result.totalRows += n;
        for (uint64_t r = 0; r < n; ++r) {
            result.ids.push_back(static_cast<Vector<int32_t> *>((*batch)[0])->GetValue(static_cast<int32_t>(r)));
            result.names.push_back(CellString((*batch)[1], static_cast<int32_t>(r)));
        }
        ClearBatch(batch);
        batch = nullptr;
        n = rowReader->Next(&batch, typeIds.data(), batchLen);
    }
    ClearBatch(batch);
    return result;
}

StringProjResult RunScan(bool filterWhileDecode, const nlohmann::json &cond, const std::string &filename,
                         uint64_t batchLen)
{
    return RunScan(filterWhileDecode, cond, filename, batchLen, VarcharType(65535), OMNI_VARCHAR);
}

void ExpectValueFilterThrows(bool filterWhileDecode, const nlohmann::json &cond, const std::string &filename,
                             const DataTypePtr &payloadType, int payloadOmniTypeId)
{
    EXPECT_THROW(
        {
            try {
                RunScan(filterWhileDecode, cond, filename, 16, payloadType, payloadOmniTypeId);
            } catch (const omniruntime::exception::OmniException &e) {
                // CreateReader calls ParsePredicate before the selective string reader is built.
                // VARCHAR/VARBINARY EQUAL_TO is rejected there (not by RejectUnsupportedValueFilter).
                EXPECT_NE(std::string(e.what()).find("UnSupport DataTypeId"), std::string::npos);
                throw;
            }
        },
        omniruntime::exception::OmniException);
}

} // namespace

TEST(StringProjectionFilterWhileDecodeTest, DictProjectionOnOffParity)
{
    StringProjectionFixture fx("dict");
    constexpr int kRows = 200;
    fx.WriteDictPair(kRows, /*forceDictionary=*/true);

    // Sparse survivors: id < 20 → 10% of batch; exercises selective dict id gather.
    auto cond = Leaf(::common::LESS_THAN, /*index=*/0, OMNI_INT, "20");

    auto off = RunScan(false, cond, fx.filename(), 32);
    auto on = RunScan(true, cond, fx.filename(), 32);

    EXPECT_EQ(off.totalRows, on.totalRows);
    EXPECT_EQ(off.ids, on.ids);
    EXPECT_EQ(off.names, on.names);
    EXPECT_EQ(off.totalRows, 20u);
}

TEST(StringProjectionFilterWhileDecodeTest, DirectProjectionOnOffParity)
{
    StringProjectionFixture fx("direct");
    constexpr int kRows = 120;
    fx.WriteDirectPair(kRows);

    auto cond = Leaf(::common::LESS_THAN, /*index=*/0, OMNI_INT, "12");
    auto off = RunScan(false, cond, fx.filename(), 16);
    auto on = RunScan(true, cond, fx.filename(), 16);

    EXPECT_EQ(off.totalRows, on.totalRows);
    EXPECT_EQ(off.ids, on.ids);
    EXPECT_EQ(off.names, on.names);
    EXPECT_EQ(off.totalRows, 12u);
}

TEST(StringProjectionFilterWhileDecodeTest, StringValueFilterThrowsOnFwd)
{
    StringProjectionFixture fx("throw_string");
    fx.WriteDictPair(40, /*forceDictionary=*/true);

    // EQUAL_TO on string: ParsePredicate cannot build LeafPredicateCondition.
    auto cond = Leaf(::common::EQUAL_TO, /*index=*/1, OMNI_VARCHAR, "alpha");
    ExpectValueFilterThrows(true, cond, fx.filename(), VarcharType(65535), OMNI_VARCHAR);
}

TEST(StringProjectionFilterWhileDecodeTest, BinaryValueFilterThrowsOnFwd)
{
    StringProjectionFixture fx("throw_binary");
    fx.WriteBinaryDictPair(40, /*forceDictionary=*/true);

    // EQUAL_TO on BINARY: same ParsePredicate DataTypeId rejection as VARCHAR.
    auto cond = Leaf(::common::EQUAL_TO, /*index=*/1, OMNI_VARBINARY, "alpha");
    ExpectValueFilterThrows(true, cond, fx.filename(), VarBinaryType(), OMNI_VARBINARY);
}
