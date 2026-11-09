/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include "Comparisons.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vectorization/SelectivityVector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::vec;

template <typename Cmp, DataTypeId kind>
class ComparisonFunction final : public VectorFunction {
    using T = typename NativeType<kind>::type;

    static constexpr bool isStringLike = (kind == OMNI_VARCHAR || kind == OMNI_CHAR || kind == OMNI_VARBINARY);
    using DictionaryVectorType = std::conditional_t<isStringLike, Vector<DictionaryContainer<std::string_view, LargeStringContainer>>, Vector<DictionaryContainer<T>>>;
    using FlatVectorType = std::conditional_t<isStringLike, Vector<LargeStringContainer<std::string_view>>, Vector<T>>;

    bool SupportsFlatNoNullsFastPath() const override
    {
        return true;
    }

    // this method is used to compare between columns which one is dictionary and another is flat
    // if leftArg is DICTIONARY comparison is cmp(left, right) corresponding to comparison order
    static void handleColumnWithDiffEncodingComparison(BaseVector *leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {
        const Cmp cmp;
        if (leftArg->GetEncoding() == OMNI_DICTIONARY) {
            auto leftVector = static_cast<DictionaryVectorType *>(leftArg);
            auto rightVector = static_cast<FlatVectorType *>(rightArg);
            auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
            auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(rightVector));
            rightSelectivity->setFromBitsNegate(rightNullBits, rowSize);
            leftSelectivity->intersect(*rightSelectivity);
            leftSelectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, cmp(leftVector->GetValue(i), rightVector->GetValue(i)));
            });

        } else {
            auto leftVector = static_cast<FlatVectorType *>(leftArg);
            auto rightVector = static_cast<DictionaryVectorType *>(rightArg);
            auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
            leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
            auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
            const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(rightVector));
            rightSelectivity->setFromBitsNegate(rightNullBits, rowSize);
            leftSelectivity->intersect(*rightSelectivity);
            leftSelectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, cmp(rightVector->GetValue(i), leftVector->GetValue(i)));
            });
        }
    }

    // this method is used to compare between columns
    // this is also an example of how to use Selectivity
    static void handleDictColumnComparison(BaseVector* leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {
        const Cmp cmp;
        auto leftVector = static_cast<DictionaryVectorType *>(leftArg);
        auto rightVector = static_cast<DictionaryVectorType *>(rightArg);
        auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
        auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(rightVector));
        rightSelectivity->setFromBitsNegate(rightNullBits, rowSize);
        leftSelectivity->intersect(*rightSelectivity);
        leftSelectivity->applyToSelected([&](vector_size_t i) {
            comparedResult->SetValue(i, cmp(leftVector->GetValue(i), rightVector->GetValue(i)));
        });
    }


    static void handleFlatColumnComparison(BaseVector* leftArg, BaseVector* rightArg, Vector<bool> * comparedResult, int32_t rowSize) {
        const Cmp cmp;
        auto leftVector = static_cast<FlatVectorType *>(leftArg);
        auto rightVector = static_cast<FlatVectorType *>(rightArg);
        auto leftSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto leftNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(leftVector));
        leftSelectivity->setFromBitsNegate(leftNullBits, rowSize);
        auto rightSelectivity = std::make_shared<SelectivityVector>(rowSize);
        const auto rightNullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(rightVector));
        rightSelectivity->setFromBitsNegate(rightNullBits, rowSize);
        leftSelectivity->intersect(*rightSelectivity);
        leftSelectivity->applyToSelected([&](vector_size_t i) {
            comparedResult->SetValue(i, cmp(leftVector->GetValue(i), rightVector->GetValue(i)));
        });
    }

    // SQL / Spark: comparison result is unknown (NULL) if either operand is NULL.
    static void PropagateSqlComparisonNulls(BaseVector *leftArg, BaseVector *rightArg, Vector<bool> *out, int32_t rowSize)
    {
        bool leftIsConst = leftArg->GetEncoding() == OMNI_ENCODING_CONST;
        bool rightIsConst = rightArg->GetEncoding() == OMNI_ENCODING_CONST;

        bool leftConstNull = leftIsConst && leftArg->IsNull(0);
        bool rightConstNull = rightIsConst && rightArg->IsNull(0);

        if (leftConstNull || rightConstNull) {
            for (int32_t i = 0; i < rowSize; ++i) {
                out->SetNull(i);
            }
            return;
        }

        for (int32_t i = 0; i < rowSize; ++i) {
            if ((!leftIsConst && leftArg->IsNull(i)) || (!rightIsConst && rightArg->IsNull(i))) {
                out->SetNull(i);
            }
        }
    }

    // this method is used to compare between column and constant
    /**
     *
     * @param leftIsConst ture means original expresion is original expression is cmp(const, rightArg), false means cmp(leftArg, constant)
     */
    static void handleConstComparison(BaseVector *vectorArg, ConstVector<T> *constantVector, Vector<bool> * comparedResult, int32_t rowSize, bool leftIsConst) {
        const Cmp cmp;
        auto selectivity = std::make_shared<SelectivityVector>(rowSize);

        // we always think value type is same with const type
        using ValueType = std::conditional_t<(kind == OMNI_VARCHAR || kind == OMNI_CHAR || kind == OMNI_VARBINARY), std::string_view, T>;
        ValueType constValue;
        if constexpr (kind == OMNI_VARCHAR || kind == OMNI_CHAR || kind == OMNI_VARBINARY) {
            constValue = reinterpret_cast<ConstVector<std::string_view>*>(constantVector)->GetConstValue();
        } else {
            constValue = constantVector->GetConstValue();
        }

        if (vectorArg->GetEncoding() == OMNI_ENCODING_CONST) {
            ValueType otherVal;
            if constexpr (kind == OMNI_VARCHAR || kind == OMNI_CHAR || kind == OMNI_VARBINARY) {
                otherVal = reinterpret_cast<ConstVector<std::string_view>*>(vectorArg)->GetConstValue();
            } else {
                otherVal = reinterpret_cast<ConstVector<T>*>(vectorArg)->GetConstValue();
            }
            bool cmpResult = leftIsConst ? cmp(constValue, otherVal) : cmp(otherVal, constValue);
            for (int32_t i = 0; i < rowSize; ++i) {
                comparedResult->SetValue(i, cmpResult);
            }
        } else if (vectorArg->GetEncoding() == OMNI_DICTIONARY) {
            auto* vector = static_cast<DictionaryVectorType*>(vectorArg);
            const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(vector));
            selectivity->setFromBitsNegate(nullBits, rowSize);
            selectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, leftIsConst ? cmp(constValue, vector->GetValue(i)) : cmp(vector->GetValue(i), constValue));
            });
        } else {
            auto* vector = static_cast<FlatVectorType*>(vectorArg);
            const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(vector));
            selectivity->setFromBitsNegate(nullBits, rowSize);
            selectivity->applyToSelected([&](vector_size_t i) {
                comparedResult->SetValue(i, leftIsConst ? cmp(constValue, vector->GetValue(i)) : cmp(vector->GetValue(i), constValue));
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
        // Default false for rows that will be written; rows with unknown (NULL) result are marked via PropagateSqlComparisonNulls.
        memset(alignedBuffer, 0, rowSize);
        if (leftArg->GetEncoding() == OMNI_ENCODING_CONST || rightArg->GetEncoding() == OMNI_ENCODING_CONST) {
            // Fast path for (flat, const).
            auto leftIsConst = leftArg->GetEncoding() == OMNI_ENCODING_CONST;
            auto constant =  leftIsConst ? reinterpret_cast<ConstVector<T> *>(leftArg) : reinterpret_cast<ConstVector<T> *>(rightArg);
            handleConstComparison(leftIsConst ? rightArg : leftArg, constant, flatResult, rowSize, leftIsConst);
        } else if (leftArg->GetEncoding() == OMNI_FLAT && rightArg->GetEncoding() == OMNI_FLAT) {
            // Fast path for (flat, flat).
            handleFlatColumnComparison(leftArg, rightArg, flatResult, rowSize);
        } else if (leftArg->GetEncoding() == OMNI_DICTIONARY && rightArg->GetEncoding() == OMNI_DICTIONARY) {
            // Fast path for (dict, dict).
            handleDictColumnComparison(leftArg, rightArg, flatResult, rowSize);
        } else if (leftArg->GetEncoding() == OMNI_DICTIONARY && rightArg->GetEncoding() == OMNI_FLAT
            || leftArg->GetEncoding() == OMNI_FLAT && rightArg->GetEncoding() == OMNI_DICTIONARY) {
            // Fast path for (dict, flat) or (flat, dict)
            handleColumnWithDiffEncodingComparison(leftArg, rightArg, flatResult, rowSize);
        } else {
            // Path if one or more arguments are encoded.
            OMNI_THROW("ComparisonFunction Error:", "Not support decoded vector");
        }
        PropagateSqlComparisonNulls(leftArg, rightArg, flatResult, rowSize);
        result = flatResult;
        delete rightArg;
        delete leftArg;
    }
};

