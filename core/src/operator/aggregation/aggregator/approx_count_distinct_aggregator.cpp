/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: approx_count_distinct aggregate implementation.
 */
#include "approx_count_distinct_aggregator.h"
#include "operator/aggregation/definitions.h"
#include "operator/hash_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {

using omniruntime::vec::VectorHelper;

namespace {
// Align with Velox approx_distinct: hash value bytes with XXH64-style (Velox uses XXH64(seed=0)),
// so HLL (index, value) distribution matches Velox and the bias correction tables apply correctly.
inline uint64_t HllHashBytes(const void* data, int32_t len) {
  return static_cast<uint64_t>(HashUtil::XxHash64Hash(
      0, const_cast<int8_t*>(reinterpret_cast<const int8_t*>(data)), 0, len));
}
}  // namespace

/**
 * Destructor. Frees all HLL/boolean state buffers allocated in InitState and held in stateBufferPtrs_.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
ApproxCountDistinctAggregator<IN_ID, OUT_ID>::~ApproxCountDistinctAggregator()
{
    for (int64_t ptr : stateBufferPtrs_) {
        if (ptr != 0) {
            delete[] reinterpret_cast<char *>(ptr);
        }
    }
    stateBufferPtrs_.clear();
}

/**
 * Invoked by GetOutput, it extracts the serialized data of HyperLogLog (or boolean state) from AggregateState
 * and stores it into the specified BaseVector.
 *
 * @param state: Pointer to AggregateState from which the serialized HLL/boolean data is extracted.
 * @param vectors: A vector containing BaseVector pointers, used to store the extracted serialized data or cardinality.
 * @param rowIndex: Row index in BaseVector, used to store the extracted data.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    const auto *aggState = ApproxCountDistinctAggState::ConstCastState(state + aggStateOffset);
    if (IsOutputPartial()) {
        // Partial output: write VARBINARY (serialized HLL or boolean [0,state])
        auto *outVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
        if (aggState->len <= 0) {
            outVec->SetNull(rowIndex);
        } else {
            char *buf = reinterpret_cast<char *>(aggState->serializePtr);
            std::string_view sv(buf, static_cast<size_t>(aggState->len));
        outVec->SetValue(rowIndex, sv);
    }
    } else {
        // Final/single-stage output: write BIGINT cardinality from serialized form
        auto *outVec = static_cast<Vector<int64_t> *>(vectors[0]);
        int64_t card = aggState->len >= 2 ?
            HllCardinalityFromSerialized(reinterpret_cast<const char *>(aggState->serializePtr), aggState->len) : 0;
        outVec->SetValue(rowIndex, card);
    }
}

/**
 * Batch version of ExtractValues for Partial or Final output. Used when multiple group states are written at once.
 *
 * @param groupStates: Pointers to AggregateState for each group.
 * @param vectors: BaseVector container to store extracted serialized data or cardinality.
 * @param rowOffset: Start row offset in the output vectors.
 * @param rowCount: Number of rows to extract.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (IsOutputPartial()) {
        auto *outVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
        for (int32_t i = 0; i < rowCount; ++i) {
            auto *s = ApproxCountDistinctAggState::CastState(groupStates[i] + aggStateOffset);
            if (s->len <= 0) {
                outVec->SetNull(i);
            } else {
                std::string_view sv(reinterpret_cast<const char *>(s->serializePtr), static_cast<size_t>(s->len));
                outVec->SetValue(i, sv);
            }
        }
    } else {
        auto *outVec = static_cast<Vector<int64_t> *>(vectors[0]);
        for (int32_t i = 0; i < rowCount; ++i) {
            auto *s = ApproxCountDistinctAggState::CastState(groupStates[i] + aggStateOffset);
            int64_t card = s->len >= 2 ?
                HllCardinalityFromSerialized(reinterpret_cast<const char *>(s->serializePtr), s->len) : 0;
            outVec->SetValue(i, card);
        }
    }
}

/**
 * Invoked when spilling group states to disk. Extracts serialized HLL/boolean data from each AggregateState
 * into the given vectors (VARBINARY) for persistence.
 *
 * @param groupStates: Pointers to AggregateState to spill.
 * @param vectors: BaseVector container to store serialized data (vectors[0] is VARBINARY).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto *outVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[0]);
    int32_t n = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < n; ++i) {
        auto *s = ApproxCountDistinctAggState::CastState(groupStates[i] + aggStateOffset);
        if (s->len <= 0) {
            outVec->SetNull(i);
        } else {
            std::string_view sv(reinterpret_cast<const char *>(s->serializePtr), static_cast<size_t>(s->len));
            outVec->SetValue(i, sv);
        }
    }
}

/**
 * Returns the data types used when spilling approx_count_distinct state (VARBINARY).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> ApproxCountDistinctAggregator<IN_ID, OUT_ID>::GetSpillType()
{
    std::vector<DataTypePtr> types;
    types.push_back(std::make_shared<DataType>(OMNI_VARBINARY));
    return types;
}

/**
 * Initialize a single AggregateState: allocate buffer and set len=0. No serialized state is written here;
 * the first ProcessPartialRaw (boolean or HLL Partial) or first HllMergeSerialized (Final) fills the buffer.
 *
 * @param state: Pointer to AggregateState to initialize; buffer is recorded in stateBufferPtrs_ for destructor.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state)
{
    auto *aggState = ApproxCountDistinctAggState::CastState(state + aggStateOffset);
    int32_t capacity = isBooleanInput_ ? kHllBooleanSerializedSize : kApproxCountDistinctMaxSerializedSize;
    char *buf = new char[capacity];
    stateBufferPtrs_.push_back(reinterpret_cast<int64_t>(buf));
    aggState->serializePtr = reinterpret_cast<int64_t>(buf);
    // len=0: boolean/Partial first ProcessPartialRaw writes [0,state]; Final first HllMergeSerialized copies first partial; other Partial builds HLL in buf.
    aggState->len = 0;
}

/**
 * Initialize multiple AggregateStates (one per group). Delegates to InitState for each.
 *
 * @param groupStates: Vector of AggregateState pointers to initialize.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (AggregateState *s : groupStates) {
        InitState(s);
    }
}

/**
 * Invoked by AddInput. Dispatches to Partial phase (ProcessPartialRaw) or Final phase (ProcessFinalMerge) based on IsInputRaw().
 *
 * @param state: Pointer to AggregateState (already offset by aggStateOffset when from TypedAggregator).
 * @param vector: Input BaseVector (raw column for Partial, VARBINARY for Final).
 * @param rowOffset: Start row offset in the input vector.
 * @param rowCount: Number of rows to process.
 * @param nullMap: Null bitmap; rows with null are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    if (IsInputRaw()) {
        ProcessPartialRaw(state, vector, rowOffset, rowCount, nullMap);
    } else {
        ProcessFinalMerge(state, vector, 0, rowCount, nullMap);
    }
}

/**
 * Partial phase: merge input raw values into HLL (or boolean state). State is at (base + aggStateOffset); used only for Partial or single-stage.
 * May read max standard error from second column on first row when input has two columns.
 *
 * @param state: Pointer to this aggregator's AggregateState (serializePtr + len).
 * @param vector: Input column (value type) or second column for double max error.
 * @param rowOffset: Start row in vector.
 * @param rowCount: Number of rows.
 * @param nullMap: Null bitmap; null rows are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessPartialRaw(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *aggState = ApproxCountDistinctAggState::CastState(state);
    char *buf = reinterpret_cast<char *>(aggState->serializePtr);
    int32_t capacity = isBooleanInput_ ? kHllBooleanSerializedSize : kApproxCountDistinctMaxSerializedSize;

    if (isBooleanInput_) {
        // Boolean: accumulate seen false/true in 2-byte state [0, state]
        int8_t st = aggState->len >= 2 ? HllBooleanDeserialize(buf) : 0;
        for (int32_t i = 0; i < rowCount; ++i) {
            const int32_t absIdx = rowOffset + i;
            if (nullMap && (*nullMap)[absIdx]) continue;
            bool val = VectorHelper::GetFlatValue<type::OMNI_BOOLEAN>(vector, absIdx);
            st |= (1 << (val ? 1 : 0));
        }
        HllBooleanSerialize(st, buf);
        aggState->len = kHllBooleanSerializedSize;
        return;
    }

    // Optional: read max standard error from second column (first row only)
    if (this->channels.size() >= 2 && !hasMaxErrorFromColumn_ && this->curVectorBatch != nullptr) {
        BaseVector *errVec = this->curVectorBatch->Get(this->channels[1]);
        if (errVec != nullptr && errVec->GetTypeId() == OMNI_DOUBLE && !errVec->IsNull(rowOffset)) {
            double maxErr = VectorHelper::GetFlatValue<type::OMNI_DOUBLE>(errVec, rowOffset);
            HllCheckMaxStandardError(maxErr);
            indexBitLength_ = HllToIndexBitLength(maxErr);
            hasMaxErrorFromColumn_ = true;
        }
    }

    HllAccumulator acc(indexBitLength_);
    if (aggState->len >= 2 && HllAccumulator::canDeserialize(buf, aggState->len)) {
        acc.mergeWith(buf, aggState->len);
    }

    // Flat / dictionary / const: same as max_by / approx_percentile value column.
    constexpr int32_t valueSize = (IN_ID == OMNI_LONG || IN_ID == OMNI_DOUBLE) ? 8
        : (IN_ID == OMNI_INT || IN_ID == OMNI_FLOAT) ? 4
        : (IN_ID == OMNI_SHORT) ? 2 : 1;
    for (int32_t i = 0; i < rowCount; ++i) {
        const int32_t absIdx = rowOffset + i;
        if (nullMap && (*nullMap)[absIdx]) continue;
        if constexpr (IN_ID == OMNI_VARCHAR || IN_ID == OMNI_CHAR || IN_ID == OMNI_VARBINARY) {
            std::string_view sv = VectorHelper::GetFlatValue<IN_ID>(vector, absIdx);
            acc.insertHash(static_cast<uint64_t>(HashUtil::HashValue(
                const_cast<int8_t *>(reinterpret_cast<const int8_t *>(sv.data())), static_cast<int32_t>(sv.size()))));
        } else {
            InType v = VectorHelper::GetFlatValue<IN_ID>(vector, absIdx);
            acc.insertHash(HllHashBytes(&v, valueSize));
        }
    }

    int32_t sz = acc.serializedSize();
    if (sz <= capacity) {
        acc.serialize(buf);
        aggState->len = sz;
    }
}

/**
 * Final phase: merge incoming serialized partial results (VARBINARY) into current state. Used when input is not raw.
 *
 * @param state: Pointer to this aggregator's AggregateState (current buffer and len).
 * @param vector: VARBINARY vector of serialized HLL or boolean partials.
 * @param rowOffset: Start row in vector.
 * @param rowCount: Number of rows to merge.
 * @param nullMap: Null bitmap; null rows are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessFinalMerge(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *aggState = ApproxCountDistinctAggState::CastState(state);
    char *buf = reinterpret_cast<char *>(aggState->serializePtr);
    int32_t capacity = kApproxCountDistinctMaxSerializedSize;
    int32_t *currentLen = &aggState->len;

    for (int32_t i = 0; i < rowCount; ++i) {
        int32_t row = rowOffset + i;
        if (nullMap && (*nullMap)[row]) {
            continue;
        }
        std::string_view sv = VectorHelper::GetFlatValue<type::OMNI_VARBINARY>(vector, row);
        if (sv.empty()) {
            continue;
        }
        HllMergeSerialized(buf, currentLen, capacity,
            sv.data(), static_cast<int32_t>(sv.size()));
    }
}

/**
 * Group aggregation: process one row per group. Dispatches to ProcessPartialRaw or ProcessFinalMerge per group state.
 *
 * @param rowStates: One AggregateState per group.
 * @param vector: Input column (raw or VARBINARY).
 * @param rowOffset: Start row offset.
 * @param nullMap: Null bitmap.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    int32_t n = static_cast<int32_t>(rowStates.size());
    if (IsInputRaw()) {
        for (int32_t i = 0; i < n; ++i) {
            std::shared_ptr<NullsHelper> rowNull = nullMap;
            ProcessPartialRaw(rowStates[i] + aggStateOffset, vector, rowOffset + i, 1, rowNull);
        }
    } else {
        for (int32_t i = 0; i < n; ++i) {
            ProcessFinalMerge(rowStates[i] + aggStateOffset, vector, rowOffset + i, 1, nullMap);
        }
    }
}

/**
 * Restore state from spilled VARBINARY rows: merge each row's serialized HLL into the corresponding group state.
 *
 * @param unspillRows: Rows read from spill (batch, state, rowIdx per row).
 * @param rowCount: Number of rows to process.
 * @param vectorIndex: Index of the VARBINARY column in row.batch; incremented on return.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
    int32_t rowCount, int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        BaseVector *varbinaryVec = row.batch->Get(firstVecIdx);
        auto *aggState = ApproxCountDistinctAggState::CastState(row.state + aggStateOffset);
        if (varbinaryVec->IsNull(row.rowIdx)) {
            continue;
        }
        std::string_view sv = VectorHelper::GetFlatValue<type::OMNI_VARBINARY>(varbinaryVec, row.rowIdx);
        if (sv.empty()) {
            continue;
        }
        char *buf = reinterpret_cast<char *>(aggState->serializePtr);
        int32_t *currentLen = &aggState->len;
        int32_t capacity = isBooleanInput_ ? kHllBooleanSerializedSize : kApproxCountDistinctMaxSerializedSize;
        HllMergeSerialized(buf, currentLen, capacity, sv.data(), static_cast<int32_t>(sv.size()));
    }
}

/**
 * Align aggregation schema: for Partial output, produce one VARBINARY row per input row (per-row HLL or boolean state).
 * Used when aggregator output schema does not match the expected partial schema (e.g. after filter or join).
 *
 * @param result: VectorBatch to append the aligned VARBINARY vector to.
 * @param originVector: Input column (value or dictionary).
 * @param nullMap: Null bitmap; null rows get null in output.
 * @param aggFilter: Whether aggregation filter is applied.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    if constexpr (OUT_ID != OMNI_VARBINARY) {
        std::string omniExceptionInfo = "ApproxCountDistinctAggregator ProcessAlignAggSchema only for partial (varbinary) output";
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }
    int rowCount = originVector->GetSize();
    const int32_t kAlignBufSize = 256;
    char alignBuf[kAlignBufSize];
    auto *outVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateFlatVector(this->pool_, OMNI_VARBINARY, rowCount, kAlignBufSize * rowCount));
    if (isBooleanInput_) {
        for (int32_t i = 0; i < rowCount; ++i) {
            if (nullMap != nullptr && (*nullMap)[i]) {
                outVec->SetNull(i);
            } else {
                bool val = VectorHelper::GetFlatValue<type::OMNI_BOOLEAN>(originVector, i);
                int8_t st = (1 << (val ? 1 : 0));
                HllBooleanSerialize(st, alignBuf);
                std::string_view svRef(alignBuf, kHllBooleanSerializedSize);
                outVec->SetValue(i, svRef);
            }
        }
    } else {
        constexpr int32_t alignValueSize = (IN_ID == OMNI_LONG || IN_ID == OMNI_DOUBLE) ? 8
            : (IN_ID == OMNI_INT || IN_ID == OMNI_FLOAT) ? 4
            : (IN_ID == OMNI_SHORT) ? 2 : 1;
        for (int32_t i = 0; i < rowCount; ++i) {
            if (nullMap != nullptr && (*nullMap)[i]) {
                outVec->SetNull(i);
            } else {
                uint64_t hash = 0;
                if constexpr (IN_ID == OMNI_VARCHAR || IN_ID == OMNI_CHAR || IN_ID == OMNI_VARBINARY) {
                    std::string_view sv = VectorHelper::GetFlatValue<IN_ID>(originVector, i);
                    hash = static_cast<uint64_t>(HashUtil::HashValue(
                        const_cast<int8_t *>(reinterpret_cast<const int8_t *>(sv.data())), static_cast<int32_t>(sv.size())));
                } else {
                    InType v = VectorHelper::GetFlatValue<IN_ID>(originVector, i);
                    hash = HllHashBytes(&v, alignValueSize);
                }
                HllAccumulator acc(indexBitLength_);
                acc.insertHash(hash);
                int32_t sz = acc.serializedSize();
                if (sz <= kAlignBufSize) {
                    acc.serialize(alignBuf);
                    std::string_view svRef(alignBuf, sz);
                    outVec->SetValue(i, svRef);
                }
            }
        }
    }
    result->Append(outVec);
}

/**
 * Factory: create ApproxCountDistinctAggregator if input/output types and arity match. Supports single column or
 * (value, double max_standard_error). Partial output must be VARBINARY; Final input VARBINARY and output BIGINT.
 *
 * @param inputTypes: One or two types (second must be OMNI_DOUBLE for max error).
 * @param outputTypes: VARBINARY for Partial, BIGINT for Final/single-stage.
 * @param channels: Column indices.
 * @param rawIn: True for Partial/single-stage (raw column input).
 * @param partialOut: True for Partial (VARBINARY output).
 * @param isOverflowAsNull: Unused for this aggregator.
 * @return Unique_ptr to aggregator or nullptr if types do not match.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::unique_ptr<Aggregator> ApproxCountDistinctAggregator<IN_ID, OUT_ID>::Create(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull)
{
    if (!TypedAggregator::CheckTypes("approx_count_distinct", inputTypes, outputTypes, IN_ID, OUT_ID)) {
        return nullptr;
    }
    if (inputTypes.GetSize() > 2) {
        return nullptr;
    }
    if (inputTypes.GetSize() == 2 && inputTypes.GetType(1)->GetId() != OMNI_DOUBLE) {
        return nullptr;  // second argument must be double (max standard error)
    }
    if (rawIn && partialOut && OUT_ID != OMNI_VARBINARY) {
        LogError("approx_count_distinct Partial expects OMNI_VARBINARY output");
        return nullptr;
    }
    if (rawIn && !partialOut && OUT_ID != OMNI_LONG) {
        LogError("approx_count_distinct single-stage expects OMNI_LONG output");
        return nullptr;
    }
    if (!rawIn && !partialOut && (IN_ID != OMNI_VARBINARY || OUT_ID != OMNI_LONG)) {
        LogError("approx_count_distinct Final expects OMNI_VARBINARY input and OMNI_LONG output");
        return nullptr;
    }
    return std::unique_ptr<ApproxCountDistinctAggregator<IN_ID, OUT_ID>>(
        new ApproxCountDistinctAggregator<IN_ID, OUT_ID>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull));
}

/**
 * Constructor. Sets boolean flag, default index bit length, and no max-error-from-column.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
ApproxCountDistinctAggregator<IN_ID, OUT_ID>::ApproxCountDistinctAggregator(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
    const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull),
      isBooleanInput_(IN_ID == OMNI_BOOLEAN),
      indexBitLength_(kDefaultHllIndexBitLength),
      hasMaxErrorFromColumn_(false)
{}

// Explicit instantiations: Partial (raw -> varbinary), Final (varbinary -> long), single-stage (raw -> long)
template class ApproxCountDistinctAggregator<OMNI_BOOLEAN, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_BOOLEAN, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_BYTE, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_BYTE, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_SHORT, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_SHORT, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_LONG, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_LONG, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_INT, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_INT, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_FLOAT, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_FLOAT, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_DOUBLE, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_DOUBLE, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_VARCHAR, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_VARCHAR, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_CHAR, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_CHAR, OMNI_LONG>;
template class ApproxCountDistinctAggregator<OMNI_VARBINARY, OMNI_VARBINARY>;
template class ApproxCountDistinctAggregator<OMNI_VARBINARY, OMNI_LONG>;

}  // namespace op
}  // namespace omniruntime
