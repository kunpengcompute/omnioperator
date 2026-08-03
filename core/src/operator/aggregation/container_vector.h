/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 */
#ifndef OMNI_RUNTIME_CONTAINER_VECTOR_H
#define OMNI_RUNTIME_CONTAINER_VECTOR_H

#include <vector>
#include "vector/vector.h"
#include "util/error_code.h"
#include "util/omni_exception.h"
#include "type/data_type.h"

/**
 * ContainerVector is for combining more than one vectors to one vector. Present as a vector
 * to the place where it is used. For instance, in two-stage aggregations, calculating average
 * in partial stage will produce two vectors. One is intermediate average value, the other is
 * intermediate count value.
 */
namespace omniruntime {
namespace vec {
class ContainerVector : public Vector<int64_t> {
public:
    ContainerVector(int32_t positionCount, std::vector<int64_t> &fieldVectors,
        std::vector<omniruntime::type::DataTypePtr> &dataTypes);

    ContainerVector(int32_t capacityInBytes, int32_t positionCount);

    // inline for high performance.
    int64_t ALWAYS_INLINE GetValue(int32_t index)
    {
        return values[index];
    }

    /* *
     * Set the value at the indicated index
     * @param index
     * @param value
     */
    void ALWAYS_INLINE SetValue(int index, int64_t value)
    {
        values[index] = value;
    }

    int32_t ALWAYS_INLINE GetVectorCount()
    {
        return dataTypes.size();
    }

    std::vector<type::DataTypePtr> ALWAYS_INLINE &GetDataTypes()
    {
        return dataTypes;
    }

    static void AppendToVector(BaseVector *destVector, int32_t offset, BaseVector *srcVector, int32_t length,
        int32_t dataTypeId);

    /* *
     *
     * @param other the dst data from
     * @param positionOffset element position
     * @param length number of elements
     */
    void Append(BaseVector *other, int positionOffset, int length);

    /* *
     * not support
     * @param positions
     * @param offset
     * @param length
     */
    Vector<int64_t> *CopyPositions(const int *positions, int positionOffset, int length);

    /* *
     * not support
     * @param positionOffset
     * @param length
     * @param isCopy reserved parameters
     */
    ContainerVector *Slice(int positionOffset, int length, bool isCopy = false);

    ~ContainerVector();

    void SetDataTypes(const std::vector<type::DataTypePtr> &dataTypes);

private:
    std::vector<type::DataTypePtr> dataTypes;
};
} // namespace vec
} // namespace omniruntime
#endif // OMNI_RUNTIME_CONTAINER_VECTOR_H