template <template <typename> class Cmp, typename StdCmp>
std::shared_ptr<VectorFunction> MakeImpl(const std::string &functionName, const std::vector<DataTypeId> &args)
{
    OMNI_CHECK(args.size()==2, "Compare must have 2 arg!");
    for (size_t i = 1; i < args.size(); i++) {
        OMNI_CHECK(args[i] == args[0], "Compare must have the same type!");
    }
    switch (args[0]) {
        case OMNI_BOOLEAN:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_BOOLEAN>>();
        case OMNI_BYTE:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_BYTE>>();
        case OMNI_SHORT:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_SHORT>>();
        case OMNI_INT:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_INT>>();
        case OMNI_LONG:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_LONG>>();
        case OMNI_FLOAT:
            return std::make_shared<ComparisonFunction<Cmp<double>, OMNI_FLOAT>>();
        case OMNI_DOUBLE:
            return std::make_shared<ComparisonFunction<Cmp<double>, OMNI_DOUBLE>>();
        case OMNI_VARCHAR:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_VARCHAR>>();
        case OMNI_DATE32:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_DATE32>>();
        case OMNI_DATE64:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_DATE64>>();
        case OMNI_TIMESTAMP:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_TIMESTAMP>>();
        case OMNI_DECIMAL64:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_DECIMAL64>>();
        case OMNI_DECIMAL128:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_DECIMAL128>>();
        case OMNI_CHAR:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_CHAR>>();
        case OMNI_VARBINARY:
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_VARBINARY>>();
#ifdef STRINGVIEW_ENABLE
        case OMNI_STRING_VIEW:
            // StringView is a 16-byte fixed-width value type, so the template uses the default
            // (non-isStringLike) branch:
            // T=StringView, FlatVectorType=Vector<StringView>, ConstVector<StringView>。
            // StringView provides operator==, operator<, and compare, so std::equal_to<> and
            // std::less<> work directly.
            return std::make_shared<ComparisonFunction<StdCmp, OMNI_STRING_VIEW>>();
#endif
        default: OMNI_THROW("Compare error:", "{} Not support type!", functionName);
    }
}

