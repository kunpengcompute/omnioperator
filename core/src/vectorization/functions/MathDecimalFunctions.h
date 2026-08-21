/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Stateful vectorized math functions for DECIMAL inputs.
 *
 * Mirrors Flink's native "function overload + internal doubleValue descale"
 * pattern for math functions on decimals, e.g.
 *   SqlFunctionUtils.asin(DecimalData a)      { return Math.asin(doubleValue(a)); }
 *   SqlFunctionUtils.degrees(DecimalData a)   { return Math.toDegrees(doubleValue(a)); }
 *   SqlFunctionUtils.atan2(DecimalData y, x)  { return Math.atan2(doubleValue(y), doubleValue(x)); }
 * where doubleValue descales the unscaled integer by 10^scale.
 *
 * Unlike the stateless SimpleFunction templates (AsinFunction<T>), these are
 * stateful VectorFunctions that hold the input DECIMAL DataType(s), because the
 * per-row value only carries the unscaled integer while scale is type metadata
 * (Decimal64DataType/Decimal128DataType::GetScale()). This follows the same
 * pattern as CastFunction (constructed with the operand DataType).
 *
 * Generic classes are parameterized by the underlying double op so that adding a
 * new decimal math function is one table entry (DecimalMathFunctionTable for unary,
 * DecimalBinaryMathFunctionTable for binary), not a new class or a new FuncExpr branch.
 *
 * Two categories live here:
 *   1. DECIMAL -> DOUBLE  (asin/degrees/atan2/...): descale operands then apply a double op.
 *   2. DECIMAL -> DECIMAL (sign): scale-preserving, mirrors DecimalDataUtils.sign
 *      (result = signum * 10^scale, same precision/scale). NOT a descale-to-double op.
 * Functions like round (scale-CHANGING via BigDecimal) are NOT here yet.
 */
#ifndef OMNI_VECTORIZATION_MATH_DECIMAL_FUNCTIONS_H
#define OMNI_VECTORIZATION_MATH_DECIMAL_FUNCTIONS_H

#pragma once

#include <cmath>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "type/decimal128.h"
#include "operator/execution_context.h"

namespace omniruntime::vectorization {

/// Scalar kernels operating on the descaled double(s).
using UnaryDoubleMathOp = double (*)(double);
using BinaryDoubleMathOp = double (*)(double, double);

inline bool IsDecimalTypeId(type::DataTypeId id)
{
    return id == type::OMNI_DECIMAL64 || id == type::OMNI_DECIMAL128;
}

/// 10^scale for a DECIMAL DataType (used to descale the unscaled integer to double).
inline double DecimalScaleFactor(const type::DataTypePtr &decimalType)
{
    const int32_t scale = static_cast<type::DecimalDataType *>(decimalType.get())->GetScale();
    return std::pow(10.0, static_cast<double>(scale));
}

/// Descale one row's unscaled DECIMAL value to double (unscaled / 10^scale), matching Flink
/// DecimalDataUtils.doubleValue compact path. GetValueFromVector handles CONST/FLAT/DICTIONARY.
inline double DescaleDecimalToDouble(vec::BaseVector *input, int32_t row, type::DataTypeId id, double factor)
{
    if (id == type::OMNI_DECIMAL64) {
        return static_cast<double>(vec::VectorHelper::GetValueFromVector<int64_t>(input, row)) / factor;
    }
    return static_cast<double>(
        vec::VectorHelper::GetValueFromVector<type::Decimal128>(input, row).ToInt128()) / factor;
}

/// 10^scale as a 128-bit integer (the unscaled representation of 1.0 at that scale). scale is a
/// valid DECIMAL scale (0..38), and 10^38 < INT128_MAX, so this never overflows.
inline __int128 DecimalPowerOfTen(int32_t scale)
{
    __int128 p = 1;
    for (int32_t i = 0; i < scale; ++i) {
        p *= 10;
    }
    return p;
}

/// Generic stateful unary math on DECIMAL64/DECIMAL128 -> DOUBLE.
/// Descales the input then applies `op_`. Out-of-domain inputs produce NaN, consistent with
/// the DOUBLE overloads and Flink.
class DecimalToDoubleMathFunction : public VectorFunction {
public:
    DecimalToDoubleMathFunction(const type::DataTypePtr &inputType, UnaryDoubleMathOp op)
        : inputType_(inputType), op_(op)
    {}

    ~DecimalToDoubleMathFunction() override = default;

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override
    {
        auto input = args.top();
        args.pop();

        const int32_t size = context->GetResultRowSize();
        const double factor = DecimalScaleFactor(inputType_);
        const type::DataTypeId fromId = inputType_->GetId();
        const bool isConst = input->GetEncoding() == vec::OMNI_ENCODING_CONST;

        result = vec::VectorHelper::CreateFlatVector(type::OMNI_DOUBLE, size);
        auto resultVec = static_cast<vec::Vector<double> *>(result);

        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(isConst ? 0 : row)) {
                result->SetNull(row);
                continue;
            }
            resultVec->SetValue(row, op_(DescaleDecimalToDouble(input, row, fromId, factor)));
        }

