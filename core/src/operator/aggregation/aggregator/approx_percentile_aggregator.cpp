/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ApproxPercentile aggregate implementation (KLL sketch). Reference: approx_count_distinct.
 * - Partial:      accumulates raw values into KLL sketch, reads percentile/accuracy from curVectorBatch; output VARBINARY.
 * - PartialMerge: merges VARBINARY partials into accumulator, re-serializes to VARBINARY on output.
 * - Final:        merges VARBINARY partials, then estimateQuantile for each requested percentile; output scalar or ARRAY.
 * - Complete:     accumulates raw values, then estimateQuantile for each requested percentile; output scalar or ARRAY.
 */
#include "approx_percentile_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include <cmath>

namespace omniruntime {
namespace op {

using namespace omniruntime::type;
using vec::VectorHelper;

namespace {

constexpr double kDefaultAccuracy = 0.01;

/** Partial serialization header: [valueTypeId:int32][numPercentiles:int32][percentiles:double*][accuracy:double], then KLL bytes. */
inline void WriteHeader(char* out, size_t& off, type::DataTypeId valueTypeId, const std::vector<double>& percentiles,
    double accuracy) {
    *reinterpret_cast<int32_t*>(out + off) = static_cast<int32_t>(valueTypeId);
    off += sizeof(int32_t);
    *reinterpret_cast<int32_t*>(out + off) = static_cast<int32_t>(percentiles.size());
    off += sizeof(int32_t);
    for (double percentile : percentiles) {
        *reinterpret_cast<double*>(out + off) = percentile;
        off += sizeof(double);
    }
    *reinterpret_cast<double*>(out + off) = accuracy;
    off += sizeof(double);
}
/** Reads header from serialized partial; advances off to start of KLL bytes. */
inline void ReadHeader(const char* data, size_t& off, type::DataTypeId& valueTypeId, std::vector<double>& percentiles,
    double& accuracy) {
    valueTypeId = static_cast<type::DataTypeId>(*reinterpret_cast<const int32_t*>(data + off));
    off += sizeof(int32_t);
    int32_t numPercentiles = *reinterpret_cast<const int32_t*>(data + off);
    off += sizeof(int32_t);
    percentiles.resize(numPercentiles);
    for (int32_t idx = 0; idx < numPercentiles; ++idx) {
        percentiles[idx] = *reinterpret_cast<const double*>(data + off);
        off += sizeof(double);
    }
    accuracy = *reinterpret_cast<const double*>(data + off);
    off += sizeof(double);
}

/**
 * Single accumulator for one group: type-erased KLL sketch (void* + valueTypeId), percentiles, accuracy.
 * Used in both Partial (insert values) and Final (merge deserialized, then estimateQuantile). Reference: Velox KllSketchAccumulator.
 */
struct ApproxPercentileAccumulator {
    type::DataTypeId valueTypeId{ type::OMNI_INVALID };
    std::vector<double> percentiles;
    double accuracy{ kDefaultAccuracy };
    void* sketch{ nullptr };

    ~ApproxPercentileAccumulator() { deleteSketch(); }

    void deleteSketch() {
        if (!sketch) return;
        switch (valueTypeId) {
            case OMNI_BYTE: delete reinterpret_cast<kll::KllSketch<int8_t>*>(sketch); break;
            case OMNI_SHORT: delete reinterpret_cast<kll::KllSketch<int16_t>*>(sketch); break;
            case OMNI_INT: delete reinterpret_cast<kll::KllSketch<int32_t>*>(sketch); break;
            case OMNI_LONG: delete reinterpret_cast<kll::KllSketch<int64_t>*>(sketch); break;
            case OMNI_FLOAT: delete reinterpret_cast<kll::KllSketch<float>*>(sketch); break;
            case OMNI_DOUBLE: delete reinterpret_cast<kll::KllSketch<double>*>(sketch); break;
            default: break;
        }
        sketch = nullptr;
    }

    void ensureSketch(type::DataTypeId typeId, uint32_t k = kll::kDefaultK) {
        if (sketch && valueTypeId == typeId) return;
        deleteSketch();
        valueTypeId = typeId;
        switch (typeId) {
            case type::OMNI_BYTE: sketch = new kll::KllSketch<int8_t>(k); break;
            case type::OMNI_SHORT: sketch = new kll::KllSketch<int16_t>(k); break;
            case type::OMNI_INT: sketch = new kll::KllSketch<int32_t>(k); break;
            case type::OMNI_LONG: sketch = new kll::KllSketch<int64_t>(k); break;
            case type::OMNI_FLOAT: sketch = new kll::KllSketch<float>(k); break;
            case type::OMNI_DOUBLE: sketch = new kll::KllSketch<double>(k); break;
            default: throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile unsupported value type");
        }
    }

