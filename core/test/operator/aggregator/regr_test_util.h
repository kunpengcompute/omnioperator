/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#ifndef OMNI_RUNTIME_REGR_TEST_UTIL_H
#define OMNI_RUNTIME_REGR_TEST_UTIL_H

#include <memory>
#include <vector>

#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/regr/regr_state.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/operator.h"
#include "type/data_type.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

namespace omniruntime::test {

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;

inline std::unique_ptr<HashAggregationOperatorFactory> CreateRegrHashAggregationFactory(
    uint32_t aggFuncType,
    const DataTypePtr &aggOutputType)
{
    std::vector<uint32_t> groupByColumns = {0};
    std::vector<DataTypePtr> groupTypes = {LongType()};
    std::vector<std::vector<uint32_t>> aggInputColsWrap = {{1, 2}};
    std::vector<DataTypes> aggInputTypesWrap = {DataTypes({DoubleType(), DoubleType()})};
    std::vector<DataTypes> aggOutputTypesWrap = {DataTypes({aggOutputType})};
    std::vector<uint32_t> aggFuncTypes = {aggFuncType};
    std::vector<uint32_t> aggMask = {static_cast<uint32_t>(-1)};
    std::vector<bool> inputRawWrap = {true};
    std::vector<bool> outputPartialWrap = {false};

    auto factory = std::make_unique<HashAggregationOperatorFactory>(
        groupByColumns, DataTypes(groupTypes), aggInputColsWrap, aggInputTypesWrap,
        aggOutputTypesWrap, aggFuncTypes, aggMask, inputRawWrap, outputPartialWrap, false);
    factory->Init();
    return factory;
}

inline VectorBatch *MakeLinearBatch()
{
    constexpr int32_t rowCount = 5;
    auto *batch = new VectorBatch(rowCount);
    auto *groupCol = new Vector<int64_t>(rowCount);
    auto *yCol = new Vector<double>(rowCount);
    auto *xCol = new Vector<double>(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        groupCol->SetValue(i, 0);
        xCol->SetValue(i, static_cast<double>(i));
        yCol->SetValue(i, 1.0 + 2.0 * i);
    }
    batch->Append(groupCol);
    batch->Append(yCol);
    batch->Append(xCol);
    return batch;
}

inline double RunRegrAndGetDouble(uint32_t aggType)
{
    auto *batch = MakeLinearBatch();
    auto factory = CreateRegrHashAggregationFactory(aggType, DoubleType());
    auto *op = factory->CreateOperator(nullptr);
    op->Init();
    op->AddInput(batch);
    VectorBatch *out = nullptr;
    op->GetOutput(&out);
    auto *resVec = out->Get(1);
    double v = static_cast<Vector<double> *>(resVec)->GetValue(0);
    omniruntime::op::Operator::DeleteOperator(op);
    VectorHelper::FreeVecBatch(out);
    return v;
}

inline int64_t RunRegrAndGetLong(uint32_t aggType)
{
    auto *batch = MakeLinearBatch();
    auto factory = CreateRegrHashAggregationFactory(aggType, LongType());
    auto *op = factory->CreateOperator(nullptr);
    op->Init();
    op->AddInput(batch);
    VectorBatch *out = nullptr;
    op->GetOutput(&out);
    auto *resVec = out->Get(1);
    int64_t v = static_cast<Vector<int64_t> *>(resVec)->GetValue(0);
    omniruntime::op::Operator::DeleteOperator(op);
    VectorHelper::FreeVecBatch(out);
    return v;
}

/** y = 1 + 2*x, x = 0..rowCount-1; column 0 = y, column 1 = x. Caller frees with VectorHelper::FreeVecBatch. */
/** y = 1 + 2*x, x runs startInclusive .. startInclusive+rowCount-1. Caller frees. */
inline VectorBatch *MakeRegrYxLinearSlice(int32_t startInclusive, int32_t rowCount)
{
    auto *batch = new VectorBatch(rowCount);
    auto *yCol = new Vector<double>(rowCount);
    auto *xCol = new Vector<double>(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        int32_t x = startInclusive + i;
        xCol->SetValue(i, static_cast<double>(x));
        yCol->SetValue(i, 1.0 + 2.0 * static_cast<double>(x));
    }
    batch->Append(yCol);
    batch->Append(xCol);
    return batch;
}

inline VectorBatch *MakeRegrYxLinearBatch(int32_t rowCount = 5)
{
    return MakeRegrYxLinearSlice(0, rowCount);
}

inline double RunNonGroupRegrGetDouble(uint32_t aggFuncType)
{
    DataTypes sourceTypes({DoubleType(), DoubleType()});
    std::vector<uint32_t> aggFuncTypes = {aggFuncType};
    std::vector<std::vector<uint32_t>> aggInputCols = {{0, 1}};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1)};
    std::vector<DataTypes> aggOutputTypes = {DataTypes({DoubleType()})};
    std::vector<bool> inputRaws = {true};
    std::vector<bool> outputPartials = {false};
    std::vector<int8_t> hasAggFilters = {0};
    auto factory = std::make_unique<AggregationOperatorFactory>(sourceTypes, aggFuncTypes, aggInputCols, maskCols,
        aggOutputTypes, inputRaws, outputPartials, hasAggFilters, false, false);
    factory->Init();
    auto *op = factory->CreateOperator(nullptr);
    auto *in = MakeRegrYxLinearBatch();
    op->Init();
    op->AddInput(in);
    VectorBatch *out = nullptr;
    op->GetOutput(&out);
    double v = static_cast<Vector<double> *>(out->Get(0))->GetValue(0);
    omniruntime::op::Operator::DeleteOperator(op);
    VectorHelper::FreeVecBatch(out);
    return v;
}

