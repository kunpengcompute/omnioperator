/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Predicate JSON → ScanSpec; layering aligned with Velox HiveConnectorUtil / ExprToSubfieldFilter.

#include "ScanSpecBuilder.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "reader/common/Filter.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/PredicateUtil.h"
#include "util/debug.h"

namespace omniruntime::reader {

using omniruntime::codegen::ScanSpec;
using ::common::FilterPtr;

bool allSelectedColumnsAreSupported(const omniruntime::type::RowType &rowType)
{
    for (int i = 0; i < rowType.size(); ++i) {
        switch (rowType.childAt(i)->GetId()) {
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_LONG:
            case omniruntime::type::OMNI_SHORT:
            case omniruntime::type::OMNI_DATE32:
            case omniruntime::type::OMNI_BOOLEAN:
            case omniruntime::type::OMNI_DOUBLE:
            case omniruntime::type::OMNI_DECIMAL64:
            case omniruntime::type::OMNI_DECIMAL128:
            case omniruntime::type::OMNI_BYTE:
            case omniruntime::type::OMNI_FLOAT:
            case omniruntime::type::OMNI_VARCHAR:
            case omniruntime::type::OMNI_CHAR:
            case omniruntime::type::OMNI_VARBINARY:
            case omniruntime::type::OMNI_TIMESTAMP:
                break;
            default:
                return false;
        }
    }
    return true;
}

namespace {
using namespace ::common;
using omniruntime::type::DataTypeId;

constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

bool parseBool(const std::string &literal)
{
    if (literal == "true" || literal == "1") {
        return true;
    }
    if (literal == "false" || literal == "0") {
        return false;
    }
    throw std::invalid_argument("invalid boolean filter literal: " + literal);
}

template <typename T, FilterKind Kind>
FilterPtr floatingRange(T lower, bool lowerUnbounded, bool lowerExclusive,
                        T upper, bool upperUnbounded, bool upperExclusive, bool negated = false)
{
    return std::make_shared<FloatingPointRange<T, Kind>>(
        lower, lowerUnbounded, lowerExclusive, upper, upperUnbounded, upperExclusive, negated, false);
}

// Align with Velox ExprToSubfieldFilter by dispatching makeXxxFilter to
// bigint, double, boolean, or bytes filters based on the column type.
FilterPtr makeEqualFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return std::make_shared<BigintRange>(v, v, false);
        }
        case omniruntime::type::OMNI_DOUBLE: {
            double v = std::stod(literal);
            return floatingRange<double, FilterKind::kDoubleRange>(v, false, false, v, false, false);
        }
        case omniruntime::type::OMNI_BOOLEAN:
            return std::make_shared<BoolValue>(parseBool(literal), false, false);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<BytesRange>(literal, false, false, literal, false, false, false);
        default:
            return nullptr;
    }
}

FilterPtr makeNotEqualFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return std::make_shared<NegatedBigintRange>(v, v, false);
        }
        case omniruntime::type::OMNI_DOUBLE: {
            double v = std::stod(literal);
            return floatingRange<double, FilterKind::kDoubleRange>(v, false, false, v, false, false, true);
        }
        case omniruntime::type::OMNI_BOOLEAN:
            return std::make_shared<BoolValue>(parseBool(literal), true, false);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<NegatedBytesRange>(literal, false, false, literal, false, false, false);
        default:
            return nullptr;
    }
}

FilterPtr makeGreaterThanFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return v == kMax ? AlwaysFalse::instance()
                             : std::make_shared<BigintRange>(v + 1, kMax, false);
        }
        case omniruntime::type::OMNI_DOUBLE:
            return floatingRange<double, FilterKind::kDoubleRange>(std::stod(literal), false, true, 0, true, false);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<BytesRange>(literal, false, true, std::string(), true, false, false);
        default:
            return nullptr;
    }
}

FilterPtr makeGreaterThanOrEqualFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return std::make_shared<BigintRange>(v, kMax, false);
        }
        case omniruntime::type::OMNI_DOUBLE:
            return floatingRange<double, FilterKind::kDoubleRange>(std::stod(literal), false, false, 0, true, false);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<BytesRange>(literal, false, false, std::string(), true, false, false);
        default:
            return nullptr;
    }
}

FilterPtr makeLessThanFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return v == kMin ? AlwaysFalse::instance()
                             : std::make_shared<BigintRange>(kMin, v - 1, false);
        }
        case omniruntime::type::OMNI_DOUBLE:
            return floatingRange<double, FilterKind::kDoubleRange>(0, true, false, std::stod(literal), false, true);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<BytesRange>(std::string(), true, false, literal, false, true, false);
        default:
            return nullptr;
    }
}

