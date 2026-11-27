/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: CodegenUtil Class
 */

#include "codegen_util.h"

#include <string>
#include <vector>

using namespace std;
using namespace omniruntime::vec;

namespace CodegenUtil {
int64_t GetRawAddr(const DataTypes &types, int32_t i, BaseVector *colVec)
{
    switch (types.GetIds()[i]) {
        case OMNI_BYTE:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int8_t> *>(colVec)));
        case OMNI_INT:
        case OMNI_DATE32:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int32_t> *>(colVec)));
        case OMNI_SHORT:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int16_t> *>(colVec)));
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<int64_t> *>(colVec)));
        case OMNI_DOUBLE:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<double> *>(colVec)));
        case OMNI_FLOAT:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<float> *>(colVec)));
        case OMNI_BOOLEAN:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<bool> *>(colVec)));
        case OMNI_DECIMAL128:
            return reinterpret_cast<int64_t>(
                unsafe::UnsafeVector::GetRawValues(reinterpret_cast<Vector<Decimal128> *>(colVec)));
        case OMNI_VARCHAR:
        case OMNI_CHAR:
            return reinterpret_cast<int64_t>(unsafe::UnsafeStringVector::GetValues(
                reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(colVec)));
        default:
            LogError("Do not support such vector type %d", types.GetIds()[i]);
            return 0;
    }
}

void GetDataFromVecBatch(VectorBatch &vecBatch, intptr_t valueAddrs[], intptr_t nullAddrs[], intptr_t offsetAddrs[],
    intptr_t dictionaries[], const DataTypes &types)
{
    intptr_t valuesAddress;
    intptr_t dictVecAddress;
    auto vectorCount = static_cast<int32_t>(vecBatch.GetVectorCount());
    for (int32_t i = 0; i < vectorCount; i++) {
        auto colVec = vecBatch.Get(i);
        dictVecAddress = 0;
        valuesAddress = 0;
        if (colVec->GetEncoding() == OMNI_DICTIONARY || colVec->GetEncoding() == OMNI_ENCODING_CONST) {
            dictVecAddress = reinterpret_cast<intptr_t>(reinterpret_cast<void *>(colVec));
        } else {
            valuesAddress = GetRawAddr(types, i, colVec);
        }
        dictionaries[i] = dictVecAddress;
        valueAddrs[i] = valuesAddress;
        nullAddrs[i] = reinterpret_cast<intptr_t>(unsafe::UnsafeBaseVector::GetNulls(colVec));
        offsetAddrs[i] = reinterpret_cast<intptr_t>(VectorHelper::UnsafeGetOffsetsAddr(colVec));
    }
}

VectorBatch *FilterAndProject(std::unique_ptr<omniruntime::codegen::Filter> &filter,
    std::vector<std::unique_ptr<omniruntime::codegen::Projection>> &projections, int32_t numCols, VectorBatch *vecBatch,
    int32_t &numSelectedRows, const DataTypes &types)
{
    const int vectorCount = static_cast<int32_t>(vecBatch->GetVectorCount());
    int64_t valueAddrs[vectorCount];
    int64_t nullAddrs[vectorCount];
    int64_t offsetAddrs[vectorCount];
    int64_t dictionaries[vectorCount];

    const int rowCount = static_cast<int32_t>(vecBatch->GetRowCount());
    auto *selectedRows = new int32_t[rowCount];
    GetDataFromVecBatch(*vecBatch, valueAddrs, nullAddrs, offsetAddrs, dictionaries, types);

    auto context = new omniruntime::op::ExecutionContext(4096, nullptr);
    if (filter != nullptr) {
        numSelectedRows = filter->GetFilterFunc()(valueAddrs, rowCount, selectedRows, nullAddrs, offsetAddrs,
            reinterpret_cast<int64_t>(context), dictionaries);
    }

    if (context->HasError()) {
        delete[] selectedRows;
        context->GetArena()->Reset();
        VectorHelper::FreeVecBatch(vecBatch);
        std::string errorMessage = context->GetError();
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
    }

    auto ret = (!projections.empty()) ? new VectorBatch(numSelectedRows) : nullptr;
    for (uint32_t i = 0; i < projections.size(); i++) {
        BaseVector *col =
            projections[i]->Project(vecBatch, (filter != nullptr) ? selectedRows : nullptr, numSelectedRows, valueAddrs,
            nullAddrs, offsetAddrs, context, dictionaries, types.GetIds());
        if (context->HasError()) {
            VectorHelper::FreeVecBatch(ret);
            VectorHelper::FreeVecBatch(vecBatch);
            delete[] selectedRows;
            context->GetArena()->Reset();

            std::string errorMessage = context->GetError();
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", errorMessage);
        }
        ret->Append(col);
    }
    context->GetArena()->Reset();
    delete[] selectedRows;
    delete context;
    return ret;
}

unique_ptr<Filter> GenerateFilterAndProjections(Expr *filterExpr, std::vector<Expr *> &projExprs, DataTypes &inputTypes,
    std::vector<std::unique_ptr<Projection>> &projections, OverflowConfig *overflowConfig)
{
    for (uint32_t i = 0; i < projExprs.size(); i++) {
        auto projection = make_unique<Projection>(*(projExprs[i]), filterExpr != nullptr, projExprs[i]->GetReturnType(),
            inputTypes, overflowConfig);
        projections.push_back(move(projection));
    }
    if (filterExpr == nullptr) {
        Filter *filter = nullptr;
        return std::move(reinterpret_cast<unique_ptr<omniruntime::op::Filter> &>(filter));
    }
    return std::move(make_unique<Filter>(*filterExpr, inputTypes, nullptr));
}
}