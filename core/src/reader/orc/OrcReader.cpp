#include "OrcReader.h"
#include "OmniColReader.hh"
#include "SelectiveIntegerColumnReader.hh"
#include "reader/common/Filter.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/ScanSpecBuilder.h"
#include "type/data_type.h"
#include "util/debug.h"
#include <nlohmann/json.hpp>
#include <limits>
#include <numeric>
#include "OrcFileOverride.hh"
#include "RegionCoalescer.h"
#include <list>
#include <unordered_map>

namespace omniruntime::reader {

namespace
{

void ClearRecordBatch(std::vector<BaseVector*>& recordBatch)
{
    for (auto vec : recordBatch)
    {
        delete vec;
    }
    recordBatch.clear();
}

// The trailing pool param forwards the
// per-task pool from OrcRowReader's options_->GetTaskPool() through to the hot
// CreateVector allocation inside common::GetFlatBaseVectorsFromBitMark.
uint64_t FilterData(uint8_t *bitMark, std::vector<BaseVector*> *recordBatch, int32_t vectorSize,
    const std::set<int32_t>& isNullSet, const std::set<int32_t>& isNotNullSet,
    omniruntime::mem::OmniTaskMemoryPool *taskPool)
{
    std::vector<BaseVector*> resultBatch;
    if (common::GetFlatBaseVectorsFromBitMark(*recordBatch, resultBatch, bitMark, vectorSize, isNullSet, isNotNullSet,
        taskPool)) {
        ClearRecordBatch(*recordBatch);
        *recordBatch = std::move(resultBatch);
        return (*recordBatch)[0]->GetSize();
    }
    // On failure, return the original batch
    ClearRecordBatch(resultBatch);
    return vectorSize;
}

bool ReadAndFilterData(OrcRowReader& rowReaderPtr,
std::vector<BaseVector*> *recordBatch, uint64_t &batchRowSize, int *omniTypeId, uint64_t batchLen)
{
    batchRowSize = rowReaderPtr.NextDirect(recordBatch, omniTypeId, batchLen);
    std::shared_ptr<common::PredicateCondition>& predicateCondition = rowReaderPtr.GetPredicatePtr();
    if (batchRowSize == 0 || predicateCondition == nullptr) {
        return false;
    }
    try {
        auto predicateResult = predicateCondition->compute(*recordBatch);
        // WHERE keeps TRUE only; FALSE and UNKNOWN are both rejected.
        uint8_t *bitMark = predicateResult.trueBits;
        int32_t vectorSize = (*recordBatch)[0]->GetSize();
        if (omniruntime::BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize) == 0) {
            ClearRecordBatch(*recordBatch);
            return true;
        }
        // Thread the per-task pool from OrcRowReader::options_->GetTaskPool(),
        // as the omniBuildReader call does.
        auto* taskPool = rowReaderPtr.GetTaskPool();
        batchRowSize = FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
            predicateCondition->getIsAllNotNullColumns(), taskPool);
    } catch (const std::exception &e) {
        ClearRecordBatch(*recordBatch);
        throw OmniException("OPERATOR_RUNTIME_ERROR", std::string("ORC predicate filtering failed: ") + e.what());
    }
    return false;
}

// Apply residual (unpushed) predicate on an already-compacted batch; reuse compute + FilterData.
uint64_t ApplyResidualFilter(std::shared_ptr<common::PredicateCondition> &predicateCondition,
                             std::vector<BaseVector *> *recordBatch, uint64_t rows,
                             omniruntime::mem::OmniTaskMemoryPool *taskPool)
{
    if (predicateCondition == nullptr || rows == 0 || recordBatch->empty()) {
        return rows;
    }
    try {
        auto predicateResult = predicateCondition->compute(*recordBatch);
        // WHERE keeps TRUE only; FALSE and UNKNOWN are both rejected.
        uint8_t *bitMark = predicateResult.trueBits;
        int32_t vectorSize = (*recordBatch)[0]->GetSize();
        if (omniruntime::BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize) == 0) {
            ClearRecordBatch(*recordBatch);
            return 0;
        }
        return FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
                          predicateCondition->getIsAllNotNullColumns(), taskPool);
    } catch (const std::exception &e) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", std::string("ApplyResidualFilter failed: ") + e.what());
    }
}

}