    void setK(uint32_t k) {
        if (!sketch) return;
        switch (valueTypeId) {
            case type::OMNI_BYTE: reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->setK(k); break;
            case type::OMNI_SHORT: reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->setK(k); break;
            case type::OMNI_INT: reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->setK(k); break;
            case type::OMNI_LONG: reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->setK(k); break;
            case type::OMNI_FLOAT: reinterpret_cast<kll::KllSketch<float>*>(sketch)->setK(k); break;
            case type::OMNI_DOUBLE: reinterpret_cast<kll::KllSketch<double>*>(sketch)->setK(k); break;
            default: break;
        }
    }

    void insert(type::DataTypeId typeId, const void* valuePtr) {
        switch (typeId) {
            case type::OMNI_BYTE: reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->insert(*reinterpret_cast<const int8_t*>(valuePtr)); break;
            case type::OMNI_SHORT: reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->insert(*reinterpret_cast<const int16_t*>(valuePtr)); break;
            case type::OMNI_INT: reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->insert(*reinterpret_cast<const int32_t*>(valuePtr)); break;
            case type::OMNI_LONG: reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->insert(*reinterpret_cast<const int64_t*>(valuePtr)); break;
            case type::OMNI_FLOAT: reinterpret_cast<kll::KllSketch<float>*>(sketch)->insert(*reinterpret_cast<const float*>(valuePtr)); break;
            case type::OMNI_DOUBLE: reinterpret_cast<kll::KllSketch<double>*>(sketch)->insert(*reinterpret_cast<const double*>(valuePtr)); break;
            default: break;
        }
    }

