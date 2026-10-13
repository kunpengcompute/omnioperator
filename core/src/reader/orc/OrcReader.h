#ifndef OMNIOPERATORJIT_ORCREADER_H
#define OMNIOPERATORJIT_ORCREADER_H

#include <stdint-gcc.h>
#include <type/data_type.h>
#include <vector>
#include "reader/BufferInput.h"
#include "codegen/Options.h"
#include "reader/Reader.h"
#include "reader/ReaderFactory.h"
#include "orc/RowReader/Reader.hh"
#include "codegen/Options.h"
#include "reader/common/PredicateUtil.h"
#include "orc/StripeStream.hh"
#include "codegen/ScanSpec.h"
#include "RowGroupStatsFilter.hh"
#include "SelectiveStructColumnReader.hh"

using ::orc::InputStream;
using ::orc::FileContents;
using ::orc::DataBuffer;
using omniruntime::type::RowType;

namespace omniruntime::reader {

class OrcRowReader : public omniruntime::reader::RowReader, public ::orc::RowReaderImpl {
public:
    OrcRowReader() = default;

    ~OrcRowReader() override;

    OrcRowReader(std::shared_ptr<FileContents> contents, const std::shared_ptr<ReaderOptions> &options);


    /**
          * direct read VectorBatch in next
          * @param batch the batch to push
          * @param omniTypeId the omniTypeId to push
          * @param batchLen the max row count of batch
          * @return the row size read
     */
    uint64_t Next(uint64_t size, vec::VectorPtr &result) override {};

    uint64_t NextDirect(std::vector<BaseVector *> *batch, int *omniTypeId, uint64_t batchLen) override;

    uint64_t Next(std::vector<BaseVector *> **batch, int *omniTypeId, uint64_t batchLen) override;

    // Filter-while-decode path; called from Next when the capability gate hits.
    uint64_t NextSelective(std::vector<BaseVector *> *batch, int *omniTypeId, uint64_t batchLen);

    void StartNextStripe();

    // Expose the per-task pool stashed on ReaderOptions so the file-scope
    // FilterData/ReadAndFilterData helpers can thread it through
    // common::GetFlatBaseVectorsFromBitMark.
    omniruntime::mem::OmniTaskMemoryPool *GetTaskPool() const
    {
        return options_ != nullptr ? options_->GetTaskPool() : nullptr;
    }

private:
    // Index streams are read on their own so that a stripe eliminated by statistics costs only
    // the index, not a prefetch of every data stream in it.
    enum class PrefetchScope : uint8_t { kIndexOnly, kDataOnly, kAll };

    // Prefetch the current stripe's selected streams as coalesced reads.
    void PrefetchSelectedStreams(PrefetchScope scope);

    // Seek selective children to a row group. Base RowReaderImpl::seekToRowGroup only moves the
    // unused legacy reader on the filter-while-decode path.
    void SeekSelectiveToRowGroup(uint32_t rowGroupEntryId);

    // Fill includedRowGroups_ for the current stripe: the SargsApplier mask, further reduced by
    // the ScanSpec filters (which is where dynamic filters live).
    void PickIncludedRowGroups();

    bool RowGroupMayMatchFilters(uint32_t rowGroupEntryId) const;

    // True when at least one row group at or after 'rowInStripe' survived pruning.
    bool AnyRowGroupSelectedFrom(uint64_t rowInStripe) const;

    // Whether the ScanSpec currently carries a filter worth testing against the row index. This is
    // re-evaluated per stripe because dynamic filters arrive while the scan is already running.
    bool StatsPruningActive() const;

    std::shared_ptr <FileContents> contents_;
    std::vector<BaseVector *> *batch;
    int *omniTypeId;

    bool useFilterWhileDecode_ = false;
    bool applyResidual_ = false;
    std::shared_ptr<common::PredicateCondition> residualPredicate_; // Unpushed residual subtree
    std::shared_ptr<codegen::ScanSpec> scanSpec_;
    std::unique_ptr<SelectiveStructColumnReader> selectiveStructReader_;

    std::vector<StatsPrunableColumn> statsPrunableColumns_;
    // Row groups to read in the current stripe. Empty means "no pruning information, read all".
    std::vector<bool> includedRowGroups_;
    uint64_t prunedRowGroups_ = 0;
    uint64_t prunedStripes_ = 0;
};

class OrcReader : public omniruntime::reader::Reader, public ::orc::ReaderImpl {
public:
    OrcReader() = default;

    ~OrcReader() override = default;

    OrcReader(std::shared_ptr<::orc::FileContents> contents, const std::shared_ptr<ReaderOptions>& options,
        uint64_t fileLength, uint64_t postscriptLength)
        : ::orc::ReaderImpl(contents, options->GetOrcReaderOptions(), fileLength, postscriptLength)
    {
        options_ = options;
        contents_ = contents;
    }

    std::unique_ptr<RowReader> CreateRowReader() override
    {
        auto rowReader = std::make_unique<OrcRowReader>(contents_, options_);
        return std::move(rowReader);
    }

private:
    std::shared_ptr<FileContents> contents_;
};

}
#endif