OrcRowReader::OrcRowReader(std::shared_ptr<FileContents> contents, const std::shared_ptr<ReaderOptions>& options)
: ::orc::RowReaderImpl(contents, options->GetOrcRowReaderOptions())
{
    contents_ = contents;
    options_ = options;
    julianDaysPtr = std::make_unique<common::JulianGregorianRebaseDays>();
    julianPtr = options->GetJulianPtr();
    predicatePtr = options->GetPredicatePtr();
    rowType_ = options->GetRowType();
    fileRowType_ = options->GetFileRowType();

    // Capability gate (when switch ON): all selected cols are supported types → Selective
    // path (Velox-style). Dynamic filters are applied onto the HiveDataSource ScanSpec
    // before the reader is constructed; JSON static predicates AND-merge onto the same tree.
    // The ScanSpec is where dynamic filters are installed, so it is needed on both the selective
    // and the legacy path: statistics-based row group pruning applies to either one.
    scanSpec_ = options->GetScanSpec();

    if (options->EnableFilterWhileDecode() && rowType_ != nullptr) {
        bool allSupported = allSelectedColumnsAreSupported(*rowType_);
        const auto &enhancementJson = options->GetEnhancementJson();
        bool hasPredicate = enhancementJson != nullptr && enhancementJson->contains("vecPredicateCondition");
        bool usable = true;
        bool needResidual = false;
        if (scanSpec_ == nullptr) {
            scanSpec_ = std::make_shared<codegen::ScanSpec>("root");
            for (uint32_t i = 0; i < rowType_->size(); ++i) {
                scanSpec_->addField(rowType_->nameOf(i), i);
            }
        }
        if (allSupported && hasPredicate) {
            applyVecPredicateToScanSpec(*scanSpec_, *rowType_, enhancementJson, usable, needResidual,
                                        residualPredicate_);
            if (!usable) {
                // Keep Hive spec (may already hold a dynamic filter); skip residual.
                needResidual = false;
                residualPredicate_ = nullptr;
                usable = true;
            }
        }
        useFilterWhileDecode_ = allSupported;
        applyResidual_ = useFilterWhileDecode_ && needResidual;

        if (useFilterWhileDecode_ && scanSpec_ != nullptr) {
            LogDebug("FWD: OrcRowReader selective path hasAnyLeafFilter=%d children=%zu",
                static_cast<int>(scanSpec_->hasAnyLeafFilter()), scanSpec_->children().size());
        }

        // Residual required but evaluator missing → disable new path to avoid under-filtering.
        if (applyResidual_ && residualPredicate_ == nullptr) {
            useFilterWhileDecode_ = false;
            applyResidual_ = false;
        }
        if (applyResidual_ && residualPredicate_ != nullptr) {
            residualPredicate_->init(options->GetBatchLen());
        }

        if (!useFilterWhileDecode_) {
            const char *reason = !allSupported
                ? "selected columns include unsupported types "
                  "(supports primitive ORC scalars; array/map/row remain on the legacy path)"
                : "residual remainingFilter was required but could not be built "
                  "(refusing new path to avoid missing filters)";
            LogWarn("filterWhileDecode is enabled but this scan fell back to the legacy ORC path: %s", reason);
        }
    }

    // Collect every leaf whose statistics can be tested, not only those that already carry a
    // filter: a dynamic filter can be installed on the ScanSpec after this reader was built.
    CollectStatsPrunableColumns(this->getSelectedType(), scanSpec_.get(), statsPrunableColumns_);
}

