/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 */
#include "reader/text/TextValueConverter.h"

#include <stack>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "codegen/func_signature.h"
#include "operator/execution_context.h"
#include "vector/vector_helper.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/CastExpr.h"
#include "vectorization/registration/Register.h"

namespace omniruntime::reader::text {

TextValueConverter::TextValueConverter(std::string sessionTimezone)
    : queryConfig_([&sessionTimezone]() {
        std::unordered_map<std::string, std::string> values;
        if (!sessionTimezone.empty()) {
            values[config::QueryConfig::kSessionTimezone] = std::move(sessionTimezone);
        }
        values[config::QueryConfig::kAdjustTimestampToTimezone] = "true";
        return values;
    }())
{}

std::unique_ptr<vec::BaseVector> TextValueConverter::CastOwned(
    std::unique_ptr<vec::BaseVector> input,
    const type::DataTypePtr& fromType,
    const type::DataTypePtr& toType) const
{
    if (input == nullptr) {
        throw std::runtime_error("Text value conversion input is null.");
    }
    if (fromType->GetId() == toType->GetId()) {
        return input;
    }
    op::ExecutionContext context(4096, input->GetPool());
    context.SetConfig(queryConfig_);
    context.SetResultRowSize(input->GetSize());
    context.SetThrowOnError(false);
    std::stack<vec::BaseVector*> arguments;
    auto* ownedInput = input.release();
    arguments.push(ownedInput);
    vec::BaseVector* result = nullptr;
    vectorization::CastExpr cast(
        fromType,
        toType,
        true,
        std::make_shared<vectorization::CastHooks>(queryConfig_));
    try {
        cast.Apply(arguments, toType, result, &context);
    } catch (...) {
        delete ownedInput;
        throw;
    }
    if (result == nullptr) {
        throw std::runtime_error("Text value conversion returned a null vector.");
    }
    return std::unique_ptr<vec::BaseVector>(result);
}

std::unique_ptr<vec::BaseVector> TextValueConverter::DecodeColumn(
    std::unique_ptr<vec::BaseVector> strings,
    const type::DataTypePtr& targetType,
    const std::string& dateFormat,
    const std::vector<std::string>& timestampFormats) const
{
    if (targetType->GetId() == type::OMNI_DATE32 && !dateFormat.empty()) {
        auto timestamp = DecodeTimestampFormats(std::move(strings), {dateFormat});
        return CastOwned(std::move(timestamp), type::TimestampType(), targetType);
    }
    if (targetType->GetId() == type::OMNI_TIMESTAMP && !timestampFormats.empty()) {
        return DecodeTimestampFormats(std::move(strings), timestampFormats);
    }
    return CastOwned(
        std::move(strings), std::make_shared<type::VarcharDataType>(), targetType);
}

std::unique_ptr<vec::BaseVector> TextValueConverter::EncodeColumn(
    vec::BaseVector* source,
    const type::DataTypePtr& sourceType,
    int64_t start,
    int64_t end,
    const std::string& dateFormat,
    const std::string& timestampFormat) const
{
    if (source == nullptr || start < 0 || end < start || end > source->GetSize()) {
        throw std::runtime_error("Text writer row range is invalid.");
    }
    std::vector<int> positions(static_cast<size_t>(end - start));
    for (size_t index = 0; index < positions.size(); ++index) {
        positions[index] = static_cast<int>(start + index);
    }
    std::unique_ptr<vec::BaseVector> copied(vec::VectorHelper::CopyPositionsVector(
        source, positions.data(), 0, static_cast<int>(positions.size())));
    if (sourceType->GetId() == type::OMNI_DATE32 && !dateFormat.empty()) {
        auto timestamp = CastOwned(std::move(copied), sourceType, type::TimestampType());
        return ApplyFormatFunction("DateFormat", std::move(timestamp),
            type::OMNI_TIMESTAMP, type::OMNI_VARCHAR, dateFormat);
    }
    if (sourceType->GetId() == type::OMNI_TIMESTAMP && !timestampFormat.empty()) {
        return ApplyFormatFunction("DateFormat", std::move(copied),
            type::OMNI_TIMESTAMP, type::OMNI_VARCHAR, timestampFormat);
    }
    return CastOwned(
        std::move(copied), sourceType, std::make_shared<type::VarcharDataType>());
}

std::unique_ptr<vec::BaseVector> TextValueConverter::ApplyFormatFunction(
    const std::string& functionName,
    std::unique_ptr<vec::BaseVector> input,
    type::DataTypeId inputType,
    type::DataTypeId outputType,
    const std::string& format) const
{
    vectorization::link_register_functions();
    auto signature = std::make_shared<codegen::FunctionSignature>(
        functionName,
        std::vector<type::DataTypeId>{inputType, type::OMNI_VARCHAR},
        outputType);
    auto function = vectorization::VectorFunction::Find(signature, queryConfig_);
    if (function == nullptr) {
        throw std::runtime_error("Text time conversion function is not registered: " + functionName);
    }
    op::ExecutionContext context(4096, input->GetPool());
    context.SetConfig(queryConfig_);
    context.SetResultRowSize(input->GetSize());
    context.SetThrowOnError(false);
    std::stack<vec::BaseVector*> arguments;
    arguments.push(input.release());
    arguments.push(new vec::ConstVector<std::string_view>(
        std::string_view(format), type::OMNI_VARCHAR, context.GetResultRowSize(), context.GetPool()));
    vec::BaseVector* result = nullptr;
    const auto resultType = outputType == type::OMNI_TIMESTAMP
        ? type::TimestampType()
        : type::VarcharType();
    function->Apply(arguments, resultType, result, &context);
    if (result == nullptr) {
        throw std::runtime_error("Text time conversion returned a null vector.");
    }
    return std::unique_ptr<vec::BaseVector>(result);
}

std::unique_ptr<vec::BaseVector> TextValueConverter::DecodeTimestampFormats(
    std::unique_ptr<vec::BaseVector> strings,
    const std::vector<std::string>& formats) const
{
    if (formats.size() == 1) {
        return ApplyFormatFunction("get_timestamp", std::move(strings),
            type::OMNI_VARCHAR, type::OMNI_TIMESTAMP, formats.front());
    }
    const auto size = strings->GetSize();
    std::unique_ptr<vec::BaseVector> output(
        vec::VectorHelper::CreateFlatVector(strings->GetPool(), type::OMNI_TIMESTAMP, size));
    auto* timestamps = static_cast<vec::Vector<int64_t>*>(output.get());
    for (int32_t row = 0; row < size; ++row) {
        output->SetNull(row);
    }
    std::vector<int> positions(static_cast<size_t>(size));
    for (int32_t row = 0; row < size; ++row) {
        positions[static_cast<size_t>(row)] = row;
    }
    for (const auto& format : formats) {
        std::unique_ptr<vec::BaseVector> copied(vec::VectorHelper::CopyPositionsVector(
            strings.get(), positions.data(), 0, size));
        auto candidate = ApplyFormatFunction("get_timestamp", std::move(copied),
            type::OMNI_VARCHAR, type::OMNI_TIMESTAMP, format);
        for (int32_t row = 0; row < size; ++row) {
            if (output->IsNull(row) && !candidate->IsNull(row)) {
                timestamps->SetValue(row,
                    vec::VectorHelper::GetValueFromVector<int64_t>(candidate.get(), row));
                output->SetNotNull(row);
            }
        }
    }
    return output;
}

} // namespace omniruntime::reader::text
