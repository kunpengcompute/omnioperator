/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once

#include "VectorFunction.h"
#include "VectorReaders.h"
#include "SelectivityVector.h"
#include "type/data_type.h"
#include "util/config/QueryConfig.h"
#include "vector/unsafe_vector.h"
#include "vectorization/Status.h"
#include "ComplexViewTypes.h"

namespace omniruntime::vectorization {
template <typename T>
inline constexpr bool is_string_type_v = std::is_same_v<T, std::string_view>;

template <typename FUNC>
class SimpleFunction final : public VectorFunction {
    using T = typename FUNC::exec_return_type;
    using return_type_traits = typename FUNC::return_type;

    template <int32_t POSITION>
    using exec_arg_at = std::tuple_element_t<POSITION, typename FUNC::exec_arg_types>;
    template <int32_t POSITION>
    using arg_at = std::tuple_element_t<POSITION, typename FUNC::arg_types>;
    std::unique_ptr<FUNC> fn_;

    struct ApplyContext {
        ApplyContext(SelectivityVector *_rows, const DataTypePtr outputType, op::ExecutionContext *_context)
            : rows{_rows}, context{_context} {}

        template <typename Callable>
        void applyToSelectedNoThrow(Callable func)
        {
            rows->applyToSelected([&](auto row) INLINE_LAMBDA {
                try {
                    func(row);
                } catch (const OmniException &e) {
                    OMNI_THROW("Express Error:", e.what());
                } catch (const std::exception &e) {
                    OMNI_THROW("Express Unknown Error:", e.what());
                }
            });
        }

        int32_t GetResultRowSize() const
        {
            return context->GetResultRowSize();
        }

        void IntersectNull(BaseVector *baseVector) const
        {
            const auto size = baseVector->GetSize();
            const auto nullBits = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(baseVector));
            SelectivityVector newRows(size);
            newRows.setFromBitsNegate(nullBits, size);
            rows->intersect(newRows);
        }

        SelectivityVector *rows;
        std::shared_ptr<BaseVector> result;
        op::ExecutionContext *context;
        bool mayHaveNullsRecursive{false};
    };

    template <int32_t POSITION, typename... Values>
    void UnpackInitialize(const std::vector<DataTypeId> &inputTypes, const config::QueryConfig &config,
        const std::vector<BaseVector *> &packed, const Values *... values) const
    {
        if constexpr (POSITION == FUNC::num_args) {
            return (*fn_).initialize(inputTypes, config, values...);
        } else {
            if (packed.at(POSITION) != nullptr) {
                if constexpr (is_array_view_v<arg_at<POSITION>>) {
                    auto oneReader = VectorReader<Array<typename arg_at<POSITION>::element_t>>(packed.at(POSITION));
                    auto oneValue = oneReader[0];
                    UnpackInitialize<POSITION + 1>(inputTypes, config, packed, values..., &oneValue);
                } else {
                    auto constVector = dynamic_cast<const ConstVector<arg_at<POSITION>> *>(packed.at(POSITION));
                    if (constVector == nullptr) {
                        OMNI_THROW("Runtime Error:", "Initialize vector must be const!");
                    }
                    auto oneValue = constVector->GetConstValue();
                    UnpackInitialize<POSITION + 1>(inputTypes, config, packed, values..., &oneValue);
                }
            } else {
                using temp_type = exec_arg_at<POSITION>;
                UnpackInitialize<POSITION + 1>(inputTypes, config, packed, values...,
                    static_cast<const temp_type *>(nullptr));
            }
        }
    }

public:
    SimpleFunction(const std::vector<DataTypeId> &inputTypes, const config::QueryConfig &config,
        const std::vector<BaseVector *> &constantInputs)
        : fn_{std::make_unique<FUNC>()}
    {
        if constexpr (FUNC::udf_has_initialize) {
            UnpackInitialize<0>(inputTypes, config, constantInputs);
        }
    }

    explicit SimpleFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        result = VectorHelper::CreateFlatVector(outputType->GetId(), context->GetResultRowSize());
        auto rows = std::make_shared<SelectivityVector>(context->GetResultRowSize());
        ApplyContext applyContext(rows.get(), outputType, context);
        unpackSpecializeForAllEncodings<0>(applyContext, result, args);
    }