OrcRowReader::~OrcRowReader()
{
    // One line per split: this is the signal that tells whether a dynamic filter actually saved
    // decode work, as opposed to merely rejecting rows after they were already decoded.
    if (prunedRowGroups_ > 0 || prunedStripes_ > 0) {
        LogDebug("DFP: split pruned %llu row groups and %llu whole stripes from ScanSpec statistics",
                 static_cast<unsigned long long>(prunedRowGroups_),
                 static_cast<unsigned long long>(prunedStripes_));
    }
}

bool OrcRowReader::StatsPruningActive() const
{
    return !statsPrunableColumns_.empty() && footer->rowindexstride() > 0 && scanSpec_ != nullptr &&
           scanSpec_->hasAnyLeafFilter();
}

bool OrcRowReader::RowGroupMayMatchFilters(uint32_t rowGroupEntryId) const
{
    for (const auto &column : statsPrunableColumns_) {
        if (column.spec == nullptr || !column.spec->hasFilter()) {
            continue;
        }
        const auto it = rowIndexes.find(column.columnId);
        if (it == rowIndexes.end()) {
            continue;
        }
        const auto &rowIndex = it->second;
        if (static_cast<int32_t>(rowGroupEntryId) >= rowIndex.entry_size()) {
            continue;
        }
        const auto &entry = rowIndex.entry(static_cast<int32_t>(rowGroupEntryId));
        if (!entry.has_statistics()) {
            continue;
        }
        if (!StatsMayContainMatch(column, entry.statistics())) {
            return false;
        }
    }
    return true;
}

void OrcRowReader::PickIncludedRowGroups()
{
    if (sargsApplier != nullptr) {
        // Copy: the SARG mask belongs to the applier and is only recomputed per stripe.
        includedRowGroups_ = sargsApplier->getRowGroups();
    } else {
        includedRowGroups_.clear();
    }

    const uint64_t stride = footer->rowindexstride();
    if (!StatsPruningActive() || rowIndexes.empty() || rowsInCurrentStripe == 0 || stride == 0) {
        return;
    }

    const uint64_t groups = (rowsInCurrentStripe + stride - 1) / stride;
    if (includedRowGroups_.size() < groups) {
        includedRowGroups_.resize(groups, true);
    }

    uint64_t pruned = 0;
    for (uint64_t rg = 0; rg < groups; ++rg) {
        if (!includedRowGroups_[rg]) {
            continue;
        }
        if (!RowGroupMayMatchFilters(static_cast<uint32_t>(rg))) {
            includedRowGroups_[rg] = false;
            ++pruned;
        }
    }
    prunedRowGroups_ += pruned;
    if (pruned > 0) {
        LogDebug("DFP: stripe %llu pruned %llu/%llu row groups by ScanSpec statistics",
                 static_cast<unsigned long long>(currentStripe), static_cast<unsigned long long>(pruned),
                 static_cast<unsigned long long>(groups));
    }
}

bool OrcRowReader::AnyRowGroupSelectedFrom(uint64_t rowInStripe) const
{
    if (includedRowGroups_.empty()) {
        return true;
    }
    const uint64_t stride = footer->rowindexstride();
    if (stride == 0) {
        return true;
    }
    for (uint64_t rg = rowInStripe / stride; rg < includedRowGroups_.size(); ++rg) {
        if (includedRowGroups_[rg]) {
            return true;
        }
    }
    return false;
}