inline std::unique_ptr<Aggregator> CreateRegrAggregatorForUt(FunctionType ft, const DataTypes &inputTypes,
    const DataTypes &outputTypes, const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial)
{
    RegrAggregatorFactory factory(ft);
    std::vector<int32_t> ch = channels;
    return factory.CreateAggregator(inputTypes, outputTypes, ch, inputRaw, outputPartial, false);
}

inline double RegrUtExtractFinalDouble(Aggregator *agg, const AggregateState *state)
{
    auto *out = new Vector<double>(1);
    std::vector<BaseVector *> ov = {out};
    agg->ExtractValues(state, ov, 0);
    double v = out->GetValue(0);
    delete out;
    return v;
}

/** After InitState only; final static_cast DOUBLE metric should be null. */
inline bool RegrUtFinalIsNullAfterInitOnly(FunctionType ft)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    auto *out = new Vector<double>(1);
    std::vector<BaseVector *> ov = {out};
    agg->ExtractValues(reinterpret_cast<const AggregateState *>(buf.data()), ov, 0);
    bool isNull = out->IsNull(0);
    delete out;
    return isNull;
}

/** Direct raw accumulation final value (covers ProcessSingleInternal + ExtractValues). */
inline double RegrUtDirectRawFinal(FunctionType ft, VectorBatch *yx)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    agg->ProcessGroup(buf.data(), yx, 0, yx->GetRowCount());
    return RegrUtExtractFinalDouble(agg.get(), reinterpret_cast<const AggregateState *>(buf.data()));
}

/** Pearson layout spill row: LONG + 5 DOUBLE; ProcessGroupUnspill + ExtractValues. */
inline double RegrUtPearsonSpillUnspillFinal(FunctionType ft, VectorBatch *yx)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    agg->ProcessGroup(buf.data(), yx, 0, yx->GetRowCount());

    VectorBatch spillBatch(1);
    spillBatch.Append(new Vector<int64_t>(1));
    for (int i = 0; i < 5; ++i) {
        spillBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> spillPtrs;
    for (int c = 0; c < 6; ++c) {
        spillPtrs.push_back(spillBatch.Get(c));
    }
    std::vector<AggregateState *> gs = {buf.data()};
    agg->ExtractValuesForSpill(gs, spillPtrs);

    std::vector<uint8_t> buf2(agg->GetStateSize());
    agg->InitState(buf2.data());
    UnspillRowInfo ur{buf2.data(), &spillBatch, 0};
    std::vector<UnspillRowInfo> rows = {ur};
    int32_t vi = 0;
    agg->ProcessGroupUnspill(rows, 1, vi);

    return RegrUtExtractFinalDouble(agg.get(), reinterpret_cast<const AggregateState *>(buf2.data()));
}

inline int32_t RegrUtPearsonSpillVectorIndexAfterUnspill()
{
    return 6;
}

