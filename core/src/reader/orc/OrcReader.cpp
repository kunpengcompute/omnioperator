#include "OrcReader.h"
#include "OmniColReader.hh"
#include "SelectiveIntegerColumnReader.hh"
#include "reader/common/Filter.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/ScanSpecBuilder.h"
#include "type/data_type.h"
#include <nlohmann/json.hpp>
#include <limits>
#include <numeric>
#include "OrcFileOverride.hh"
#include "RegionCoalescer.h"

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

uint64_t FilterData(uint8_t *bitMark, std::vector<BaseVector*> *recordBatch, int32_t vectorSize,
    const std::set<int32_t>& isNullSet, const std::set<int32_t>& isNotNullSet)
{
    std::vector<BaseVector*> resultBatch;
    if (common::GetFlatBaseVectorsFromBitMark(*recordBatch, resultBatch, bitMark, vectorSize, isNullSet, isNotNullSet)) {
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
        uint8_t *bitMark = predicateCondition->compute(*recordBatch);
        int32_t vectorSize = (*recordBatch)[0]->GetSize();
        if (omniruntime::BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize) == 0) {
            ClearRecordBatch(*recordBatch);
            return true;
        }
        batchRowSize = FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
            predicateCondition->getIsAllNotNullColumns());
    } catch (const std::exception &e) {
        LogError("filterData fail: %s", e.what());
    }
    return false;
}

// Apply residual (unpushed) predicate on an already-compacted batch; reuse compute + FilterData.
uint64_t ApplyResidualFilter(std::shared_ptr<common::PredicateCondition> &predicateCondition,
                             std::vector<BaseVector *> *recordBatch, uint64_t rows)
{
    if (predicateCondition == nullptr || rows == 0 || recordBatch->empty()) {
        return rows;
    }
    try {
        uint8_t *bitMark = predicateCondition->compute(*recordBatch);
        int32_t vectorSize = (*recordBatch)[0]->GetSize();
        if (omniruntime::BitUtil::CountBits(reinterpret_cast<const uint64_t *>(bitMark), 0, vectorSize) == 0) {
            ClearRecordBatch(*recordBatch);
            return 0;
        }
        return FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
                          predicateCondition->getIsAllNotNullColumns());
    } catch (const std::exception &e) {
        LogError("ApplyResidualFilter fail: %s", e.what());
        return rows;
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

    // Capability gate (when switch ON): all selected cols are supported types AND at least one
    // pushable single-column filter → new path; else fall back to legacy.
    // Success on new path: no log. Any fallback: LogWarn with a distinct reason (no silent fallback).
    if (options->EnableFilterWhileDecode() && rowType_ != nullptr) {
        bool allSupported = allSelectedColumnsAreSupported(*rowType_);
        const auto &enhancementJson = options->GetEnhancementJson();
        bool hasPredicate = enhancementJson != nullptr && enhancementJson->contains("vecPredicateCondition");
        bool usable = false;
        bool needResidual = false;
        if (allSupported && hasPredicate) {
            scanSpec_ = makeScanSpec(*rowType_, enhancementJson, usable, needResidual, residualPredicate_);
        }
        bool hasPushable = usable && scanSpec_ != nullptr && scanSpec_->hasAnyLeafFilter();
        useFilterWhileDecode_ = allSupported && hasPushable;
        applyResidual_ = useFilterWhileDecode_ && needResidual;

        // Residual required but evaluator missing → disable new path to avoid under-filtering.
        if (applyResidual_ && residualPredicate_ == nullptr) {
            useFilterWhileDecode_ = false;
            applyResidual_ = false;
        }
        if (applyResidual_ && residualPredicate_ != nullptr) {
            residualPredicate_->init(options->GetBatchLen());
        }

        if (!useFilterWhileDecode_) {
            const char *reason = nullptr;
            if (!allSupported) {
                reason = "selected columns include unsupported types "
                         "(supports int/bigint/smallint/date/varchar/char)";
            } else if (!hasPredicate) {
                // When Gluten does not push IN etc., C++ sees no JSON — same as pure projection.
                reason = "no vecPredicateCondition from Gluten "
                         "(pure projection, or unsupported predicates such as IN that Gluten does not push down)";
            } else if (!usable) {
                reason = "predicate JSON could not be parsed into scan filters";
            } else if (!hasPushable) {
                reason = "no pushable single-column filter "
                         "(e.g. pure cross-column OR/NOT); selective path has no benefit over legacy";
            } else {
                reason = "residual remainingFilter was required but could not be built "
                         "(refusing new path to avoid missing filters)";
            }
            LogWarn("filterWhileDecode is enabled but this scan fell back to the legacy ORC path: %s", reason);
        }
    }
}