std::shared_ptr<VectorFunction> makeEqualTo(const std::string &name, const std::vector<DataTypeId> &inputArgs,
    const config::QueryConfig &)
{
    return MakeImpl<Equal, std::equal_to<>>(name, inputArgs);
}

std::shared_ptr<VectorFunction> makeNotEqualTo(const std::string &name, const std::vector<DataTypeId> &inputArgs,
    const config::QueryConfig &)
{
    return MakeImpl<NotEqual, std::not_equal_to<>>(name, inputArgs);
}

std::shared_ptr<VectorFunction> makeLessThan(const std::string &name, const std::vector<DataTypeId> &inputArgs,
    const config::QueryConfig &)
{
    return MakeImpl<Less, std::less<>>(name, inputArgs);
}

std::shared_ptr<VectorFunction> makeGreaterThan(const std::string &name, const std::vector<DataTypeId> &inputArgs,
    const config::QueryConfig &)
{
    return MakeImpl<Greater, std::greater<>>(name, inputArgs);
}

std::shared_ptr<VectorFunction> makeLessThanOrEqual(const std::string &name, const std::vector<DataTypeId> &inputArgs,
    const config::QueryConfig &)
{
    return MakeImpl<LessOrEqual, std::less_equal<>>(name, inputArgs);
}