FilterPtr makeLessThanOrEqualFilter(DataTypeId typeId, const std::string &literal)
{
    switch (typeId) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32: {
            int64_t v = std::stoll(literal);
            return std::make_shared<BigintRange>(kMin, v, false);
        }
        case omniruntime::type::OMNI_DOUBLE:
            return floatingRange<double, FilterKind::kDoubleRange>(0, true, false, std::stod(literal), false, false);
        case omniruntime::type::OMNI_VARCHAR:
        case omniruntime::type::OMNI_CHAR:
            return std::make_shared<BytesRange>(std::string(), true, false, literal, false, false, false);
        default:
            return nullptr;
    }
}

FilterPtr isNull() { return ::common::IsNull::instance(); }
FilterPtr isNotNull() { return ::common::IsNotNull::instance(); }

// Column type from rowType[index] (Gluten often writes OMNI_INT sentinel for IS NULL).
FilterPtr leafToColumnFilter(nlohmann::json &node, const omniruntime::type::RowType &rowType, bool negated,
                             int &outCol)
{
    auto op = node["op"].get<PredicateOperatorType>();
    switch (op) {
        case EQUAL_TO:
        case GREATER_THAN:
        case GREATER_THAN_OR_EQUAL:
        case LESS_THAN:
        case LESS_THAN_OR_EQUAL:
        case IS_NOT_NULL:
        case IS_NULL:
            break;
        default:
            return nullptr;
    }

    int32_t index = node["index"].get<int32_t>();
    if (index < 0 || index >= rowType.size()) {
        return nullptr;
    }
    auto colType = rowType.childAt(index)->GetId();
    outCol = index;

    if (op == IS_NULL) {
        return negated ? isNotNull() : isNull();
    }
    if (op == IS_NOT_NULL) {
        return negated ? isNull() : isNotNull();
    }

    const std::string literal = node["value"].get<std::string>();
    switch (op) {
        case EQUAL_TO:
            return negated ? makeNotEqualFilter(colType, literal) : makeEqualFilter(colType, literal);
        case GREATER_THAN:
            return negated ? makeLessThanOrEqualFilter(colType, literal)
                           : makeGreaterThanFilter(colType, literal);
        case GREATER_THAN_OR_EQUAL:
            return negated ? makeLessThanFilter(colType, literal)
                           : makeGreaterThanOrEqualFilter(colType, literal);
        case LESS_THAN:
            return negated ? makeGreaterThanOrEqualFilter(colType, literal)
                           : makeLessThanFilter(colType, literal);
        case LESS_THAN_OR_EQUAL:
            return negated ? makeGreaterThanFilter(colType, literal)
                           : makeLessThanOrEqualFilter(colType, literal);
        default:
            return nullptr;
    }
}

// Same-column AND: delegate to Filter::mergeWith; nullptr → ok=false (residual).
FilterPtr combine(const FilterPtr &existing, const FilterPtr &newFilter, bool &ok)
{
    ok = true;
    if (!existing) {
        return newFilter;
    }
    FilterPtr merged = existing->mergeWith(newFilter.get());
    ok = (merged != nullptr);
    return merged;
}

FilterPtr buildRangeUnion(std::vector<std::pair<int64_t, int64_t>> ranges)
{
    std::sort(ranges.begin(), ranges.end());
    std::vector<std::pair<int64_t, int64_t>> merged;
    for (const auto &r : ranges) {
        if (merged.empty()) {
            merged.push_back(r);
            continue;
        }
        auto &last = merged.back();
        // Merge if overlapping (r.first<=last.second) or adjacent (r.first==last.second+1, overflow-safe).
        if (r.first <= last.second || (last.second != kMax && r.first == last.second + 1)) {
            last.second = std::max(last.second, r.second);
        } else {
            merged.push_back(r);
        }
    }
    if (merged.empty()) {
        return AlwaysFalse::instance();
    }
    if (merged.size() == 1) {
        return std::make_shared<BigintRange>(merged[0].first, merged[0].second, false);
    }
    return std::make_shared<BigintMultiRange>(std::move(merged), false);
}

// OR-union of range filters; returns nullptr if any non-range disjunct.
FilterPtr tryMergeBigintRanges(std::vector<FilterPtr> &disjuncts)
{
    std::vector<std::pair<int64_t, int64_t>> ranges;
    for (const auto &f : disjuncts) {
        if (f->is(FilterKind::kBigintRange)) {
            auto *br = static_cast<BigintRange *>(f.get());
            ranges.emplace_back(br->lower(), br->upper());
        } else if (f->is(FilterKind::kBigintMultiRange)) {
            for (const auto &r : static_cast<BigintMultiRange *>(f.get())->ranges()) {
                ranges.push_back(r);
            }
        } else {
            return nullptr; // Non-range filter (e.g. IsNull) cannot become MultiRange
        }
    }
    return buildRangeUnion(std::move(ranges));
}

FilterPtr makeOrFilter(std::vector<FilterPtr> &disjuncts) { return tryMergeBigintRanges(disjuncts); }

// Residual subtree: null = fully pushed; non-null = unpushed part (pushed AND residual == original).

nlohmann::json wrapNeg(const nlohmann::json &node, bool negated)
{
    if (!negated) {
        return node;
    }
    nlohmann::json n;
    n["op"] = static_cast<int>(NOT);
    n["child"] = node;
    return n;
}

