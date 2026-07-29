#ifndef READEROPTIONS_H_H
#define READEROPTIONS_H_H

#pragma once
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <orc/Reader.hh>
#include <orc/sargs/SearchArgument.hh>
#include "reader/common/UriInfo.h"
#include "reader/common/JulianGregorianRebase.h"
#include "reader/common/PredicateCondition.h"
#include "type/data_type.h"
#include "parquet/ParquetExpression.h"
#include "codegen/Options.h"
#include "reader/common/TimeRebaseInfo.h"

using omniruntime::codegen::FileFormat;

namespace omniruntime::reader {

enum class Operator {
    OR,
    AND,
    NOT,
    LEAF,
    CONSTANT
};

enum class PredicateOperatorType {
    EQUALS = 0,
    NULL_SAFE_EQUALS,
    LESS_THAN,
    LESS_THAN_EQUALS, IN, BETWEEN, IS_NULL
};

class ReaderOptions {
public:
    virtual ~ReaderOptions() = default;

    template <typename T>
    std::shared_ptr<T> GetExtension(const std::string& key) const {
        auto it = extensions_.find(key);
        if (it != extensions_.end()) {
            try {
                return std::static_pointer_cast<T>(it->second);
            } catch (const std::bad_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }

    template <typename T>
    void SetExtension(const std::string& key, std::shared_ptr<T> ext) {
        extensions_[key] = ext;
    }

    std::unique_ptr<common::TimeRebaseInfo> timeRebaseInfo_;

    const std::unique_ptr<common::TimeRebaseInfo>& GetTimeRebaseInfo() const
    {
        return timeRebaseInfo_;
    }

    void SetTimeRebaseInfo(std::unique_ptr<common::TimeRebaseInfo> info)
    {
        timeRebaseInfo_ = std::move(info);
    }

private:
    std::map<std::string, std::shared_ptr<void>> extensions_;
    std::shared_ptr<UriInfo> uri_;
    std::shared_ptr<::orc::ReaderOptions> orcReaderOptions_;
    std::shared_ptr<::orc::RowReaderOptions> orcRowReaderOptions_;
    omniruntime::type::RowTypePtr rowType_;
    omniruntime::type::RowTypePtr fileRowType_;
    int32_t batchLen_ = 0;
    int64_t coalesceMaxBytes_ = 128 << 20;    // <=0 disables IO coalescing
    int64_t coalesceMaxDistance_ = 512 << 10;
    int64_t filePreloadThreshold_ = 8 << 20;
    std::shared_ptr<nlohmann::json> enhancementJson_;
    std::shared_ptr<common::JulianGregorianRebase> julianPtr_;
    std::shared_ptr<common::PredicateCondition> predicatePtr_;
    std::string ugiString_;
    int64_t splitStart_;
    int64_t splitEnd_;
    std::unique_ptr<::orc::SearchArgument> searchArgument_;
    std::list<std::string> includedColumnsList_;
    std::list<std::string> allColumnsList_;
    Expression parquetPushedFilterArray_;
    std::vector<std::string> parquetIncludedColumns_;
    // filterWhileDecode global switch (injected via enhancementJson, default false).
    bool enableFilterWhileDecode_ = false;

public:
    bool EnableFilterWhileDecode() const
    {
        return enableFilterWhileDecode_;
    }

    void SetEnableFilterWhileDecode(bool enable)
    {
        enableFilterWhileDecode_ = enable;
    }

    const std::shared_ptr<UriInfo>& GetUri() const
    {
        return uri_;
    }

    void SetUri(const std::shared_ptr<UriInfo>& uri)
    {
        uri_ = uri;
    }

    const ::orc::ReaderOptions& GetOrcReaderOptions() const
    {
        return *orcReaderOptions_;
    }

    void SetOrcReaderOptions(const std::shared_ptr<::orc::ReaderOptions>& orcReaderOptions)
    {
        orcReaderOptions_ = orcReaderOptions;
    }

    const ::orc::RowReaderOptions& GetConstantOrcRowReaderOptions() const
    {
        return *orcRowReaderOptions_;
    }

    ::orc::RowReaderOptions& GetOrcRowReaderOptions() const
    {
        return *orcRowReaderOptions_;
    }

    void SetOrcRowReaderOptions(const std::shared_ptr<::orc::RowReaderOptions>& orcRowReaderOptions)
    {
        orcRowReaderOptions_ = orcRowReaderOptions;
    }

    const omniruntime::type::RowTypePtr& GetRowType() const
    {
        return rowType_;
    }

    void SetRowType(const omniruntime::type::RowTypePtr& rowType)
    {
        rowType_ = rowType;
    }

    const omniruntime::type::RowTypePtr& GetFileRowType() const
    {
        return fileRowType_;
    }

    void SetFileRowType(const omniruntime::type::RowTypePtr& fileRowType)
    {
        fileRowType_ = fileRowType;
    }

    int32_t GetBatchLen() const
    {
        return batchLen_;
    }

    void SetBatchLen(int32_t batchLen)
    {
        batchLen_ = batchLen;
    }

    int64_t GetCoalesceMaxBytes() const
    {
        return coalesceMaxBytes_;
    }

    void SetCoalesceMaxBytes(int64_t coalesceMaxBytes)
    {
        coalesceMaxBytes_ = coalesceMaxBytes;
    }

    int64_t GetCoalesceMaxDistance() const
    {
        return coalesceMaxDistance_;
    }

    void SetCoalesceMaxDistance(int64_t coalesceMaxDistance)
    {
        coalesceMaxDistance_ = coalesceMaxDistance;
    }

    int64_t GetFilePreloadThreshold() const
    {
        return filePreloadThreshold_;
    }

    void SetFilePreloadThreshold(int64_t filePreloadThreshold)
    {
        filePreloadThreshold_ = filePreloadThreshold;
    }

    const std::string& GetUgiString() const
    {
        return ugiString_;
    }

    void SetUgiString(const std::string& ugi)
    {
        ugiString_ = ugi;
    }

    const std::shared_ptr<nlohmann::json>& GetEnhancementJson() const
    {
        return enhancementJson_;
    }

    void ParseEnhanceJson(const std::string& enhancementJson);

    void ParseEnhanceJson(const std::string &enhancementJson, FileFormat format);

    void ParsePredicate();

    void ParseParquetPredicate();

    const std::shared_ptr<common::JulianGregorianRebase> &GetJulianPtr() const
    {
        return julianPtr_;
    }

    const std::shared_ptr<common::PredicateCondition> &GetPredicatePtr() const
    {
        return predicatePtr_;
    }

    void SetSplitStart(int64_t splitStart)
    {
        splitStart_ = splitStart;
    }

    int64_t GetSplitStart() const
    {
        return splitStart_;
    }

    void SetSplitEnd(int64_t splitEnd)
    {
        splitEnd_ = splitEnd;
    }

    int64_t GetSplitEnd() const
    {
        return splitEnd_;
    }

    std::unique_ptr<::orc::SearchArgument> releaseSearchArgument()
    {
        return std::unique_ptr<::orc::SearchArgument>(searchArgument_.release());
    }

    std::list<std::string> GetIncludedColumnsList()
    {
        return includedColumnsList_;
    }

    void SetIncludedColumnsList(std::list<std::string> includedColumnsList)
    {
        includedColumnsList_ = includedColumnsList;
    }

    std::list<std::string>& GetAllColumnsList()
    {
        return allColumnsList_;
    }

    const Expression& GetParquetPushedFilterArray() const
    {
        return parquetPushedFilterArray_;
    }

    void SetParquetPushedFilterArray(const Expression& filterArray)
    {
        parquetPushedFilterArray_ = filterArray;
    }

    const std::vector<std::string>& GetParquetIncludedColumns() const
    {
        return parquetIncludedColumns_;
    }

    void SetParquetIncludedColumns(const std::vector<std::string>& columns)
    {
        parquetIncludedColumns_ = columns;
    }

};

// Helper functions for ORC predicate building
int GetLiteral(::orc::Literal &lit, int leafType, const std::string &value);
int BuildLeaves(PredicateOperatorType leafOp, std::vector<::orc::Literal> &litList, ::orc::Literal &lit, 
                const std::string &leafNameString, ::orc::PredicateDataType leafType, ::orc::SearchArgumentBuilder &builder);

} // namespace omniruntime::reader
#endif //READEROPTIONS_H_H