#include "OrcReader.h"
#include "OmniColReader.hh"
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
    // 失败返回原始的
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
        batchRowSize = FilterData(bitMark, recordBatch, vectorSize, predicateCondition->getIsAllNullColumns(),
            predicateCondition->getIsAllNotNullColumns());
    } catch (const std::exception &e) {
        ClearRecordBatch(*recordBatch);
        LogError("filterData fail: %s", e.what());
    }
    return false;
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

        if (sargsApplier) {
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

uint64_t OrcRowReader::Next(std::vector<BaseVector *> **batch, int *omniTypeID, uint64_t batchLen)
{
    auto recordBatch = new std::vector<BaseVector *>();
    uint64_t batchRowSize = 0;
    bool needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeID, batchLen);
    while (needReadAgain) {
        needReadAgain = ReadAndFilterData(*this, recordBatch, batchRowSize, omniTypeID, batchLen);
    }
    *batch = recordBatch;
    if (batchRowSize <= 0) {
        return batchRowSize;
    }

    for (int i = 0; i < fileRowType_->size(); ++i) {
        if (fileRowType_->childAt(i)->GetId() == type::DataTypeId::OMNI_DATE32) {
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