void OrcRowReader::StartNextStripe()
{
    reader.reset(); // ColumnReaders use lots of memory; free old memory first
    rowIndexes.clear();
    bloomFilterIndex.clear();
    includedRowGroups_.clear();

    do {
        currentStripeInfo = footer->stripes(static_cast<int>(currentStripe));
        uint64_t fileLength = contents_->stream->getLength();
        if (currentStripeInfo.offset() + currentStripeInfo.indexlength() +
            currentStripeInfo.datalength() + currentStripeInfo.footerlength() >= fileLength) {
            std::stringstream msg;
            msg << "Malformed StripeInformation at stripe index " << currentStripe << ": fileLength="
                << fileLength << ", StripeInfo=(offset=" << currentStripeInfo.offset() << ", indexLength="
                << currentStripeInfo.indexlength() << ", dataLength=" << currentStripeInfo.datalength()
                << ", footerLength=" << currentStripeInfo.footerlength() << ")";
            throw ::orc::ParseError(msg.str());
        }
        currentStripeFooter = getStripeFooter(currentStripeInfo, *contents_.get());
        rowsInCurrentStripe = currentStripeInfo.numberofrows();

        const bool prunable = sargsApplier != nullptr || StatsPruningActive();
        if (!prunable) {
            // Nothing can eliminate rows here, so warm index and data in one coalesced pass.
            PrefetchSelectedStreams(PrefetchScope::kAll);
            break;
        }

        // Read the index first. It is small, and if pruning eliminates the whole stripe we avoid
        // pulling any of its data streams at all.
        PrefetchSelectedStreams(PrefetchScope::kIndexOnly);
        loadStripeIndex();
        if (sargsApplier != nullptr) {
            sargsApplier->pickRowGroups(rowsInCurrentStripe, rowIndexes, bloomFilterIndex);
        }
        PickIncludedRowGroups();

        if (AnyRowGroupSelectedFrom(currentRowInStripe)) {
            PrefetchSelectedStreams(PrefetchScope::kDataOnly);
            break;
        }

        // No surviving row group: advance without ever touching the data streams.
        ++prunedStripes_;
        currentStripe += 1;
        currentRowInStripe = 0;
        rowsInCurrentStripe = 0;
        rowIndexes.clear();
        bloomFilterIndex.clear();
        includedRowGroups_.clear();
    } while (currentStripe < lastStripe);

    if (currentStripe >= lastStripe) {
        // Every remaining stripe was pruned. NextDirect used to fall through to
        // reader->next() with reader_ already reset() — SIGSEGV. NextSelective already
        // checks currentStripe after StartNextStripe; the legacy path must do the same.
        reader.reset();
        selectiveStructReader_.reset();
        rowsInCurrentStripe = 0;
        return;
    }

    if (currentStripe < lastStripe) {
        // get writer timezone info from stripe footer to help understand timestamp values.
        const ::orc::Timezone &writerTimezone =
            currentStripeFooter.has_writertimezone() ?
            ::orc::getTimezoneByName(currentStripeFooter.writertimezone()) :
            localTimezone;
        ::orc::StripeStreamsImpl stripeStreams(*this, currentStripe, currentStripeInfo,
                                               currentStripeFooter, currentStripeInfo.offset(),
                                               *contents_->stream, writerTimezone,
                                               readerTimezone);
        // Exactly one reader tree per stripe. The selective tree builds its own inner readers for
        // every column, so also building the legacy tree would open every stream twice, allocate a
        // second decompression buffer per stream, and re-read and re-decode every string
        // dictionary -- all for a tree that NextSelective never touches.
        if (useFilterWhileDecode_) {
            reader.reset();
            selectiveStructReader_ = std::make_unique<SelectiveStructColumnReader>(
                getSelectedType(), stripeStreams, scanSpec_.get(),
                (julianPtr == nullptr) ? nullptr : julianPtr.get(),
                options_ != nullptr ? options_->GetTaskPool() : nullptr);
        } else {
            reader = omniruntime::reader::omniBuildReader(getSelectedType(), stripeStreams,
                (julianPtr == nullptr) ? nullptr : julianPtr.get(),
                options_ != nullptr ? options_->GetTaskPool() : nullptr);
        }

        if (!includedRowGroups_.empty()) {
            // move to the 1st selected row group when PPD or statistics pruning is in effect.
            currentRowInStripe = advanceToNextRowGroup(currentRowInStripe, rowsInCurrentStripe,
                                                       footer->rowindexstride(), includedRowGroups_);
            previousRow = firstRowOfStripe[currentStripe] + currentRowInStripe - 1;
            if (currentRowInStripe > 0) {
                const auto rowGroupId =
                    static_cast<uint32_t>(currentRowInStripe / footer->rowindexstride());
                if (useFilterWhileDecode_) {
                    SeekSelectiveToRowGroup(rowGroupId);
                } else {
                    seekToRowGroup(rowGroupId);
                }
            }
        }
    }
}

