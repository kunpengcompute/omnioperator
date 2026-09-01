/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "Comparisons.h"
#include "vectorization/SelectivityVector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;

class EqualStringFunction final : public VectorFunction {

    bool SupportsFlatNoNullsFastPath() const override
    {
        return true;
    }

    // this method is used to compare between columns which one is dictionary and another is flat
    static void handleColumnWithDiffEncodingComparison(BaseVector *leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {

        if (leftArg->GetEncoding() == OMNI_DICTIONARY) {
            auto leftVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(leftArg);
            auto rightVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightArg);
            auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
            auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(rightNullBits, rowSize);
            leftSelectivity->intersect(*rightSelectivity);
            leftSelectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, leftVector->GetValue(i) == rightVector->GetValue(i));
            });
        } else {
            auto leftVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(leftArg);
            auto rightVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(rightArg);
            auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
            auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(rightNullBits, rowSize);
            leftSelectivity->intersect(*rightSelectivity);
            leftSelectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, leftVector->GetValue(i) == rightVector->GetValue(i));
            });
        }
    }

    // this method is used to compare between columns
    // this is also an example of how to use Selectivity
    static void handleDictColumnComparison(BaseVector* leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {
        auto leftVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(leftArg);
        auto rightVector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(rightArg);
        auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
        auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(rightNullBits, rowSize);
        leftSelectivity->intersect(*rightSelectivity);
        leftSelectivity->applyToSelected([&](vector_size_t i) {
            comparedResult->SetValue(i, leftVector->GetValue(i) == rightVector->GetValue(i));
        });
    }

    static void handleFlatColumnComparison(BaseVector* leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {
        auto leftVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(leftArg);
        auto rightVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rightArg);
        auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
        auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(rightNullBits, rowSize);
        leftSelectivity->intersect(*rightSelectivity);
        leftSelectivity->applyToSelected([&](vector_size_t i) {
            comparedResult->SetValue(i, leftVector->GetValue(i) == rightVector->GetValue(i));
        });
    }


    // this method is used to compare between column and constant
    static void handleConstComparison(BaseVector *vectorArg, const std::string_view &constant, Vector<bool> * comparedResult, int32_t rowSize) {

        if (vectorArg->GetEncoding() == OMNI_DICTIONARY) {
            auto vector = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vectorArg);
            auto selectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(vector));
            selectivity->setFromBitsNegate(nullBits, rowSize);
            selectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, vector->GetValue(i) == constant);
            });

        } else {
            auto vector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectorArg);
            auto selectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(vector));
            selectivity->setFromBitsNegate(nullBits, rowSize);
            selectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, vector->GetValue(i) == constant);
            });
        }
    }

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
               op::ExecutionContext *context) const override
    {
        auto rightArg = args.top();
        args.pop();
        auto leftArg = args.top();
        args.pop();
        auto rowSize = context->GetResultRowSize();
        auto *flatResult = reinterpret_cast<Vector<bool> *>(VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowSize));
        auto alignedBuffer = unsafe::UnsafeVector::GetValues(flatResult)->GetBuffer();
        // set defualt compared result is 0 which means the result is filtered for there might exists some row not compare when it's null
        memset(alignedBuffer, 0, rowSize);
        if (leftArg->GetEncoding() == OMNI_ENCODING_CONST || rightArg->GetEncoding() == OMNI_ENCODING_CONST) {
            // Fast path for (flat, const).
            auto leftIsConst = leftArg->GetEncoding() == OMNI_ENCODING_CONST;
            auto constant = leftIsConst ? reinterpret_cast<ConstVector<std::string_view> *>(leftArg)->GetConstValue() : reinterpret_cast<ConstVector<std::string_view> *>(rightArg)->GetConstValue();
            handleConstComparison(leftIsConst ? rightArg : leftArg, constant, flatResult, rowSize);
        } else if (leftArg->GetEncoding() == OMNI_FLAT && rightArg->GetEncoding() == OMNI_FLAT) {
            // Fast path for (flat, flat).
            handleFlatColumnComparison(leftArg, rightArg, flatResult, rowSize);
        } else if (leftArg->GetEncoding() == OMNI_DICTIONARY && rightArg->GetEncoding() == OMNI_DICTIONARY) {
            // Fast path for (dict, dict).
            handleDictColumnComparison(leftArg, rightArg, flatResult, rowSize);
        } else if ((leftArg->GetEncoding() == OMNI_DICTIONARY && rightArg->GetEncoding() == OMNI_FLAT)
            || (leftArg->GetEncoding() == OMNI_FLAT && rightArg->GetEncoding() == OMNI_DICTIONARY)) {
            // Fast path for (dict, flat) or (flat, dict)
            handleColumnWithDiffEncodingComparison(leftArg, rightArg, flatResult, rowSize);
        } else {
            // Path if one or more arguments are encoded.
            LogWarn("left Arg type is %s, right Arg type is %s", leftArg->GetEncoding(), rightArg->GetEncoding());
            auto typeDesc = "left Arg type is " +  std::to_string(leftArg->GetEncoding()) + " right Arg type is " + std::to_string(rightArg->GetEncoding());
            OMNI_THROW("EqualStringFunction Error:", typeDesc);
        }
        result = flatResult;
        delete rightArg;
        delete leftArg;
    }
};
}