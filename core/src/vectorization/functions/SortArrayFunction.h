/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: SortArray function implementation for sorting array elements
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// sort_array function implementation for arrays.
/// sort_array(array(T)) -> array(T)
/// sort_array(array(T), ascendingOrder) -> array(T)
/// Sorts the input array in ascending or descending order according to the
/// natural ordering of the array elements. Null elements are placed at the
/// beginning of the returned array in ascending order or at the end in
/// descending order.
/// When ascendingOrder is omitted, the default is true (ascending).
/// Supports element types: BYTE, SHORT, INT, LONG, FLOAT, DOUBLE, BOOLEAN,
/// VARCHAR, CHAR, VARBINARY, DECIMAL64, DECIMAL128, DATE32, TIMESTAMP.
/// For FLOAT/DOUBLE types, NaN values are treated as greater than any other
/// non-null value (consistent with Spark behavior).
class SortArrayFunction : public VectorFunction {
public:
    explicit SortArrayFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    template <typename T>
    void SortTypedElements(BaseVector *srcElem, BaseVector *dstElem,
        int64_t offset, int64_t size, bool ascending) const;

    void SortStringElements(BaseVector *srcElem, BaseVector *dstElem,
        int64_t offset, int64_t size, bool ascending) const;

    static BaseVector *CreateElementVectorByType(DataTypeId typeId, int64_t size,
        omniruntime::mem::OmniTaskMemoryPool *pool);

    static void CopyElementValue(BaseVector *srcElem, int32_t srcIdx,
        BaseVector *dstElem, int32_t dstIdx);
};
} // namespace omniruntime::vectorization