namespace {

// The two kinds loadStripeIndex() consumes; everything else in a stripe is column data.
bool IsIndexStreamKind(::orc::proto::Stream_Kind kind)
{
    return kind == ::orc::proto::Stream_Kind_ROW_INDEX || kind == ::orc::proto::Stream_Kind_BLOOM_FILTER_UTF8;
}

} // namespace

void OrcRowReader::PrefetchSelectedStreams(PrefetchScope scope)
{
    auto *prefetchable = dynamic_cast<PrefetchableInputStream *>(contents_->stream.get());
    if (prefetchable == nullptr) {
        return; // non-HDFS / non-prefetchable stream: keep direct reads
    }
    const int64_t maxBytes = options_->GetCoalesceMaxBytes();
    if (maxBytes <= 0) {
        return; // coalescing disabled by config
    }

    // Every selected column is fetched, filter columns and projections alike.
    //
    // Restricting this to filter columns only pays off with lazy materialization, where a
    // projection is opened after filtering and can be skipped for batches that survive nothing.
    // SelectiveStructColumnReader has no such deferral: it reads every projection in the same
    // batch, and even the empty-survivor path still walks their data streams through skipBatch.
    // Narrowing therefore saves no IO at all -- it only drops the projections out of the coalesced
    // read, leaving them to the chunked synchronous reads in the input stream's cache-miss path,
    // interleaved with decoding.
    const std::vector<bool> &prefetchCols = this->getSelectedColumns();

    std::vector<IoRegion> regions;
    regions.reserve(static_cast<size_t>(currentStripeFooter.streams_size()));
    uint64_t offset = currentStripeInfo.offset();
    for (int i = 0; i < currentStripeFooter.streams_size(); ++i) {
        const auto &stream = currentStripeFooter.streams(i);
        uint64_t length = stream.length();
        uint64_t column = stream.column();
        const bool isIndex = stream.has_kind() && IsIndexStreamKind(stream.kind());
        const bool inScope = scope == PrefetchScope::kAll ||
                             (scope == PrefetchScope::kIndexOnly ? isIndex : !isIndex);
        if (inScope && column < prefetchCols.size() && prefetchCols[column]) {
            regions.push_back(IoRegion{offset, length});
        }
        offset += length;
    }
    if (regions.empty()) {
        return;
    }

    const int64_t maxDistance = options_->GetCoalesceMaxDistance();
    auto merged = coalesceRegions(std::move(regions), static_cast<uint64_t>(maxDistance < 0 ? 0 : maxDistance),
                                  static_cast<uint64_t>(maxBytes));

    prefetchable->prefetchRegions(merged);
}