nlohmann::json andJson(nlohmann::json lhs, nlohmann::json rhs)
{
    if (lhs.is_null()) {
        return rhs;
    }
    if (rhs.is_null()) {
        return lhs;
    }
    nlohmann::json n;
    n["op"] = static_cast<int>(AND);
    n["left"] = std::move(lhs);
    n["right"] = std::move(rhs);
    return n;
}

nlohmann::json extractFiltersFromRemainingFilter(nlohmann::json &node, const omniruntime::type::RowType &rowType,
                                                 std::vector<FilterPtr> &filters, bool negated);

nlohmann::json handleDisjunction(nlohmann::json &node, const omniruntime::type::RowType &rowType,
                                 std::vector<FilterPtr> &filters, bool negated)
{
    const int columnCount = rowType.size();
    std::vector<FilterPtr> disjuncts;
    int col = -1;
    for (auto *child : {&node["left"], &node["right"]}) {
        std::vector<FilterPtr> tmp(columnCount);
        nlohmann::json childResidual = extractFiltersFromRemainingFilter(*child, rowType, tmp, negated);
        int found = -1;
        int count = 0;
        for (int i = 0; i < columnCount; ++i) {
            if (tmp[i]) {
                found = i;
                ++count;
            }
        }
        if (!childResidual.is_null() || count != 1) {
            return wrapNeg(node, negated);
        }
        if (col == -1) {
            col = found;
        } else if (col != found) {
            return wrapNeg(node, negated);
        }
        disjuncts.push_back(tmp[found]);
    }

    // Single-column string OR goes to residual in phase 1 (only Bigint* ranges are merged).
    FilterPtr orFilter = makeOrFilter(disjuncts);
    if (orFilter == nullptr) {
        return wrapNeg(node, negated);
    }
    bool ok = false;
    FilterPtr merged = combine(filters[col], orFilter, ok);
    if (!ok) {
        return wrapNeg(node, negated);
    }
    filters[col] = merged;
    return nlohmann::json();
}

nlohmann::json extractFiltersFromRemainingFilter(nlohmann::json &node, const omniruntime::type::RowType &rowType,
                                                 std::vector<FilterPtr> &filters, bool negated)
{
    auto op = node["op"].get<PredicateOperatorType>();

    if (op == NOT) {
        return extractFiltersFromRemainingFilter(node["child"], rowType, filters, !negated);
    }
    if ((op == AND && !negated) || (op == OR && negated)) {
        nlohmann::json l = extractFiltersFromRemainingFilter(node["left"], rowType, filters, negated);
        nlohmann::json r = extractFiltersFromRemainingFilter(node["right"], rowType, filters, negated);
        return andJson(std::move(l), std::move(r));
    }
    if ((op == OR && !negated) || (op == AND && negated)) {
        return handleDisjunction(node, rowType, filters, negated);
    }
    if (op == TRUE) {
        return negated ? wrapNeg(node, true) : nlohmann::json();
    }
    if (op == FALSE) {
        return negated ? nlohmann::json() : node;
    }

    int col = -1;
    FilterPtr leaf = leafToColumnFilter(node, rowType, negated, col);
    if (leaf == nullptr) {
        return wrapNeg(node, negated);
    }
    bool ok = false;
    FilterPtr merged = combine(filters[col], leaf, ok);
    if (!ok) {
        return wrapNeg(node, negated);
    }
    filters[col] = merged;
    return nlohmann::json();
}

} // namespace

std::shared_ptr<ScanSpec> makeScanSpec(const omniruntime::type::RowType &rowType,
                                       const std::shared_ptr<nlohmann::json> &enhancementJson, bool &usable,
                                       bool &needResidual,
                                       std::shared_ptr<::common::PredicateCondition> &residualPredicate)
{
    usable = false;
    needResidual = false;
    residualPredicate = nullptr;
    auto root = std::make_shared<ScanSpec>("root");
    const int n = rowType.size();
    for (int i = 0; i < n; ++i) {
        root->addField(rowType.nameOf(i), i);
    }

    std::vector<FilterPtr> filters(n);
    try {
        auto condStr = (*enhancementJson)["vecPredicateCondition"].get<std::string>();
        auto cond = nlohmann::json::parse(condStr);
        nlohmann::json residual = extractFiltersFromRemainingFilter(cond, rowType, filters, /*negated*/ false);
        const auto &children = root->children();
        for (int i = 0; i < n; ++i) {
            if (filters[i]) {
                children[i]->setFilter(filters[i]);
            }
        }
        needResidual = !residual.is_null();
        if (needResidual) {
            residualPredicate = ::common::BuildResidualPredicateCondition(residual, n);
        }
        usable = true;
    } catch (const std::exception &e) {
        LogError("makeScanSpec fallback to legacy path: %s", e.what());
        usable = false;
        needResidual = false;
        residualPredicate = nullptr;
    }
    return root;
}

} // namespace omniruntime::reader
