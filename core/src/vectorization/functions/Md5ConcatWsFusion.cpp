/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Matcher for MD5 and concat_ws expression fusion
 */

#include "vectorization/functions/Md5ConcatWsFusion.h"

#include "type/data_type.h"

namespace omniruntime::vectorization {
using namespace omniruntime::expressions;
using namespace omniruntime::type;

std::optional<Md5ConcatWsFusionPlan> Md5ConcatWsFusion::Match(const FuncExpr &expression)
{
    static constexpr char MD5_SUFFIX[] = "Md5";
    static constexpr char CONCAT_WS_NAME[] = "concat_ws";
    if (expression.funcName.size() < 3 ||
        expression.funcName.compare(expression.funcName.size() - 3, 3, MD5_SUFFIX) != 0 ||
        expression.arguments.size() != 1 || expression.GetReturnTypeId() != OMNI_VARCHAR) {
        return std::nullopt;
    }

    const auto *castFunction = dynamic_cast<const FuncExpr *>(expression.arguments[0]);
    if (castFunction == nullptr || castFunction->funcName != "CAST" ||
        castFunction->arguments.size() != 1 || castFunction->GetReturnTypeId() != OMNI_VARBINARY ||
        castFunction->arguments[0]->GetReturnTypeId() != OMNI_VARCHAR) {
        return std::nullopt;
    }
    const auto *childFunction = dynamic_cast<const FuncExpr *>(castFunction->arguments[0]);

    if (childFunction == nullptr || childFunction->funcName != CONCAT_WS_NAME || childFunction->arguments.empty() ||
        childFunction->GetReturnTypeId() != OMNI_VARCHAR) {
        return std::nullopt;
    }
    for (const Expr *argument : childFunction->arguments) {
        if (argument->GetReturnTypeId() != OMNI_VARCHAR) {
            return std::nullopt;
        }
    }

    const std::string prefix = expression.funcName.substr(0, expression.funcName.size() - 3);
    return Md5ConcatWsFusionPlan {childFunction, prefix + "FusedMd5ConcatWs"};
}

} // namespace omniruntime::vectorization