/** VarPop partial (regr_sxx / regr_syy): 3 DOUBLE spill columns. */
inline double RegrUtVarPopSpillUnspillFinal(FunctionType ft, VectorBatch *yx)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    agg->ProcessGroup(buf.data(), yx, 0, yx->GetRowCount());

    VectorBatch spillBatch(1);
    for (int i = 0; i < 3; ++i) {
        spillBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> spillPtrs = {spillBatch.Get(0), spillBatch.Get(1), spillBatch.Get(2)};
    std::vector<AggregateState *> gs = {buf.data()};
    agg->ExtractValuesForSpill(gs, spillPtrs);

    std::vector<uint8_t> buf2(agg->GetStateSize());
    agg->InitState(buf2.data());
    UnspillRowInfo ur{buf2.data(), &spillBatch, 0};
    std::vector<UnspillRowInfo> rows = {ur};
    int32_t vi = 0;
    agg->ProcessGroupUnspill(rows, 1, vi);

    return RegrUtExtractFinalDouble(agg.get(), reinterpret_cast<const AggregateState *>(buf2.data()));
}

inline int32_t RegrUtVarPopSpillVectorIndexAfterUnspill()
{
    return 3;
}

/** regr_sxy: 4 DOUBLE spill columns. */
inline double RegrUtCov4SpillUnspillFinal(FunctionType ft, VectorBatch *yx)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    agg->ProcessGroup(buf.data(), yx, 0, yx->GetRowCount());

    VectorBatch spillBatch(1);
    for (int i = 0; i < 4; ++i) {
        spillBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> spillPtrs = {spillBatch.Get(0), spillBatch.Get(1), spillBatch.Get(2),
        spillBatch.Get(3)};
    std::vector<AggregateState *> gs = {buf.data()};
    agg->ExtractValuesForSpill(gs, spillPtrs);

    std::vector<uint8_t> buf2(agg->GetStateSize());
    agg->InitState(buf2.data());
    UnspillRowInfo ur{buf2.data(), &spillBatch, 0};
    std::vector<UnspillRowInfo> rows = {ur};
    int32_t vi = 0;
    agg->ProcessGroupUnspill(rows, 1, vi);

    return RegrUtExtractFinalDouble(agg.get(), reinterpret_cast<const AggregateState *>(buf2.data()));
}

inline int32_t RegrUtCov4SpillVectorIndexAfterUnspill()
{
    return 4;
}

/** regr_slope / regr_intercept: 7 DOUBLE spill columns. */
inline double RegrUtSlopeFamilySpillUnspillFinal(FunctionType ft, VectorBatch *yx)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> buf(agg->GetStateSize());
    agg->InitState(buf.data());
    agg->ProcessGroup(buf.data(), yx, 0, yx->GetRowCount());

    VectorBatch spillBatch(1);
    for (int i = 0; i < 7; ++i) {
        spillBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> spillPtrs;
    for (int c = 0; c < 7; ++c) {
        spillPtrs.push_back(spillBatch.Get(c));
    }
    std::vector<AggregateState *> gs = {buf.data()};
    agg->ExtractValuesForSpill(gs, spillPtrs);

    std::vector<uint8_t> buf2(agg->GetStateSize());
    agg->InitState(buf2.data());
    UnspillRowInfo ur{buf2.data(), &spillBatch, 0};
    std::vector<UnspillRowInfo> rows = {ur};
    int32_t vi = 0;
    agg->ProcessGroupUnspill(rows, 1, vi);

    return RegrUtExtractFinalDouble(agg.get(), reinterpret_cast<const AggregateState *>(buf2.data()));
}

inline int32_t RegrUtSlopeFamilySpillVectorIndexAfterUnspill()
{
    return 7;
}

inline std::vector<DataTypeId> RegrUtGetSpillTypeIds(Aggregator *agg)
{
    auto types = agg->GetSpillType();
    std::vector<DataTypeId> ids;
    ids.reserve(types.size());
    for (const auto &p : types) {
        ids.push_back(p->GetId());
    }
    return ids;
}