    size_t serializedByteSize() const {
        if (!sketch) return 0;
        size_t headerSize = sizeof(int32_t) * 2 + sizeof(double) * (1 + percentiles.size());
        switch (valueTypeId) {
            case type::OMNI_BYTE: { kll::KllSketch<int8_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            case type::OMNI_SHORT: { kll::KllSketch<int16_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            case type::OMNI_INT: { kll::KllSketch<int32_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            case type::OMNI_LONG: { kll::KllSketch<int64_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            case type::OMNI_FLOAT: { kll::KllSketch<float> sketchCopy(*reinterpret_cast<kll::KllSketch<float>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            case type::OMNI_DOUBLE: { kll::KllSketch<double> sketchCopy(*reinterpret_cast<kll::KllSketch<double>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); return headerSize + sketchCopy.serializedByteSize(); }
            default: return headerSize;
        }
    }

    void serialize(char* out) const {
        if (!sketch) return;
        size_t writeOffset = 0;
        WriteHeader(out, writeOffset, valueTypeId, percentiles, accuracy);
        switch (valueTypeId) {
            case type::OMNI_BYTE: { kll::KllSketch<int8_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            case type::OMNI_SHORT: { kll::KllSketch<int16_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            case type::OMNI_INT: { kll::KllSketch<int32_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            case type::OMNI_LONG: { kll::KllSketch<int64_t> sketchCopy(*reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            case type::OMNI_FLOAT: { kll::KllSketch<float> sketchCopy(*reinterpret_cast<kll::KllSketch<float>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            case type::OMNI_DOUBLE: { kll::KllSketch<double> sketchCopy(*reinterpret_cast<kll::KllSketch<double>*>(sketch)); sketchCopy.finish(); sketchCopy.compact(); sketchCopy.serialize(out + writeOffset); break; }
            default: break;
        }
    }

    void mergeDeserialized(const char* data) {
        size_t readOffset = 0;
        DataTypeId mergedValueTypeId;
        std::vector<double> mergedPercentiles;
        double mergedAccuracy;
        ReadHeader(data, readOffset, mergedValueTypeId, mergedPercentiles, mergedAccuracy);
        uint32_t k = kll::kDefaultK;
        if (std::isfinite(mergedAccuracy) && mergedAccuracy > 0) {
            double eps = (mergedAccuracy >= 1.0) ? (1.0 / mergedAccuracy) : mergedAccuracy;
            if (eps > 0.0 && eps < 1.0) k = kll::kFromEpsilon(eps);
        }
        ensureSketch(mergedValueTypeId, k);
        if (percentiles.empty()) {
            percentiles = mergedPercentiles;
            accuracy = mergedAccuracy;
        }
        switch (valueTypeId) {
            case type::OMNI_BYTE: reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->mergeDeserialized(data + readOffset); break;
            case type::OMNI_SHORT: reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->mergeDeserialized(data + readOffset); break;
            case type::OMNI_INT: reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->mergeDeserialized(data + readOffset); break;
            case type::OMNI_LONG: reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->mergeDeserialized(data + readOffset); break;
            case type::OMNI_FLOAT: reinterpret_cast<kll::KllSketch<float>*>(sketch)->mergeDeserialized(data + readOffset); break;
            case type::OMNI_DOUBLE: reinterpret_cast<kll::KllSketch<double>*>(sketch)->mergeDeserialized(data + readOffset); break;
            default: break;
        }
    }

    size_t estimateQuantile(double q, void* out) {
        switch (valueTypeId) {
            case type::OMNI_BYTE: *reinterpret_cast<int8_t*>(out) = reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->estimateQuantile(q); return sizeof(int8_t);
            case type::OMNI_SHORT: *reinterpret_cast<int16_t*>(out) = reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->estimateQuantile(q); return sizeof(int16_t);
            case type::OMNI_INT: *reinterpret_cast<int32_t*>(out) = reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->estimateQuantile(q); return sizeof(int32_t);
            case type::OMNI_LONG: *reinterpret_cast<int64_t*>(out) = reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->estimateQuantile(q); return sizeof(int64_t);
            case type::OMNI_FLOAT: *reinterpret_cast<float*>(out) = reinterpret_cast<kll::KllSketch<float>*>(sketch)->estimateQuantile(q); return sizeof(float);
            case type::OMNI_DOUBLE: *reinterpret_cast<double*>(out) = reinterpret_cast<kll::KllSketch<double>*>(sketch)->estimateQuantile(q); return sizeof(double);
            default: return 0;
        }
    }

    void finishAndCompact() {
        switch (valueTypeId) {
            case type::OMNI_BYTE: reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->compact(); break;
            case type::OMNI_SHORT: reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->compact(); break;
            case type::OMNI_INT: reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->compact(); break;
            case type::OMNI_LONG: reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->compact(); break;
            case type::OMNI_FLOAT: reinterpret_cast<kll::KllSketch<float>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<float>*>(sketch)->compact(); break;
            case type::OMNI_DOUBLE: reinterpret_cast<kll::KllSketch<double>*>(sketch)->finish(); reinterpret_cast<kll::KllSketch<double>*>(sketch)->compact(); break;
            default: break;
        }
    }

    bool empty() const {
        if (!sketch) return true;
        switch (valueTypeId) {
            case type::OMNI_BYTE: return reinterpret_cast<kll::KllSketch<int8_t>*>(sketch)->totalCount() == 0;
            case type::OMNI_SHORT: return reinterpret_cast<kll::KllSketch<int16_t>*>(sketch)->totalCount() == 0;
            case type::OMNI_INT: return reinterpret_cast<kll::KllSketch<int32_t>*>(sketch)->totalCount() == 0;
            case type::OMNI_LONG: return reinterpret_cast<kll::KllSketch<int64_t>*>(sketch)->totalCount() == 0;
            case type::OMNI_FLOAT: return reinterpret_cast<kll::KllSketch<float>*>(sketch)->totalCount() == 0;
            case type::OMNI_DOUBLE: return reinterpret_cast<kll::KllSketch<double>*>(sketch)->totalCount() == 0;
            default: return true;
        }
    }
};

}  // namespace

/**
 * Destructor. Frees all ApproxPercentileAccumulator instances recorded in stateAccumulatorPtrs_.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
ApproxPercentileAggregator<IN_ID, OUT_ID>::~ApproxPercentileAggregator() {
    for (int64_t accPtr : stateAccumulatorPtrs_) {
        if (accPtr != 0) delete reinterpret_cast<ApproxPercentileAccumulator*>(accPtr);
    }
    stateAccumulatorPtrs_.clear();
}

/**
 * Initialize a single AggregateState: allocate ApproxPercentileAccumulator; for Partial, ensure KLL sketch with default accuracy.
 * Accumulator pointer is stored in state and in stateAccumulatorPtrs_ for destructor.
 *
 * @param state Pointer to AggregateState to initialize.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::InitState(AggregateState* state) {
    auto* aggState = ApproxPercentileAggState::CastState(state + aggStateOffset);
    auto* acc = new ApproxPercentileAccumulator();
    // Partial (raw->partial) and Complete (raw->final) both accumulate raw values, so ensure a sketch up front.
    // PartialMerge / Final (input is VARBINARY partial) create the sketch lazily inside mergeDeserialized.
    if (IsInputRaw())
        acc->ensureSketch(IN_ID, kll::kFromEpsilon(kDefaultAccuracy));
    stateAccumulatorPtrs_.push_back(reinterpret_cast<int64_t>(acc));
    aggState->accumulatorPtr = reinterpret_cast<int64_t>(acc);
}

/**
 * Initialize multiple AggregateStates (one per group). Delegates to InitState for each.
 *
 * @param groupStates Vector of AggregateState pointers to initialize.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState*>& groupStates) {
    for (AggregateState* s : groupStates) InitState(s);
}

/**
 * Invoked by GetOutput: extracts from AggregateState either serialized KLL (Partial -> VARBINARY) or
 * estimated percentile value(s) (Final -> scalar or ARRAY<value_type>).
 *
 * @param state   Pointer to AggregateState (offset by aggStateOffset).
 * @param vectors vectors[0] is output (VARBINARY for partial, scalar/ARRAY for final).
 * @param rowIndex Row index in output vector.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState* state,
    std::vector<BaseVector*>& vectors, int32_t rowIndex) {
    const auto* aggState = ApproxPercentileAggState::ConstCastState(state + aggStateOffset);
    int64_t ptr = aggState->accumulatorPtr;
    if (ptr == 0) {
        if (IsOutputPartial()) {
            static_cast<Vector<LargeStringContainer<std::string_view>>*>(vectors[0])->SetNull(rowIndex);
        } else if (vectors[0]->GetTypeId() == type::OMNI_ARRAY) {
            static_cast<omniruntime::vec::ArrayVector*>(vectors[0])->SetNull(rowIndex);
        } else {
            static_cast<Vector<OutType>*>(vectors[0])->SetNull(rowIndex);
        }
        return;
    }
    if (IsOutputPartial()) {
        auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(ptr);
        size_t serializedSize = acc->serializedByteSize();
        std::vector<char> buf(serializedSize);
        acc->serialize(buf.data());
        std::string_view sv(buf.data(), serializedSize);
        static_cast<Vector<LargeStringContainer<std::string_view>>*>(vectors[0])->SetValue(rowIndex, sv);
    } else {
        auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(ptr);
        if (acc->empty() || acc->percentiles.empty()) {
            if (vectors[0]->GetTypeId() == type::OMNI_ARRAY) {
                static_cast<omniruntime::vec::ArrayVector*>(vectors[0])->SetNull(rowIndex);
            } else {
                static_cast<Vector<OutType>*>(vectors[0])->SetNull(rowIndex);
            }
            return;
        }
        acc->finishAndCompact();
        if (vectors[0]->GetTypeId() == type::OMNI_ARRAY) {
            auto* arrayVec = static_cast<omniruntime::vec::ArrayVector*>(vectors[0]);
            size_t numPercentiles = acc->percentiles.size();
            BaseVector* elemVec = new Vector<OutType>(static_cast<int32_t>(numPercentiles));
            for (size_t pctIdx = 0; pctIdx < numPercentiles; ++pctIdx) {
                OutType val;
                acc->estimateQuantile(acc->percentiles[pctIdx], &val);
                static_cast<Vector<OutType>*>(elemVec)->SetValue(static_cast<int32_t>(pctIdx), val);
            }
            arrayVec->SetValue(rowIndex, elemVec);
            delete elemVec;
        } else {
            double quantile = acc->percentiles[0];
            OutType resultValue;
            acc->estimateQuantile(quantile, &resultValue);
            static_cast<Vector<OutType>*>(vectors[0])->SetValue(rowIndex, resultValue);
        }
    }
}

/**
 * Batch version of ExtractValues for multiple groups. Used when writing group aggregation output.
 *
 * @param groupStates Pointers to AggregateState for each group.
 * @param vectors     Output vectors (same semantics as ExtractValues).
 * @param rowOffset   Start row in output vectors.
 * @param rowCount    Number of rows to extract.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState*>& groupStates,
    std::vector<BaseVector*>& vectors, int32_t rowOffset, int32_t rowCount) {
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        ExtractValues(groupStates[rowIdx], vectors, rowOffset + rowIdx);
    }
}

/**
 * Invoked when spilling group states: serializes each group's KLL sketch into VARBINARY (vectors[0]).
 *
 * @param groupStates Pointers to AggregateState to spill.
 * @param vectors     vectors[0] is VARBINARY output for serialized partials.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState*>& groupStates,
    std::vector<BaseVector*>& vectors) {
    auto* outVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vectors[0]);
    for (size_t groupIdx = 0; groupIdx < groupStates.size(); ++groupIdx) {
        const auto* aggState = ApproxPercentileAggState::ConstCastState(groupStates[groupIdx] + aggStateOffset);
        int64_t accPtr = aggState->accumulatorPtr;
        if (accPtr == 0) {
            outVec->SetNull(static_cast<int32_t>(groupIdx));
            continue;
        }
        auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(accPtr);
        if (acc->empty()) {
            outVec->SetNull(static_cast<int32_t>(groupIdx));
            continue;
        }
        size_t serializedSize = acc->serializedByteSize();
        std::vector<char> buf(serializedSize);
        acc->serialize(buf.data());
        outVec->SetValue(static_cast<int32_t>(groupIdx), std::string_view(buf.data(), serializedSize));
    }
}

/**
 * Returns the data type used when spilling approx_percentile state (VARBINARY).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> ApproxPercentileAggregator<IN_ID, OUT_ID>::GetSpillType() {
    return { std::make_shared<type::DataType>(type::OMNI_VARBINARY) };
}

/**
 * Invoked by AddInput. Dispatches to Partial (ProcessPartialRaw) or Final (ProcessFinalMerge) based on IsInputRaw().
 *
 * @param state     AggregateState for this group (already offset by aggStateOffset when from TypedAggregator).
 * @param vector    Input: value column for Partial, VARBINARY for Final.
 * @param rowOffset Start row in input vector.
 * @param rowCount  Number of rows to process.
 * @param nullMap   Null bitmap; null rows are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState* state, BaseVector* vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    if (IsInputRaw()) ProcessPartialRaw(state, vector, rowOffset, rowCount, nullMap);
    else ProcessFinalMerge(state, vector, 0, rowCount, nullMap);
}

/**
 * Partial phase: merge input values into group's KLL sketch. Reads percentile (and optionally accuracy) from
 * curVectorBatch via percentileChannel_/accuracyChannel_. Value column and accuracy/percentile scalars use
 * VectorHelper::GetFlatValue so flat, dictionary, and const encodings are supported consistently.
 *
 * @param state     This aggregator's AggregateState (accumulatorPtr).
 * @param vector    Value column (IN_ID); percentile/accuracy from curVectorBatch.
 * @param rowOffset Start row in batch.
 * @param rowCount  Number of rows.
 * @param nullMap   Null bitmap; null value rows are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessPartialRaw(AggregateState* state, BaseVector* vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    // Final-merge instantiations use IN_ID == OMNI_VARBINARY; do not compile GetFlatValue<IN_ID> for that case.
    if constexpr (IN_ID != type::OMNI_VARBINARY) {
        auto* aggState = ApproxPercentileAggState::CastState(state);
        auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(aggState->accumulatorPtr);
        if (!acc || !acc->sketch) return;

        if (curVectorBatch && percentileChannel_ >= 0) {
            BaseVector* pVec = curVectorBatch->Get(percentileChannel_);
            if (pVec && !pVec->IsNull(rowOffset)) {
                if (pVec->GetTypeId() == type::OMNI_DOUBLE) {
                    if (acc->percentiles.empty()) {
                        acc->percentiles.push_back(VectorHelper::GetFlatValue<type::OMNI_DOUBLE>(pVec, rowOffset));
                    }
                } else if (pVec->GetTypeId() == type::OMNI_ARRAY) {
                    auto* arrVec = reinterpret_cast<omniruntime::vec::ArrayVector*>(pVec);
                    std::shared_ptr<BaseVector> elemVec = arrVec->GetArrayAt(static_cast<int64_t>(rowOffset), false);
                    if (elemVec && elemVec->GetTypeId() == type::OMNI_DOUBLE && acc->percentiles.empty()) {
                        int64_t arraySize = elemVec->GetSize();
                        for (int64_t elemIdx = 0; elemIdx < arraySize; ++elemIdx) {
                            const int32_t ei = static_cast<int32_t>(elemIdx);
                            if (!elemVec->IsNull(ei)) {
                                acc->percentiles.push_back(
                                    VectorHelper::GetFlatValue<type::OMNI_DOUBLE>(elemVec.get(), ei));
                            }
                        }
                    }
                }
            }
            if (accuracyChannel_ >= 0) {
                BaseVector* aVec = curVectorBatch->Get(accuracyChannel_);
                if (aVec && !aVec->IsNull(rowOffset)) {
                    double accVal = 0;
                    // Spark passes accuracy as INT or LONG; reading LONG as INT (or vice versa) gives wrong value
                    if (aVec->GetTypeId() == type::OMNI_INT) {
                        accVal = static_cast<double>(VectorHelper::GetFlatValue<type::OMNI_INT>(aVec, rowOffset));
                    } else if (aVec->GetTypeId() == type::OMNI_LONG) {
                        accVal = static_cast<double>(VectorHelper::GetFlatValue<type::OMNI_LONG>(aVec, rowOffset));
                    }
                    if (std::isfinite(accVal) && accVal > 0 && acc->empty()) {
                        acc->accuracy = accVal;
                        double eps = (accVal >= 1.0) ? (1.0 / accVal) : accVal;
                        if (eps > 0.0 && eps < 1.0) acc->setK(kll::kFromEpsilon(eps));
                    }
                }
            }
        }
        if (acc->percentiles.empty()) acc->percentiles.push_back(0.5);

        // Flat / dictionary / const: VectorHelper::GetFlatValue dispatches encoding (same pattern as max_by col2).
        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            const int32_t absIdx = rowOffset + rowIdx;
            if (nullMap && (*nullMap)[absIdx]) continue;
            InType v = VectorHelper::GetFlatValue<IN_ID>(vector, absIdx);
            acc->insert(IN_ID, &v);
        }
    }
}

/**
 * Final phase: merge VARBINARY partial sketches into this group's accumulator (mergeDeserialized).
 *
 * @param state     This aggregator's AggregateState (accumulatorPtr).
 * @param vector    VARBINARY column of serialized partials.
 * @param rowOffset Start row.
 * @param rowCount  Number of rows to merge.
 * @param nullMap   Null bitmap; null rows are skipped.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessFinalMerge(AggregateState* state, BaseVector* vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    auto* aggState = ApproxPercentileAggState::CastState(state);
    auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(aggState->accumulatorPtr);
    if (!acc) return;

    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        int32_t row = rowOffset + rowIdx;
        if (nullMap && (*nullMap)[row]) continue;
        std::string_view sv = VectorHelper::GetFlatValue<type::OMNI_VARBINARY>(vector, row);
        if (sv.empty()) continue;
        acc->mergeDeserialized(sv.data());
    }
}

/**
 * Group aggregation path: for each row, dispatch to ProcessPartialRaw or ProcessFinalMerge (one row per group).
 *
 * @param rowStates One state per row in the current batch (hash-aggregation group mapping).
 * @param vector    Value column (Partial) or VARBINARY (Final).
 * @param rowOffset Start row.
 * @param nullMap   Null bitmap.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState*>& rowStates,
    BaseVector* vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
    int32_t numRows = static_cast<int32_t>(rowStates.size());
    if (IsInputRaw()) {
        for (int32_t rowIdx = 0; rowIdx < numRows; ++rowIdx) {
            std::shared_ptr<NullsHelper> rowNull = nullMap;
            ProcessPartialRaw(rowStates[rowIdx] + aggStateOffset, vector, rowOffset + rowIdx, 1, rowNull);
        }
    } else {
        for (int32_t rowIdx = 0; rowIdx < numRows; ++rowIdx) {
            ProcessFinalMerge(rowStates[rowIdx] + aggStateOffset, vector, rowOffset + rowIdx, 1, nullMap);
        }
    }
}

/**
 * After unspill: merge VARBINARY partials from unspill batch into each row's accumulator.
 *
 * @param unspillRows Per-row state and batch/rowIdx for reading VARBINARY.
 * @param rowCount    Number of unspill rows.
 * @param vectorIndex Index of VARBINARY column in batch; incremented on return.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo>& unspillRows,
    int32_t rowCount, int32_t& vectorIndex) {
    int32_t vecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        auto& unspillRow = unspillRows[rowIdx];
        BaseVector* varbinaryVec = unspillRow.batch->Get(vecIdx);
        auto* aggState = ApproxPercentileAggState::CastState(unspillRow.state + aggStateOffset);
        if (varbinaryVec->IsNull(unspillRow.rowIdx)) continue;
        std::string_view sv = VectorHelper::GetFlatValue<type::OMNI_VARBINARY>(varbinaryVec, unspillRow.rowIdx);
        if (sv.empty()) continue;
        auto* acc = reinterpret_cast<ApproxPercentileAccumulator*>(aggState->accumulatorPtr);
        if (acc) acc->mergeDeserialized(sv.data());
    }
}

/**
 * Adaptive partial aggregation: convert each row of the value column into a single-value serialized KLL (VARBINARY),
 * so output schema is (group cols, VARBINARY) with same row count. Only for Partial (OUT_ID == VARBINARY), raw input.
 *
 * @param result       Output batch; group columns already appended, we append VARBINARY column.
 * @param originVector Value column (first agg input channel).
 * @param nullMap      Null bitmap; non-null rows get serialized sketch, null rows get null VARBINARY.
 * @param aggFilter    Unused.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void ApproxPercentileAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch* result, BaseVector* originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) {
    if (OUT_ID != type::OMNI_VARBINARY)
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile ProcessAlignAggSchema only for partial");
    if constexpr (IN_ID == type::OMNI_VARBINARY)
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile ProcessAlignAggSchema not for varbinary input");
    if constexpr (IN_ID != type::OMNI_VARBINARY) {
        int32_t rowCount = originVector->GetSize();
        auto* outVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>>*>(
            VectorHelper::CreateFlatVector(type::OMNI_VARBINARY, rowCount, 4096 * static_cast<size_t>(rowCount)));
        for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
            if (nullMap && (*nullMap)[rowIdx]) {
                outVec->SetNull(rowIdx);
                continue;
            }
            ApproxPercentileAccumulator acc;
            acc.ensureSketch(IN_ID);
            acc.percentiles.push_back(0.5);
            InType value = VectorHelper::GetFlatValue<IN_ID>(originVector, rowIdx);
            acc.insert(IN_ID, &value);
            size_t serializedSize = acc.serializedByteSize();
            std::vector<char> buf(serializedSize);
            acc.serialize(buf.data());
            outVec->SetValue(rowIdx, std::string_view(buf.data(), serializedSize));
        }
        result->Append(outVec);
    }
}

/**
 * Factory: creates ApproxPercentileAggregator if input/output types match IN_ID/OUT_ID (for Partial or Final).
 *
 * @param inputTypes  Partial: 2–4 columns (value, percentile [, weight] [, accuracy]); Final: 1 column (VARBINARY).
 * @param outputTypes Partial: VARBINARY; Final: scalar or ARRAY<element_type>.
 * @return New aggregator instance or nullptr if type check fails.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::unique_ptr<Aggregator> ApproxPercentileAggregator<IN_ID, OUT_ID>::Create(const DataTypes& inputTypes,
    const DataTypes& outputTypes, std::vector<int32_t>& channels, bool rawIn, bool partialOut, bool isOverflowAsNull) {
    type::DataTypeId outTypeId = outputTypes.GetType(0)->GetId();
    if (outTypeId == type::OMNI_ARRAY) {
        type::DataTypeId elemId = outputTypes.GetType(0)->asArray().ElementType()->GetId();
        if (!TypedAggregator::CheckType(inputTypes.GetType(0)->GetId(), IN_ID) ||
            !TypedAggregator::CheckType(elemId, OUT_ID)) {
            return nullptr;
        }
    } else if (!TypedAggregator::CheckTypes("approx_percentile", inputTypes, outputTypes, IN_ID, OUT_ID)) {
        return nullptr;
    }
    return std::unique_ptr<ApproxPercentileAggregator<IN_ID, OUT_ID>>(
        new ApproxPercentileAggregator<IN_ID, OUT_ID>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull));
}

/**
 * Constructor. Resolves channel indices for percentile (and optionally weight, accuracy) from input column count and types.
 * - 2 cols: percentile = channels[1].
 * - 3 cols: if second is LONG then weight=channels[1], percentile=channels[2]; else percentile=channels[1], accuracy=channels[2].
 * - 4 cols: weight=channels[1], percentile=channels[2], accuracy=channels[3].
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
ApproxPercentileAggregator<IN_ID, OUT_ID>::ApproxPercentileAggregator(const DataTypes& inputTypes,
    const DataTypes& outputTypes, std::vector<int32_t>& channels, const bool inputRaw, const bool outputPartial,
    const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE, inputTypes, outputTypes, channels,
        inputRaw, outputPartial, isOverflowAsNull),
      percentileChannel_(-1), weightChannel_(-1), accuracyChannel_(-1) {
    size_t numInputCols = inputTypes.GetSize();
    /* Merge path (Final): single VARBINARY column; Raw path (Partial): 2-4 columns (value, percentile[, weight][, accuracy]). */
    if (inputRaw && (numInputCols < 2 || numInputCols > 4))
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile requires 2-4 input columns");
    if (!inputRaw && numInputCols != 1)
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile merge requires 1 column (partial)");
    if (numInputCols == 1) {
        return;  // merge path: percentile/weight/accuracy in serialized sketch
    }
    if (numInputCols == 2) {
        percentileChannel_ = channels[1];
    } else if (numInputCols == 3) {
        type::DataTypeId secondColTypeId = inputTypes.GetType(1)->GetId();
        if (secondColTypeId == type::OMNI_LONG) {
            weightChannel_ = channels[1];
            percentileChannel_ = channels[2];
        } else {
            percentileChannel_ = channels[1];
            accuracyChannel_ = channels[2];
        }
    } else {
        weightChannel_ = channels[1];
        percentileChannel_ = channels[2];
        accuracyChannel_ = channels[3];
    }
}

/**
 * Factory entry: selects ApproxPercentileAggregator<IN, OUT> by stage (inputRaw/outputPartial) and value/output type.
 *
 * Partial:      inputRaw=true,  outputPartial=true;  value type -> VARBINARY.
 * PartialMerge: inputRaw=false, outputPartial=true;  VARBINARY -> VARBINARY (merge serialized partials, re-serialize).
 * Final:        inputRaw=false, outputPartial=false; VARBINARY -> scalar or ARRAY<value_type>.
 * Complete:     inputRaw=true,  outputPartial=false; value type -> scalar or ARRAY<value_type> (single-stage, no shuffle).
 */
std::unique_ptr<Aggregator> ApproxPercentileAggregatorFactory::CreateAggregator(const DataTypes& inputTypes,
    const DataTypes& outputTypes, std::vector<int32_t>& channels, bool inputRaw, bool outputPartial,
    bool isOverflowAsNull) {
    size_t numInputCols = inputTypes.GetSize();
    /* Merge path (Final): 1 column (VARBINARY partial). Raw path (Partial): 2-4 columns. */
    if (inputRaw && (numInputCols < 2 || numInputCols > 4))
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile requires 2-4 columns");
    if (!inputRaw && numInputCols != 1)
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile merge requires 1 column (partial)");
    type::DataTypeId valueTypeId = inputTypes.GetType(0)->GetId();
    type::DataTypeId outTypeId = outputTypes.GetType(0)->GetId();

    // PartialMerge: VARBINARY partial -> VARBINARY partial (shuffle-merge stage between Partial and Final).
    if (!inputRaw && outputPartial) {
        if (valueTypeId != type::OMNI_VARBINARY || outTypeId != type::OMNI_VARBINARY)
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "ApproxPercentile partialMerge requires VARBINARY input/output");
        return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes,
            channels, inputRaw, outputPartial, isOverflowAsNull);
    }

    if (inputRaw && outputPartial) {
        switch (valueTypeId) {
            case type::OMNI_BYTE: return ApproxPercentileAggregator<type::OMNI_BYTE, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_SHORT: return ApproxPercentileAggregator<type::OMNI_SHORT, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_INT: return ApproxPercentileAggregator<type::OMNI_INT, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_LONG: return ApproxPercentileAggregator<type::OMNI_LONG, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_FLOAT: return ApproxPercentileAggregator<type::OMNI_FLOAT, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_DOUBLE: return ApproxPercentileAggregator<type::OMNI_DOUBLE, type::OMNI_VARBINARY>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            default: throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile value type must be BYTE/SHORT/INT/LONG/FLOAT/DOUBLE");
        }
    }
    if (!inputRaw && !outputPartial) {
        if (outTypeId == type::OMNI_ARRAY) {
            type::DataTypeId elemId = outputTypes.GetType(0)->asArray().ElementType()->GetId();
            switch (elemId) {
                case type::OMNI_BYTE: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_BYTE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_SHORT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_SHORT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_INT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_INT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_LONG: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_LONG>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_FLOAT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_FLOAT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_DOUBLE: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                default: throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile array output element type must be BYTE/SHORT/INT/LONG/FLOAT/DOUBLE");
            }
        }
        if (outTypeId != type::OMNI_VARBINARY) {
            switch (outTypeId) {
                case type::OMNI_BYTE: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_BYTE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_SHORT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_SHORT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_INT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_INT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_LONG: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_LONG>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_FLOAT: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_FLOAT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                case type::OMNI_DOUBLE: return ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
                default: throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile final output type must be BYTE/SHORT/INT/LONG/FLOAT/DOUBLE");
            }
        }
    }
    // Complete: raw value -> scalar or ARRAY<value_type> (single-stage, no shuffle). Output type equals value type.
    if (inputRaw && !outputPartial) {
        if (outTypeId == type::OMNI_ARRAY) {
            type::DataTypeId elemId = outputTypes.GetType(0)->asArray().ElementType()->GetId();
            if (elemId != valueTypeId)
                throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                    "ApproxPercentile complete array element type must match value type");
        } else if (outTypeId != valueTypeId) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
                "ApproxPercentile complete output type must match value type");
        }
        switch (valueTypeId) {
            case type::OMNI_BYTE: return ApproxPercentileAggregator<type::OMNI_BYTE, type::OMNI_BYTE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_SHORT: return ApproxPercentileAggregator<type::OMNI_SHORT, type::OMNI_SHORT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_INT: return ApproxPercentileAggregator<type::OMNI_INT, type::OMNI_INT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_LONG: return ApproxPercentileAggregator<type::OMNI_LONG, type::OMNI_LONG>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_FLOAT: return ApproxPercentileAggregator<type::OMNI_FLOAT, type::OMNI_FLOAT>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            case type::OMNI_DOUBLE: return ApproxPercentileAggregator<type::OMNI_DOUBLE, type::OMNI_DOUBLE>::Create(inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull);
            default: throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile value type must be BYTE/SHORT/INT/LONG/FLOAT/DOUBLE");
        }
    }

    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "ApproxPercentile invalid inputRaw/outputPartial");
}

/* Explicit template instantiations:
 * - Partial      (value type -> VARBINARY)
 * - PartialMerge (VARBINARY -> VARBINARY)
 * - Final        (VARBINARY -> value type or ARRAY)
 * - Complete     (value type -> value type or ARRAY) */
template class ApproxPercentileAggregator<type::OMNI_BYTE, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_SHORT, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_INT, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_LONG, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_FLOAT, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_DOUBLE, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_VARBINARY>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_BYTE>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_SHORT>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_INT>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_LONG>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_FLOAT>;
template class ApproxPercentileAggregator<type::OMNI_VARBINARY, type::OMNI_DOUBLE>;
template class ApproxPercentileAggregator<type::OMNI_BYTE, type::OMNI_BYTE>;
template class ApproxPercentileAggregator<type::OMNI_SHORT, type::OMNI_SHORT>;
template class ApproxPercentileAggregator<type::OMNI_INT, type::OMNI_INT>;
template class ApproxPercentileAggregator<type::OMNI_LONG, type::OMNI_LONG>;
template class ApproxPercentileAggregator<type::OMNI_FLOAT, type::OMNI_FLOAT>;
template class ApproxPercentileAggregator<type::OMNI_DOUBLE, type::OMNI_DOUBLE>;

}  // namespace op
}  // namespace omniruntime