void OrcRowReader::StartNextStripe()
{
    reader.reset(); // ColumnReaders use lots of memory; free old memory first
    rowIndexes.clear();
    bloomFilterIndex.clear();

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

        // Prefetch selected streams (index + data) before loadStripeIndex() so reads hit cache.
        PrefetchSelectedStreams();

        if (sargsApplier) {
            // read row group statistics and bloom filters of current stripe
            loadStripeIndex();
            // select row groups to read in the current stripe
            sargsApplier->pickRowGroups(rowsInCurrentStripe, rowIndexes, bloomFilterIndex);
            if (sargsApplier->hasSelectedFrom(currentRowInStripe)) {
                // current stripe has at least one row group matching the predicate
                break;
            } else {
                // advance to next stripe when current stripe has no matching rows
                currentStripe += 1;
                currentRowInStripe = 0;
            }
        }
    } while (sargsApplier && currentStripe < lastStripe);

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
        reader = omniruntime::reader::omniBuildReader(getSelectedType(), stripeStreams,
            (julianPtr == nullptr) ? nullptr : julianPtr.get());

        // New path: also build SelectiveStructColumnReader (does not wrap the top-level legacy reader).
        if (useFilterWhileDecode_) {
            selectiveStructReader_ = std::make_unique<SelectiveStructColumnReader>(
                getSelectedType(), stripeStreams, scanSpec_.get(),
                (julianPtr == nullptr) ? nullptr : julianPtr.get());
        }

        // New path skips intra-stripe row-group seek (not forwarded to selective children);
        // stripe-level sarg still applies.
        if (sargsApplier && !useFilterWhileDecode_) {
            // move to the 1st selected row group when PPD is enabled.
            currentRowInStripe = advanceToNextRowGroup(currentRowInStripe, rowsInCurrentStripe,
                                                       footer->rowindexstride(), sargsApplier->getRowGroups());
            previousRow = firstRowOfStripe[currentStripe] + currentRowInStripe - 1;
            if (currentRowInStripe > 0) {
                seekToRowGroup(static_cast<uint32_t>(currentRowInStripe / footer->rowindexstride()));
            }
        }
    }
}

void OrcRowReader::PrefetchSelectedStreams()
{
    auto *prefetchable = dynamic_cast<PrefetchableInputStream *>(contents_->stream.get());
    if (prefetchable == nullptr) {
        return; // non-HDFS / non-prefetchable stream: keep direct reads
    }
    const int64_t maxBytes = options_->GetCoalesceMaxBytes();
    if (maxBytes <= 0) {
        return; // coalescing disabled by config
    }

    // A column's index streams share its column id, so they are included too.
    const std::vector<bool> &selected = this->getSelectedColumns();
    std::vector<IoRegion> regions;
    regions.reserve(static_cast<size_t>(currentStripeFooter.streams_size()));
    uint64_t offset = currentStripeInfo.offset();
    for (int i = 0; i < currentStripeFooter.streams_size(); ++i) {
        const auto &stream = currentStripeFooter.streams(i);
        uint64_t length = stream.length();
        uint64_t column = stream.column();
        if (column < selected.size() && selected[column]) {
            regions.push_back(IoRegion{offset, length});
        }
        offset += length;
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

    uint64_t rowsToRead = std::min(batchLen, rowsInCurrentStripe - currentRowInStripe);
    if (sargsApplier) {
        rowsToRead = computeBatchSize(rowsToRead, currentRowInStripe, rowsInCurrentStripe,
                                      footer->rowindexstride(), sargsApplier->getRowGroups());
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
    if (sargsApplier) {
        uint64_t nextRowToRead = advanceToNextRowGroup(currentRowInStripe, rowsInCurrentStripe,
                                                       footer->rowindexstride(), sargsApplier->getRowGroups());
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
            if (currentStripe >= lastStripe) {
                break;
            }
        }

        uint64_t rowsToRead = std::min(batchLen, rowsInCurrentStripe - currentRowInStripe);
        if (rowsToRead == 0) {
            previousRow = lastStripe <= 0 ? footer->numberofrows() :
                          firstRowOfStripe[lastStripe - 1] +
                          footer->stripes(static_cast<int>(lastStripe - 1)).numberofrows();
            return 0;
        }

        uint64_t survivors = selectiveStructReader_->read(rowsToRead, *batch, omniTypeID);
        if (applyResidual_ && survivors > 0) {
            survivors = ApplyResidualFilter(residualPredicate_, batch, survivors);
        }

        previousRow = firstRowOfStripe[currentStripe] + currentRowInStripe;
        currentRowInStripe += rowsToRead;
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
    const auto &rebaseRowType = useFilterWhileDecode_ ? rowType_ : fileRowType_;
    for (int i = 0; i < rebaseRowType->size(); ++i) {
        if (rebaseRowType->childAt(i)->GetId() == type::DataTypeId::OMNI_DATE32) {
            auto vector = recordBatch->at(i);
            for (int j = 0; j < batchRowSize; ++j) {
                auto intVector = reinterpret_cast<Vector<int32_t> *>(vector);
                auto srcVal = intVector->GetValue(j);
                auto finalVal = (GetJulianDaysPtr()->RebaseJulianToGregorianDays(srcVal));
                intVector->SetValue(j, finalVal);
            }
        }
    }
    return batchRowSize;
}
}