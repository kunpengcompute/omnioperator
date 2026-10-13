/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __UNNEST_H__
#define __UNNEST_H__

#include <string_view>
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "util/omni_exception.h"
#include "vector/row_vector.h"
#include "vector/map_vector.h"
#include "vector/array_vector.h"

namespace omniruntime {
namespace op {

class UnnestOperatorFactory : public OperatorFactory {
public:
    explicit UnnestOperatorFactory(std::shared_ptr<const UnnestNode> planNode) : planNode_(planNode) {}

    static UnnestOperatorFactory *CreateUnnestOperatorFactory(std::shared_ptr<const UnnestNode> planNode);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;
private:
    std::shared_ptr<const UnnestNode> planNode_;
};

class UnnestOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    UnnestOperator(omniruntime::mem::OmniTaskMemoryPool *pool, std::shared_ptr<const UnnestNode> planNode);

    ~UnnestOperator() override = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **resultVecBatch) override;

    OmniStatus Close() override;

private:
    // Shared init body for legacy + pool-bound ctors.
    void InitFromCtor(const std::shared_ptr<const UnnestNode>& planNode);

    // Generate output for 'size' input rows starting from 'start' input row.
    // @param start First input row to include in the output.
    // @param size Number of input rows to include in the output.
    // @param outputSize Pre-computed number of output rows.
    void generateOutput(int32_t numElements, omniruntime::vec::VectorBatch *vecBatch);

    void generateRepeatedColumns(int32_t numElements, omniruntime::vec::VectorBatch *vecBatch,
                                 omniruntime::vec::VectorBatch* resultVecBatch);
    void generateUnrepeatedColumns(int32_t numElements, omniruntime::vec::VectorBatch *vecBatch,
                                   omniruntime::vec::VectorBatch* resultVecBatch);
    void generateOrdinalityColumns(int32_t numElements, omniruntime::vec::VectorBatch *vecBatch,
                                   omniruntime::vec::VectorBatch* resultVecBatch);

    omniruntime::vec::BaseVector* generateUnrepeatedValuesForType(omniruntime::vec::BaseVector* elementVector,
                                         omniruntime::vec::BaseVector* inputVector, int32_t numElements);

    omniruntime::vec::BaseVector* generateUnrepeatedRowValues(omniruntime::vec::RowVector* elementVector,
                                         omniruntime::vec::BaseVector* inputVector, int32_t numElements);

    omniruntime::vec::BaseVector* generateUnrepeatedMapValues(omniruntime::vec::MapVector* elementVector,
                                         omniruntime::vec::BaseVector* inputVector, int32_t numElements);

    omniruntime::vec::BaseVector* generateUnrepeatedArrayValues(omniruntime::vec::ArrayVector* elementVector,
                                         omniruntime::vec::BaseVector* inputVector, int32_t numElements);

    omniruntime::vec::BaseVector* generateUnrepeatedElementValues(omniruntime::vec::BaseVector* elementVector,
                                         omniruntime::vec::BaseVector* inputVector, int32_t numElements);

    void generateComplexRepeatedValuesForType(DataTypeId typeId, int32_t inputSize, auto* inputVector, auto* outputVector,
                                              BaseVector* inputElementVector, BaseVector* outputElementVector);

    template<typename VectorType>
    void generateRepeatedValues(VectorType* inputVector, VectorType* outputVector);

    void generateArrayRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                     omniruntime::vec::BaseVector* outputVector);

    void generateMapRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                   omniruntime::vec::BaseVector* outputVector);

    void generateStructRepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                      omniruntime::vec::BaseVector* outputVector);

    template<typename VectorType>
    void generateComplexRepeatedValues(int32_t inputSize, auto* inputVector, auto* outputVector,
                                         VectorType* inputElementVector, VectorType* outputElementVector);

    template<typename VectorType>
    void generateUnrepeatedValues(omniruntime::vec::BaseVector* inputVector,
                                  VectorType* elementVector, VectorType* outputVector);

    std::vector<int32_t> rawMaxSizes_;

    const bool withOrdinality_;
    const bool outer_;
    std::vector<uint32_t> unnestChannels_;

    vec::VectorBatch *outputVecBatch;
    int32_t outputTypeSize_;
};
}
}

#endif // __UNNEST_H__