        // Apply owns the popped operand vectors; always newly-allocated result above.
        if (input != nullptr) {
            delete input;
        }
    }

private:
    type::DataTypePtr inputType_;
    UnaryDoubleMathOp op_;
};

/// Generic stateful binary math on (DECIMAL, DECIMAL) -> DOUBLE, e.g. atan2.
/// Holds both operand DataTypes (each with its own scale). Descales both then applies `op_`.
/// Result is NULL if either operand is NULL (SQL null propagation).
class BinaryDecimalToDoubleMathFunction : public VectorFunction {
public:
    BinaryDecimalToDoubleMathFunction(const type::DataTypePtr &inputType0,
        const type::DataTypePtr &inputType1, BinaryDoubleMathOp op)
        : inputType0_(inputType0), inputType1_(inputType1), op_(op)
    {}

    ~BinaryDecimalToDoubleMathFunction() override = default;

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override
    {
        // Arguments are pushed in order, so the last argument is on top of the stack.
        // For f(arg0, arg1): top = arg1, next = arg0.
        auto in1 = args.top();
        args.pop();
        auto in0 = args.top();
        args.pop();

        const int32_t size = context->GetResultRowSize();
        const double factor0 = DecimalScaleFactor(inputType0_);
        const double factor1 = DecimalScaleFactor(inputType1_);
        const type::DataTypeId id0 = inputType0_->GetId();
        const type::DataTypeId id1 = inputType1_->GetId();
        const bool const0 = in0->GetEncoding() == vec::OMNI_ENCODING_CONST;
        const bool const1 = in1->GetEncoding() == vec::OMNI_ENCODING_CONST;

        result = vec::VectorHelper::CreateFlatVector(type::OMNI_DOUBLE, size);
        auto resultVec = static_cast<vec::Vector<double> *>(result);

        for (int32_t row = 0; row < size; ++row) {
            if (in0->IsNull(const0 ? 0 : row) || in1->IsNull(const1 ? 0 : row)) {
                result->SetNull(row);
                continue;
            }
            double a0 = DescaleDecimalToDouble(in0, row, id0, factor0);
            double a1 = DescaleDecimalToDouble(in1, row, id1, factor1);
            resultVec->SetValue(row, op_(a0, a1));
        }

        if (in0 != nullptr) {
            delete in0;
        }
        if (in1 != nullptr) {
            delete in1;
        }
    }

private:
    type::DataTypePtr inputType0_;
    type::DataTypePtr inputType1_;
    BinaryDoubleMathOp op_;
};

/// Unary decimal-input math: funcName -> double->double kernel. Add one line to enable a
/// function for DECIMAL64/DECIMAL128 inputs. Kernels MUST mirror the DOUBLE-overload impl in
/// MathFunctions.h exactly (incl. NaN/Inf at domain boundaries). In particular `cot` uses
/// `1 / std::tan(x)` (matching CotFunction::call), and degrees/radians use the same
/// `x*(180/π)` / `x*(π/180)` as Degrees/RadiansFunction. Captureless lambdas disambiguate the
/// overloaded std:: math functions and decay to plain function pointers.
inline const std::unordered_map<std::string, UnaryDoubleMathOp> &DecimalMathFunctionTable()
{
    static const std::unordered_map<std::string, UnaryDoubleMathOp> table = {
        {"asin",    [](double x) { return std::asin(x); }},
        {"acos",    [](double x) { return std::acos(x); }},
        {"atan",    [](double x) { return std::atan(x); }},
        {"sin",     [](double x) { return std::sin(x); }},
        {"sinh",    [](double x) { return std::sinh(x); }},
        {"cos",     [](double x) { return std::cos(x); }},
        {"cosh",    [](double x) { return std::cosh(x); }},
        {"tan",     [](double x) { return std::tan(x); }},
        {"tanh",    [](double x) { return std::tanh(x); }},
        {"cot",     [](double x) { return 1.0 / std::tan(x); }},
        {"degrees", [](double x) { return x * (180.0 / M_PI); }},
        {"radians", [](double x) { return x * (M_PI / 180.0); }},
    };
    return table;
}

/// Binary decimal-input math: funcName -> double,double->double kernel. atan2(y, x) mirrors
/// Atan2Function (std::atan2). Arguments keep Flink/Java order: op(descale(arg0), descale(arg1)).
inline const std::unordered_map<std::string, BinaryDoubleMathOp> &DecimalBinaryMathFunctionTable()
{
    static const std::unordered_map<std::string, BinaryDoubleMathOp> table = {
        {"atan2", [](double y, double x) { return std::atan2(y, x); }},
    };
    return table;
}