/** InitStates + two ProcessGroup(raw slices) + ExtractValuesBatch (group merge path on multi-row batches). */
inline void RegrUtExtractValuesBatchTwoStates(FunctionType ft, double *out0, double *out1, bool *null0, bool *null1)
{
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> ch = {0, 1};
    auto agg = CreateRegrAggregatorForUt(ft, inRaw, outF, ch, true, false);
    agg->SetStateOffset(0);
    std::vector<uint8_t> b1(agg->GetStateSize());
    std::vector<uint8_t> b2(agg->GetStateSize());
    std::vector<AggregateState *> rs = {b1.data(), b2.data()};
    agg->InitStates(rs);

    auto *batch3 = new VectorBatch(3);
    auto *y3 = new Vector<double>(3);
    auto *x3 = new Vector<double>(3);
    for (int32_t i = 0; i < 3; ++i) {
        x3->SetValue(i, static_cast<double>(i));
        y3->SetValue(i, 1.0 + 2.0 * static_cast<double>(i));
    }
    batch3->Append(y3);
    batch3->Append(x3);
    agg->ProcessGroup(b1.data(), batch3, 0, 3);
    VectorHelper::FreeVecBatch(batch3);

    auto *batch2 = new VectorBatch(2);
    auto *y2 = new Vector<double>(2);
    auto *x2 = new Vector<double>(2);
    for (int32_t i = 0; i < 2; ++i) {
        int32_t x = 3 + i;
        x2->SetValue(i, static_cast<double>(x));
        y2->SetValue(i, 1.0 + 2.0 * static_cast<double>(x));
    }
    batch2->Append(y2);
    batch2->Append(x2);
    agg->ProcessGroup(b2.data(), batch2, 0, 2);
    VectorHelper::FreeVecBatch(batch2);

    auto *o = new Vector<double>(2);
    std::vector<BaseVector *> ov = {o};
    agg->ExtractValuesBatch(rs, ov, 0, 2);
    *null0 = o->IsNull(0);
    *null1 = o->IsNull(1);
    if (!*null0) {
        *out0 = o->GetValue(0);
    }
    if (!*null1) {
        *out1 = o->GetValue(1);
    }
    delete o;
}

