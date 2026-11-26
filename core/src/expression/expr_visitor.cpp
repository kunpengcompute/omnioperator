/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: visitor accept methods
 */

#include "expr_visitor.h"

using namespace omniruntime::expressions;

void LiteralExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void FieldExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void BinaryExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void InExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void BetweenExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void SwitchExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void IfExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void CoalesceExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void IsNullExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void FuncExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }

void UnaryExpr::Accept(ExprVisitor &visitor) const { return visitor.Visit(*this); }