std::shared_ptr<VectorFunction> makeGreaterThanOrEqual(const std::string &name,
    const std::vector<DataTypeId> &inputArgs, const config::QueryConfig &)
{
    return MakeImpl<GreaterOrEqual, std::greater_equal<>>(name, inputArgs);
}

namespace {
enum class SqlEq { kFalse, kTrue, kUnknown };

int32_t RowIndex(BaseVector *vector, int32_t row)
{
    return vector->GetEncoding() == OMNI_ENCODING_CONST ? 0 : row;
}

SqlEq EqAt(BaseVector *left, BaseVector *right, int32_t row, DataTypeId typeId);

SqlEq EqArray(BaseVector *left, BaseVector *right, int32_t row)
{
    const int32_t leftRow = RowIndex(left, row);
    const int32_t rightRow = RowIndex(right, row);
    if (left->IsNull(leftRow) || right->IsNull(rightRow)) {
        return SqlEq::kUnknown;
    }
    auto *leftArr = static_cast<ArrayVector *>(left);
    auto *rightArr = static_cast<ArrayVector *>(right);
    const int64_t leftLen = leftArr->GetSize(leftRow);
    const int64_t rightLen = rightArr->GetSize(rightRow);
    if (leftLen != rightLen) {
        return SqlEq::kFalse;
    }
    if (leftLen == 0) {
        return SqlEq::kTrue;
    }
    std::unique_ptr<BaseVector> leftElems(leftArr->GetValue(leftRow));
    std::unique_ptr<BaseVector> rightElems(rightArr->GetValue(rightRow));
    const DataTypeId elemType = leftArr->GetElementVector()->GetTypeId();
    bool unknown = false;
    for (int64_t i = 0; i < leftLen; ++i) {
        const SqlEq eq = EqAt(leftElems.get(), rightElems.get(), static_cast<int32_t>(i), elemType);
        if (eq == SqlEq::kFalse) {
            return SqlEq::kFalse;
        }
        if (eq == SqlEq::kUnknown) {
            unknown = true;
        }
    }
    return unknown ? SqlEq::kUnknown : SqlEq::kTrue;
}

SqlEq EqRow(BaseVector *left, BaseVector *right, int32_t row)
{
    const int32_t leftRow = RowIndex(left, row);
    const int32_t rightRow = RowIndex(right, row);
    if (left->IsNull(leftRow) || right->IsNull(rightRow)) {
        return SqlEq::kUnknown;
    }
    auto *leftRowVec = static_cast<RowVector *>(left);
    auto *rightRowVec = static_cast<RowVector *>(right);
    const int32_t childCount = leftRowVec->ChildSize();
    if (childCount != rightRowVec->ChildSize()) {
        return SqlEq::kFalse;
    }
    bool unknown = false;
    for (int32_t childIdx = 0; childIdx < childCount; ++childIdx) {
        BaseVector *leftChild = leftRowVec->ChildAt(childIdx).get();
        BaseVector *rightChild = rightRowVec->ChildAt(childIdx).get();
        const SqlEq eq = EqAt(leftChild, rightChild, row, leftChild->GetTypeId());
        if (eq == SqlEq::kFalse) {
            return SqlEq::kFalse;
        }
        if (eq == SqlEq::kUnknown) {
            unknown = true;
        }
    }
    return unknown ? SqlEq::kUnknown : SqlEq::kTrue;
}

template <typename T>
SqlEq EqScalar(BaseVector *left, BaseVector *right, int32_t leftRow, int32_t rightRow)
{
    const T l = VectorHelper::GetValueFromVector<T>(left, leftRow);
    const T r = VectorHelper::GetValueFromVector<T>(right, rightRow);
    return l == r ? SqlEq::kTrue : SqlEq::kFalse;
}

SqlEq EqAt(BaseVector *left, BaseVector *right, int32_t row, DataTypeId typeId)
{
    const int32_t leftRow = RowIndex(left, row);
    const int32_t rightRow = RowIndex(right, row);
    if (left->IsNull(leftRow) || right->IsNull(rightRow)) {
        return SqlEq::kUnknown;
    }
    switch (typeId) {
        case OMNI_BOOLEAN:
            return EqScalar<bool>(left, right, leftRow, rightRow);
        case OMNI_BYTE:
            return EqScalar<int8_t>(left, right, leftRow, rightRow);
        case OMNI_SHORT:
            return EqScalar<int16_t>(left, right, leftRow, rightRow);
        case OMNI_INT:
        case OMNI_DATE32:
            return EqScalar<int32_t>(left, right, leftRow, rightRow);
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            return EqScalar<int64_t>(left, right, leftRow, rightRow);
        case OMNI_FLOAT:
            return EqScalar<float>(left, right, leftRow, rightRow);
        case OMNI_DOUBLE:
            return EqScalar<double>(left, right, leftRow, rightRow);
        case OMNI_DECIMAL128:
            return EqScalar<Decimal128>(left, right, leftRow, rightRow);
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY: {
            const auto l = VectorHelper::GetStringValueFromVector(left, leftRow);
            const auto r = VectorHelper::GetStringValueFromVector(right, rightRow);
            return l == r ? SqlEq::kTrue : SqlEq::kFalse;
        }
        case OMNI_ARRAY:
            return EqArray(left, right, row);
        case OMNI_ROW:
            return EqRow(left, right, row);
        default:
            OMNI_THROW("Compare error:", "unsupported complex equality type {}", static_cast<int>(typeId));
    }
}

class SqlComplexEqual final : public VectorFunction {
public:
    explicit SqlComplexEqual(bool invert) : invert_(invert) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        (void)outputType;
        BaseVector *left = args.top();
        args.pop();
        BaseVector *right = args.top();
        args.pop();
        const int32_t size = static_cast<int32_t>(context->GetResultRowSize());
        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);
        }
        auto *out = static_cast<Vector<bool> *>(result);
        out->SetNulls(0, false, size);
        for (int32_t i = 0; i < size; ++i) {
            SqlEq eq = EqAt(left, right, i, left->GetTypeId());
            if (invert_ && eq != SqlEq::kUnknown) {
                eq = eq == SqlEq::kTrue ? SqlEq::kFalse : SqlEq::kTrue;
            }
            if (eq == SqlEq::kUnknown) {
                out->SetNull(i);
            } else {
                out->SetValue(i, eq == SqlEq::kTrue);
            }
        }
        delete left;
        delete right;
    }

private:
    bool invert_;
};
} // namespace

void RegisterComplexEquality()
{
    auto equal = std::make_shared<SqlComplexEqual>(false);
    auto notEqual = std::make_shared<SqlComplexEqual>(true);
    VectorFunction::RegisterVectorFunction("equal", {OMNI_ARRAY, OMNI_ARRAY}, OMNI_BOOLEAN, equal);
    VectorFunction::RegisterVectorFunction("notEqual", {OMNI_ARRAY, OMNI_ARRAY}, OMNI_BOOLEAN, notEqual);
    VectorFunction::RegisterVectorFunction("equal", {OMNI_ROW, OMNI_ROW}, OMNI_BOOLEAN, equal);
    VectorFunction::RegisterVectorFunction("notEqual", {OMNI_ROW, OMNI_ROW}, OMNI_BOOLEAN, notEqual);
}
}