uint64_t OrcRowReader::NextDirect(std::vector<BaseVector *> *batch, int *omniTypeID, uint64_t batchLen)
{
    if (currentStripe >= lastStripe) {
        if (lastStripe > 0) {
            previousRow = firstRowOfStripe[lastStripe - 1] +
                          footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows();
        } else {
            previousRow = 0;
        }
        return false;
    }
    if (currentRowInStripe == 0) {
        StartNextStripe();
    }
    if (currentStripe >= lastStripe || reader == nullptr) {
        if (lastStripe > 0) {
            previousRow = firstRowOfStripe[lastStripe - 1] +
                          footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows();
        } else {
            previousRow = 0;
        }
        return 0;
    }

    uint64_t rowsToRead = std::min(batchLen, rowsInCurrentStripe - currentRowInStripe);
    if (!includedRowGroups_.empty()) {
        rowsToRead = computeBatchSize(rowsToRead, currentRowInStripe, rowsInCurrentStripe,
                                      footer->rowindexstride(), includedRowGroups_);
    }
    if (rowsToRead == 0) {
        previousRow = lastStripe <= 0 ? footer->numberofrows() :
                      firstRowOfStripe[lastStripe - 1] +
                      footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows();
        return rowsToRead;
    }
    if (enableEncodedBlock) {
        throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT", "enableEncodedBlock is not finished!!!");
    } else {
        const ::orc::Type &baseTp = this->getSelectedType();
        reader->next(reinterpret_cast<void *&>(batch), rowsToRead, nullptr, baseTp, omniTypeID);
    }
    previousRow = firstRowOfStripe[currentStripe] + currentRowInStripe;
    currentRowInStripe += rowsToRead;
    if (!includedRowGroups_.empty()) {
        uint64_t nextRowToRead = advanceToNextRowGroup(currentRowInStripe, rowsInCurrentStripe,
                                                       footer->rowindexstride(), includedRowGroups_);
        if (currentRowInStripe != nextRowToRead) {
            // it is guaranteed to be at start of a row group
            currentRowInStripe = nextRowToRead;
            if (currentRowInStripe < rowsInCurrentStripe) {
                seekToRowGroup(static_cast<uint32_t>(currentRowInStripe / footer->rowindexstride()));
            }
        }
    }
    if (currentRowInStripe >= rowsInCurrentStripe) {
        currentStripe += 1;
        currentRowInStripe = 0;
    }
    return rowsToRead;
}

uint64_t OrcRowReader::NextSelective(std::vector<BaseVector *> *batch, int *omniTypeID, uint64_t batchLen)
{
    // If a whole batch is filtered empty, keep reading until rows remain or the stripe ends.
    while (currentStripe < lastStripe) {
        if (currentRowInStripe == 0) {
            StartNextStripe();
            if (currentStripe >= lastStripe || selectiveStructReader_ == nullptr) {
                break;
            }
        }

        uint64_t rowsToRead = std::min(batchLen, rowsInCurrentStripe - currentRowInStripe);
        if (!includedRowGroups_.empty()) {
            rowsToRead = computeBatchSize(rowsToRead, currentRowInStripe, rowsInCurrentStripe,
                                          footer->rowindexstride(), includedRowGroups_);
        }
        if (rowsToRead == 0) {
            // Pruning can leave the cursor on an excluded row group; step over it and retry
            // instead of reporting end of split.
            if (currentRowInStripe < rowsInCurrentStripe && !includedRowGroups_.empty()) {
                const uint64_t nextRowToRead = advanceToNextRowGroup(
                    currentRowInStripe, rowsInCurrentStripe, footer->rowindexstride(), includedRowGroups_);
                if (nextRowToRead > currentRowInStripe) {
                    currentRowInStripe = nextRowToRead;
                    if (currentRowInStripe < rowsInCurrentStripe) {
                        SeekSelectiveToRowGroup(
                            static_cast<uint32_t>(currentRowInStripe / footer->rowindexstride()));
                        continue;
                    }
                }
            }
            if (currentRowInStripe >= rowsInCurrentStripe) {
                currentStripe += 1;
                currentRowInStripe = 0;
                continue;
            }
            previousRow = lastStripe <= 0 ? footer->numberofrows() :
                          firstRowOfStripe[lastStripe - 1] +
                          footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows();
            return 0;
        }

        uint64_t survivors = selectiveStructReader_->read(rowsToRead, *batch, omniTypeID);
        if (applyResidual_ && survivors > 0) {
            survivors = ApplyResidualFilter(residualPredicate_, batch, survivors, GetTaskPool());
        }

        previousRow = firstRowOfStripe[currentStripe] + currentRowInStripe;
        currentRowInStripe += rowsToRead;
        if (!includedRowGroups_.empty()) {
            uint64_t nextRowToRead = advanceToNextRowGroup(currentRowInStripe, rowsInCurrentStripe,
                                                           footer->rowindexstride(), includedRowGroups_);
            if (currentRowInStripe != nextRowToRead) {
                currentRowInStripe = nextRowToRead;
                if (currentRowInStripe < rowsInCurrentStripe) {
                    SeekSelectiveToRowGroup(
                        static_cast<uint32_t>(currentRowInStripe / footer->rowindexstride()));
                }
            }
        }
        if (currentRowInStripe >= rowsInCurrentStripe) {
            currentStripe += 1;
            currentRowInStripe = 0;
        }

        if (survivors > 0) {
            return survivors;
        }
        ClearRecordBatch(*batch);
    }

    previousRow = (lastStripe > 0)
        ? firstRowOfStripe[lastStripe - 1] +
              footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows()
        : 0;
    return 0;
}

