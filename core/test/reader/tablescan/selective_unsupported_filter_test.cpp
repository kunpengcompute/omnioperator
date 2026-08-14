/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "codegen/ScanSpec.h"
#include "reader/common/Filter.h"
#include "reader/orc/SelectiveDecimalColumnReader.hh"
#include "reader/orc/SelectiveFloatingPointColumnReader.hh"
#include "reader/orc/SelectiveStringDictionaryColumnReader.hh"
#include "reader/orc/SelectiveStringDirectColumnReader.hh"
#include "reader/orc/SelectiveTimestampColumnReader.hh"
#include "type/data_type.h"
#include "util/omni_exception.h"

namespace {

class TestValueFilter final : public ::common::Filter {
public:
    explicit TestValueFilter(::common::FilterKind kind, bool nullAllowed = false)
        : Filter(kind, nullAllowed)
    {}

    ::common::FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<TestValueFilter>(kind(), nullAllowed);
    }
};

template <typename TReader>
void ExpectUnsupportedFilterBeforeDecode(
    std::unique_ptr<::orc::Type> orcType,
    int omniTypeId,
    ::common::FilterKind filterKind,
    const std::string &readerName)
{
    omniruntime::codegen::ScanSpec spec("value");
    spec.setFilter(std::make_shared<TestValueFilter>(filterKind));

    // A null inner reader is intentional: unsupported filters must be rejected before decode.
    TReader reader(&spec, orcType.get(), std::unique_ptr<::orc::ColumnReader>());
    std::vector<common::vector_size_t> activeRows = {0};

    try {
        reader.read(1, common::RowSet(activeRows.data(), activeRows.size()), omniTypeId);
        FAIL() << "unsupported filter did not throw";
    } catch (const omniruntime::exception::OmniException &e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("EXPRESSION_NOT_SUPPORT"), std::string::npos);
        EXPECT_NE(message.find(readerName + " unsupported filter kind"), std::string::npos);
        EXPECT_NE(
            message.find("filter kind: " + std::to_string(static_cast<int>(filterKind))),
            std::string::npos);
        EXPECT_NE(message.find("omniTypeId: " + std::to_string(omniTypeId)), std::string::npos);
    } catch (...) {
        FAIL() << "unsupported filter threw an unexpected exception type";
    }
}

} // namespace

TEST(SelectiveUnsupportedFilterTest, FloatValueFilterThrowsBeforeDecode)
{
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveFloatingPointColumnReader>(
        ::orc::createPrimitiveType(::orc::FLOAT),
        omniruntime::type::OMNI_FLOAT,
        ::common::FilterKind::kFloatRange,
        "SelectiveFloatingPointColumnReader");
}

TEST(SelectiveUnsupportedFilterTest, DecimalValueFiltersThrowBeforeDecode)
{
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveDecimalColumnReader>(
        ::orc::createDecimalType(18, 4),
        omniruntime::type::OMNI_DECIMAL64,
        ::common::FilterKind::kBigintRange,
        "SelectiveDecimalColumnReader");
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveDecimalColumnReader>(
        ::orc::createDecimalType(38, 4),
        omniruntime::type::OMNI_DECIMAL128,
        ::common::FilterKind::kHugeintRange,
        "SelectiveDecimalColumnReader");
}

TEST(SelectiveUnsupportedFilterTest, TimestampValueFilterThrowsBeforeDecode)
{
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveTimestampColumnReader>(
        ::orc::createPrimitiveType(::orc::TIMESTAMP),
        omniruntime::type::OMNI_TIMESTAMP,
        ::common::FilterKind::kTimestampRange,
        "SelectiveTimestampColumnReader");
}

TEST(SelectiveUnsupportedFilterTest, BinaryDirectValueFilterThrowsBeforeDecode)
{
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveStringDirectColumnReader>(
        ::orc::createPrimitiveType(::orc::BINARY),
        omniruntime::type::OMNI_VARBINARY,
        ::common::FilterKind::kBytesRange,
        "SelectiveStringDirectColumnReader");
}

TEST(SelectiveUnsupportedFilterTest, BinaryDictionaryValueFilterThrowsBeforeDecode)
{
    ExpectUnsupportedFilterBeforeDecode<omniruntime::reader::SelectiveStringDictionaryColumnReader>(
        ::orc::createPrimitiveType(::orc::BINARY),
        omniruntime::type::OMNI_VARBINARY,
        ::common::FilterKind::kBytesRange,
        "SelectiveStringDictionaryColumnReader");
}