inline double RegrUtPearsonMergeFromPartialRow(FunctionType ft, int32_t rawRowCount)
{
    auto *yx = MakeRegrYxLinearBatch(rawRowCount);
    RegrAggregatorFactory fac(ft);
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outPart(
        {LongType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes inMerge(
        {LongType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> chR = {0, 1};
    std::vector<int32_t> chM = {0, 1, 2, 3, 4, 5};
    auto partAgg = fac.CreateAggregator(inRaw, outPart, chR, true, true, false);
    partAgg->SetStateOffset(0);
    std::vector<uint8_t> bufP(partAgg->GetStateSize());
    partAgg->InitState(bufP.data());
    partAgg->ProcessGroup(bufP.data(), yx, 0, yx->GetRowCount());

    VectorBatch partialBatch(1);
    partialBatch.Append(new Vector<int64_t>(1));
    for (int i = 0; i < 5; ++i) {
        partialBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> pv;
    for (int c = 0; c < 6; ++c) {
        pv.push_back(partialBatch.Get(c));
    }
    partAgg->ExtractValues(bufP.data(), pv, 0);

    auto mergeAgg = fac.CreateAggregator(inMerge, outF, chM, false, false, false);
    mergeAgg->SetStateOffset(0);
    std::vector<uint8_t> bufM(mergeAgg->GetStateSize());
    mergeAgg->InitState(bufM.data());
    mergeAgg->ProcessGroup(bufM.data(), &partialBatch, 0, 1);
    double vMerge = RegrUtExtractFinalDouble(mergeAgg.get(), reinterpret_cast<const AggregateState *>(bufM.data()));

    VectorHelper::FreeVecBatch(yx);
    return vMerge;
}

inline double RegrUtVarPopMergeFromPartialRow(FunctionType ft, int32_t rawRowCount)
{
    auto *yx = MakeRegrYxLinearBatch(rawRowCount);
    RegrAggregatorFactory fac(ft);
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outPart({DoubleType(), DoubleType(), DoubleType()});
    DataTypes inMerge({DoubleType(), DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> chR = {0, 1};
    std::vector<int32_t> chM = {0, 1, 2};
    auto partAgg = fac.CreateAggregator(inRaw, outPart, chR, true, true, false);
    partAgg->SetStateOffset(0);
    std::vector<uint8_t> bufP(partAgg->GetStateSize());
    partAgg->InitState(bufP.data());
    partAgg->ProcessGroup(bufP.data(), yx, 0, yx->GetRowCount());

    VectorBatch partialBatch(1);
    for (int i = 0; i < 3; ++i) {
        partialBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> pv = {partialBatch.Get(0), partialBatch.Get(1), partialBatch.Get(2)};
    partAgg->ExtractValues(bufP.data(), pv, 0);

    auto mergeAgg = fac.CreateAggregator(inMerge, outF, chM, false, false, false);
    mergeAgg->SetStateOffset(0);
    std::vector<uint8_t> bufM(mergeAgg->GetStateSize());
    mergeAgg->InitState(bufM.data());
    mergeAgg->ProcessGroup(bufM.data(), &partialBatch, 0, 1);
    double vMerge = RegrUtExtractFinalDouble(mergeAgg.get(), reinterpret_cast<const AggregateState *>(bufM.data()));

    VectorHelper::FreeVecBatch(yx);
    return vMerge;
}

inline double RegrUtCov4MergeFromPartialRow(FunctionType ft, int32_t rawRowCount)
{
    auto *yx = MakeRegrYxLinearBatch(rawRowCount);
    RegrAggregatorFactory fac(ft);
    DataTypes inRaw({DoubleType(), DoubleType()});
    DataTypes outPart({DoubleType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes inMerge({LongType(), DoubleType(), DoubleType(), DoubleType()});
    DataTypes outF({DoubleType()});
    std::vector<int32_t> chR = {0, 1};
    std::vector<int32_t> chM = {0, 1, 2, 3};
    auto partAgg = fac.CreateAggregator(inRaw, outPart, chR, true, true, false);
    partAgg->SetStateOffset(0);
    std::vector<uint8_t> bufP(partAgg->GetStateSize());
    partAgg->InitState(bufP.data());
    partAgg->ProcessGroup(bufP.data(), yx, 0, yx->GetRowCount());

    VectorBatch partialBatch(1);
    for (int i = 0; i < 4; ++i) {
        partialBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> pv = {partialBatch.Get(0), partialBatch.Get(1), partialBatch.Get(2), partialBatch.Get(3)};
    partAgg->ExtractValues(bufP.data(), pv, 0);

    auto mergeAgg = fac.CreateAggregator(inMerge, outF, chM, false, false, false);
    mergeAgg->SetStateOffset(0);
    std::vector<uint8_t> bufM(mergeAgg->GetStateSize());
    mergeAgg->InitState(bufM.data());
    mergeAgg->ProcessGroup(bufM.data(), &partialBatch, 0, 1);
    double vMerge = RegrUtExtractFinalDouble(mergeAgg.get(), reinterpret_cast<const AggregateState *>(bufM.data()));

    VectorHelper::FreeVecBatch(yx);
    return vMerge;
}

inline double RegrUtSlopeFamilyMergeFromPartialRow(FunctionType ft, int32_t rawRowCount)
{
    auto *yx = MakeRegrYxLinearBatch(rawRowCount);
    RegrAggregatorFactory fac(ft);
    DataTypes inRaw({DoubleType(), DoubleType()});
    std::vector<DataTypePtr> seven(7, DoubleType());
    DataTypes outPart(seven);
    DataTypes inMerge(seven);
    DataTypes outF({DoubleType()});
    std::vector<int32_t> chR = {0, 1};
    std::vector<int32_t> chM = {0, 1, 2, 3, 4, 5, 6};
    auto partAgg = fac.CreateAggregator(inRaw, outPart, chR, true, true, false);
    partAgg->SetStateOffset(0);
    std::vector<uint8_t> bufP(partAgg->GetStateSize());
    partAgg->InitState(bufP.data());
    partAgg->ProcessGroup(bufP.data(), yx, 0, yx->GetRowCount());

    VectorBatch partialBatch(1);
    for (int i = 0; i < 7; ++i) {
        partialBatch.Append(new Vector<double>(1));
    }
    std::vector<BaseVector *> pv;
    for (int c = 0; c < 7; ++c) {
        pv.push_back(partialBatch.Get(c));
    }
    partAgg->ExtractValues(bufP.data(), pv, 0);

    auto mergeAgg = fac.CreateAggregator(inMerge, outF, chM, false, false, false);
    mergeAgg->SetStateOffset(0);
    std::vector<uint8_t> bufM(mergeAgg->GetStateSize());
    mergeAgg->InitState(bufM.data());
    mergeAgg->ProcessGroup(bufM.data(), &partialBatch, 0, 1);
    double vMerge = RegrUtExtractFinalDouble(mergeAgg.get(), reinterpret_cast<const AggregateState *>(bufM.data()));

    VectorHelper::FreeVecBatch(yx);
    return vMerge;
}

} // namespace omniruntime::test

#endif // OMNI_RUNTIME_REGR_TEST_UTIL_H