uint64_t OrcRowReader::Next(std::vector<BaseVector *> **batch, int *omniTypeID, uint64_t batchLen)
{
    auto recordBatch = new std::vector<BaseVector *>();
    uint64_t batchRowSize = 0;
    if (useFilterWhileDecode_) {
        batchRowSize = NextSelective(recordBatch, omniTypeID, batchLen);
    } else {
        bool needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeID, batchLen);
        while (needReadAgain) {
            needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeID, batchLen);
        }
    }
    *batch = recordBatch;
    if (batchRowSize <= 0) {
        return batchRowSize;
    }

    // DATE32 rebase: new path by rowType_ channel; legacy by fileRowType_.
    // Partition columns live on rowType_ beyond recordBatch; never index past the vectors
    // Next actually produced (at() on a missing slot is a SIGSEGV, not an exception here).
    const auto &rebaseRowType = useFilterWhileDecode_ ? rowType_ : fileRowType_;
    if (rebaseRowType != nullptr && recordBatch != nullptr) {
        const uint32_t n = std::min(rebaseRowType->size(), static_cast<uint32_t>(recordBatch->size()));
        for (uint32_t i = 0; i < n; ++i) {
            if (rebaseRowType->childAt(i) == nullptr ||
                rebaseRowType->childAt(i)->GetId() != type::DataTypeId::OMNI_DATE32) {
                continue;
            }
            auto *vector = recordBatch->at(i);
            if (vector == nullptr) {
                continue;
            }
            auto *intVector = reinterpret_cast<Vector<int32_t> *>(vector);
            const int rows = static_cast<int>(batchRowSize);
            for (int j = 0; j < rows; ++j) {
                auto srcVal = intVector->GetValue(j);
                auto finalVal = (GetJulianDaysPtr()->RebaseJulianToGregorianDays(srcVal));
                intVector->SetValue(j, finalVal);
            }
        }
    }
    return batchRowSize;
}

void OrcRowReader::SeekSelectiveToRowGroup(uint32_t rowGroupEntryId)
{
    if (selectiveStructReader_ == nullptr) {
        return;
    }
    // Keep position lists alive for the duration of seek: PositionProvider holds iterators.
    std::list<std::list<uint64_t>> positions;
    std::unordered_map<uint64_t, ::orc::PositionProvider> positionProviders;
    for (auto rowIndex = rowIndexes.cbegin(); rowIndex != rowIndexes.cend(); ++rowIndex) {
        const uint64_t colId = rowIndex->first;
        const auto &entry = rowIndex->second.entry(static_cast<int32_t>(rowGroupEntryId));
        positions.emplace_back();
        auto &position = positions.back();
        for (int pos = 0; pos != entry.positions_size(); ++pos) {
            position.push_back(entry.positions(pos));
        }
        positionProviders.insert(std::make_pair(colId, ::orc::PositionProvider(position)));
    }
    selectiveStructReader_->seekToRowGroup(positionProviders);
}
}