/// Returns a stateful decimal math VectorFunction if `name`+arity is a supported decimal-input
/// math function AND every operand is DECIMAL64/DECIMAL128, else nullptr. Handles unary (1 arg)
/// and binary (2 args). The returned function is scale-aware (built from the operand DataTypes).
inline std::shared_ptr<VectorFunction> TryCreateDecimalMathFunction(
    const std::string &name, const std::vector<type::DataTypePtr> &argTypes)
{
    for (const auto &t : argTypes) {
        if (t == nullptr || !IsDecimalTypeId(t->GetId())) {
            return nullptr;
        }
    }
    if (argTypes.size() == 1) {
        const auto &table = DecimalMathFunctionTable();
        auto it = table.find(name);
        if (it == table.end()) {
            return nullptr;
        }
        return std::make_shared<DecimalToDoubleMathFunction>(argTypes[0], it->second);
    }
    if (argTypes.size() == 2) {
        const auto &table = DecimalBinaryMathFunctionTable();
        auto it = table.find(name);
        if (it == table.end()) {
            return nullptr;
        }
        return std::make_shared<BinaryDecimalToDoubleMathFunction>(argTypes[0], argTypes[1], it->second);
    }
    return nullptr;
}

/// sign(DECIMAL(p,s)) -> DECIMAL(p,s). Scale-preserving, mirrors Flink DecimalDataUtils.sign:
/// result unscaled = signum(input) * 10^scale (i.e. 1.0000 / -1.0000 / 0.0000 at scale s), same
/// precision/scale as the input. Holds the input DataType to read scale (and pick DEC64 vs DEC128).
class SignDecimalFunction : public VectorFunction {
public:
    explicit SignDecimalFunction(const type::DataTypePtr &inputType) : inputType_(inputType) {}

    ~SignDecimalFunction() override = default;

    void Apply(std::stack<vec::BaseVector *> &args, const type::DataTypePtr &outputType,
        vec::BaseVector *&result, op::ExecutionContext *context) const override
    {
        auto input = args.top();
        args.pop();

        const int32_t size = context->GetResultRowSize();
        const type::DataTypeId id = inputType_->GetId();
        const int32_t scale = static_cast<type::DecimalDataType *>(inputType_.get())->GetScale();
        const __int128 one = DecimalPowerOfTen(scale);  // unscaled representation of 1.0
        const bool isConst = input->GetEncoding() == vec::OMNI_ENCODING_CONST;

        result = vec::VectorHelper::CreateFlatVector(id, size);

        if (id == type::OMNI_DECIMAL64) {
            auto resultVec = static_cast<vec::Vector<int64_t> *>(result);
            const int64_t oneI64 = static_cast<int64_t>(one);
            for (int32_t row = 0; row < size; ++row) {
                if (input->IsNull(isConst ? 0 : row)) {
                    result->SetNull(row);
                    continue;
                }
                int64_t v = vec::VectorHelper::GetValueFromVector<int64_t>(input, row);
                int32_t signum = (v > 0) - (v < 0);
                resultVec->SetValue(row, static_cast<int64_t>(signum) * oneI64);
            }
        } else {
            auto resultVec = static_cast<vec::Vector<type::Decimal128> *>(result);
            for (int32_t row = 0; row < size; ++row) {
                if (input->IsNull(isConst ? 0 : row)) {
                    result->SetNull(row);
                    continue;
                }
                __int128 v = vec::VectorHelper::GetValueFromVector<type::Decimal128>(input, row).ToInt128();
                int32_t signum = (v > 0) - (v < 0);
                resultVec->SetValue(row, type::Decimal128(static_cast<__int128>(signum) * one));
            }
        }

        if (input != nullptr) {
            delete input;
        }
    }

private:
    type::DataTypePtr inputType_;
};

/// Returns a scale-preserving DECIMAL->DECIMAL VectorFunction if `name`+arity is supported and
/// every operand is DECIMAL64/DECIMAL128, else nullptr. Currently: sign.
inline std::shared_ptr<VectorFunction> TryCreateDecimalToDecimalFunction(
    const std::string &name, const std::vector<type::DataTypePtr> &argTypes)
{
    for (const auto &t : argTypes) {
        if (t == nullptr || !IsDecimalTypeId(t->GetId())) {
            return nullptr;
        }
    }
    if (name == "sign" && argTypes.size() == 1) {
        return std::make_shared<SignDecimalFunction>(argTypes[0]);
    }
    return nullptr;
}

} // namespace omniruntime::vectorization

#endif // OMNI_VECTORIZATION_MATH_DECIMAL_FUNCTIONS_H