private:
    // This is called only when we know that all args are flat or constant and are
    // eligible for the optimization and the optimization is enabled.
    template <int32_t POSITION, typename... TReader>
    void unpackSpecializeForAllEncodings(ApplyContext &context, BaseVector *result, std::stack<BaseVector *> &rawArgs,
        TReader &... readers) const
    {
        if constexpr (POSITION == FUNC::num_args) {
            iterate(context, result, readers...);
        } else {
            auto arg = rawArgs.top();
            rawArgs.pop();
            using type = exec_arg_at<FUNC::num_args - POSITION - 1>;
            if (arg->GetEncoding() == OMNI_ENCODING_CONST) {
                if (arg->IsNull(0)) {
                    SelectivityVector emptyRows(context.GetResultRowSize(), false);
                    context.rows->intersect(emptyRows);
                }
            } else {
                context.IntersectNull(arg);
            }
            if constexpr (is_string_type_v<type>) {
                if (arg->GetEncoding() == OMNI_ENCODING_CONST) {
                    auto reader = ConstVectorReader<type>(arg);
                    unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                } else if (arg->GetEncoding() == OMNI_DICTIONARY) {
                    auto reader = DicVectorReader<type>(arg);
                    unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                } else {
                    // string_view use StringVectorReader
                    auto reader = StringVectorReader(arg);
                    unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                }
                return;
            }
            if constexpr (is_array_view_v<type>) {
                auto reader = VectorReader<Array<typename type::element_t>>(arg);
                unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
            } else {
                switch (arg->GetEncoding()) {
                    case OMNI_FLAT: {
                        auto reader = FlatVectorReader<type>(arg);
                        unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                        break;
                    }
                    case OMNI_DICTIONARY: {
                        auto reader = DicVectorReader<type>(arg);
                        unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                        break;
                    }
                    case OMNI_ENCODING_CONST: {
                        auto reader = ConstVectorReader<type>(arg);
                        unpackSpecializeForAllEncodings<POSITION + 1>(context, result, rawArgs, reader, readers...);
                        break;
                    }
                    default: {}
                }
            }
        }
    }

    template <typename... TReader>
    void iterate(ApplyContext &context, BaseVector *result, TReader &... readers) const
    {
        auto rowSize = context.GetResultRowSize();
        auto resultAddr = static_cast<return_type_traits *>(VectorHelper::UnsafeGetValues(result));
        auto nullBuffer = reinterpret_cast<uint64_t *>(unsafe::UnsafeBaseVector::GetNulls(result));
        auto copyNullSize = NullsBuffer::CalculateNbytes(rowSize) - 8;
        memcpy(nullBuffer, context.rows->allBits(), copyNullSize);
        BitUtil::Negate(nullBuffer, rowSize);
        // When a filter is active the inputs have already been compacted to the selected rows
        // by ExprEval, so every row in [0, rowSize) is evaluated and written in place here.
        if constexpr (std::is_same_v<return_type_traits, std::string>) {
            auto tempResult = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
            context.applyToSelectedNoThrow([&](auto row) INLINE_LAMBDA {
                // Passing a stack variable have shown to be boost the performance
                // of functions that repeatedly update the output. The opposite
                // optimization (eliminating the temp) is easier to do by the
                // compiler (assuming the function call is inlined).
                std::string out;
                bool notNull;
                auto status = doApplyNotNull<0>(row, out, notNull, readers...);
                if (!status.ok()) {
                    tempResult->SetNull(row);
                    return;
                }
                if (!notNull) {
                    tempResult->SetNull(row);
                }
                std::string_view tmp(out);
                tempResult->SetValue(row, tmp);
            });
        } else {
            context.applyToSelectedNoThrow([&](auto row) INLINE_LAMBDA {
                // Passing a stack variable have shown to be boost the performance
                // of functions that repeatedly update the output. The opposite
                // optimization (eliminating the temp) is easier to do by the
                // compiler (assuming the function call is inlined).
                return_type_traits out{};
                bool notNull;
                auto status = doApplyNotNull<0>(row, out, notNull, readers...);
                if (!status.ok()) {
                    BitUtil::SetBit(nullBuffer, row, true);
                    return;
                }
                if (!notNull) {
                    BitUtil::SetBit(nullBuffer, row, true);
                }
                resultAddr[row] = out;
            });
        }
    }

    // If we're guaranteed not to have any nulls, pass all parameters as
    // references.
    //
    // Note that (*fn_).call() will internally dispatch the call to either
    // call() or callNullable() (whichever is implemented by the user
    // function). Default null behavior or not does not matter in this path
    // since we don't have any nulls.
    template <size_t POSITION, typename R0, typename... TStuff, std::enable_if_t<POSITION != FUNC::num_args, int32_t>  =
        0>
    ALWAYS_INLINE Status doApplyNotNull(size_t idx, T &target, bool &notNull, R0 &currentReader,
        const TStuff &... extra) const
    {
        if constexpr (std::is_same_v<R0, StringVectorReader>) {
            std::string_view v0 = currentReader[idx];
            return doApplyNotNull<POSITION + 1>(idx, target, notNull, extra..., v0);
        } else {
            decltype(currentReader[idx]) v0 = currentReader[idx];
            return doApplyNotNull<POSITION + 1>(idx, target, notNull, extra..., v0);
        }
    }

    // For default null behavior, Terminate by with UDFHolder::call.
    template <size_t POSITION, typename... Values, std::enable_if_t<POSITION == FUNC::num_args, int32_t>  = 0>
    ALWAYS_INLINE Status doApplyNotNull(size_t /*idx*/, T &target, bool &notNull, const Values &... values) const
    {
        return (*fn_).call(target, notNull, values...);
    }
};
}
