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
#include <utility>
#include <vector>

#include "reader/common/Filter.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/PredicateUtil.h"
#include "util/debug.h"

namespace omniruntime::reader {

using omniruntime::codegen::ScanSpec;
using ::common::FilterPtr;

bool allSelectedColumnsAreInt(const omniruntime::type::RowType &rowType)
{
    for (int i = 0; i < rowType.size(); ++i) {
        switch (rowType.childAt(i)->GetId()) {
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_LONG:
            case omniruntime::type::OMNI_SHORT:
            case omniruntime::type::OMNI_DATE32:
                break;
            default:
                return false;
        }
    }
    return true;
}

namespace {
using namespace ::common;

constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

// Pushable integer family (lossless into the int64 filter path). tinyint (OMNI_BYTE) uses a
// separate byte-RLE decoder and is not included yet.
bool isIntDataType(omniruntime::type::DataTypeId id)
{
    switch (id) {
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_DATE32:
            return true;
        default:
            return false;
    }
}

// ---- Single-op → Filter builders (aligned with Velox ExprToSubfieldFilter makeXxxFilter) ----
// Negation is not handled here: leafToColumnFilter picks the opposite builder when negated
// (e.g. lt <-> gte).
FilterPtr makeEqualFilter(int64_t v) { return std::make_shared<BigintRange>(v, v, false); }
FilterPtr makeNotEqualFilter(int64_t v) { return std::make_shared<NegatedBigintRange>(v, v, false); }
FilterPtr makeGreaterThanFilter(int64_t v)
{
    return v == kMax ? AlwaysFalse::instance() // > MAX is always empty
                     : std::make_shared<BigintRange>(v + 1, kMax, false);
}
FilterPtr makeGreaterThanOrEqualFilter(int64_t v) { return std::make_shared<BigintRange>(v, kMax, false); }
FilterPtr makeLessThanFilter(int64_t v)
{
    return v == kMin ? AlwaysFalse::instance() // < MIN is always empty
                     : std::make_shared<BigintRange>(kMin, v - 1, false);
}
FilterPtr makeLessThanOrEqualFilter(int64_t v) { return std::make_shared<BigintRange>(kMin, v, false); }
FilterPtr isNull() { return ::common::IsNull::instance(); }
FilterPtr isNotNull() { return ::common::IsNotNull::instance(); }

// Single leaf (compare / null check) → (column index, common::Filter). nullptr means "not a
// pushable leaf" (non-compare/null op, OOB, non-int); caller turns it into residual.
// When negated=true, push the opposite builder (lt→gte, =→!=, is null↔is not null), matching
// Velox leafCallToSubfieldFilter. Negated compares still reject null (Spark 3VL: NOT NULL=NULL),
// so all ranges keep nullAllowed=false.
FilterPtr leafToColumnFilter(nlohmann::json &node, int columnCount, bool negated, int &outCol)
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
            return nullptr; // Structural ops (AND/OR/NOT/TRUE/FALSE) or unknown → caller
    }

    int32_t index = node["index"].get<int32_t>();
    auto typeId = node["dataType"].get<omniruntime::type::DataTypeId>();
    if (index < 0 || index >= columnCount || !isIntDataType(typeId)) {
        return nullptr; // OOB / non-int → not pushable
    }
    outCol = index;

    if (op == IS_NULL) {
        return negated ? isNotNull() : isNull();
    }
    if (op == IS_NOT_NULL) {
        return negated ? isNull() : isNotNull();
    }

    // OMNI_DATE32: no extra rebase here. Gluten OrcPushFilterBuilder.getLiteralValue already
    // rebases LocalDate via rebaseGregorianToJulianDays, so pushed literals are Julian. Selective
    // row filtering runs before Next's Julian→Gregorian rebase, and decoded values are also Julian
    // — both sides share Julian semantics. Still need new/legacy date-filter parity tests
    // (including < 1582) to lock this in.
    int64_t v = std::stoll(node["value"].get<std::string>());
    switch (op) {
        case EQUAL_TO:
            return negated ? makeNotEqualFilter(v) : makeEqualFilter(v);
        case GREATER_THAN:
            return negated ? makeLessThanOrEqualFilter(v) : makeGreaterThanFilter(v);
        case GREATER_THAN_OR_EQUAL:
            return negated ? makeLessThanFilter(v) : makeGreaterThanOrEqualFilter(v);
        case LESS_THAN:
            return negated ? makeGreaterThanOrEqualFilter(v) : makeLessThanFilter(v);
        case LESS_THAN_OR_EQUAL:
            return negated ? makeGreaterThanFilter(v) : makeLessThanOrEqualFilter(v);
        default:
            return nullptr; // unreachable
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
        // Merge if overlapping (r.first<=last.second) or adjacent (r.first==last.second+1,
        // overflow-safe).
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

nlohmann::json extractFiltersFromRemainingFilter(nlohmann::json &node, int columnCount,
                                                 std::vector<FilterPtr> &filters, bool negated);

// Disjunction pushdown: each arm must reduce to the same column with no residual, else the whole
// OR becomes residual (aligned with Velox disjunction branch).
nlohmann::json handleDisjunction(nlohmann::json &node, int columnCount, std::vector<FilterPtr> &filters, bool negated)
{
    std::vector<FilterPtr> disjuncts;
    int col = -1;
    for (auto *child : {&node["left"], &node["right"]}) {
        std::vector<FilterPtr> tmp(columnCount);
        nlohmann::json childResidual = extractFiltersFromRemainingFilter(*child, columnCount, tmp, negated);
        int found = -1;
        int count = 0;
        for (int i = 0; i < columnCount; ++i) {
            if (tmp[i]) {
                found = i;
                ++count;
            }
        }
        if (!childResidual.is_null() || count != 1) {
            return wrapNeg(node, negated); // Cannot reduce cleanly to one column → residual
        }
        if (col == -1) {
            col = found;
        } else if (col != found) {
            return wrapNeg(node, negated); // Cross-column OR → residual
        }
        disjuncts.push_back(tmp[found]);
    }

    FilterPtr orFilter = makeOrFilter(disjuncts);
    if (orFilter == nullptr) {
        return wrapNeg(node, negated); // Non-range (e.g. IS NULL) → residual
    }
    bool ok = false;
    FilterPtr merged = combine(filters[col], orFilter, ok);
    if (!ok) {
        return wrapNeg(node, negated);
    }
    filters[col] = merged;
    return nlohmann::json(); // Fully pushed, no residual
}

// Recursively push single-column int predicates; negated means an odd number of NOT layers.
// Returns the unpushed residual subtree (null = none).
nlohmann::json extractFiltersFromRemainingFilter(nlohmann::json &node, int columnCount,
                                                 std::vector<FilterPtr> &filters, bool negated)
{
    auto op = node["op"].get<PredicateOperatorType>();

    if (op == NOT) {
        return extractFiltersFromRemainingFilter(node["child"], columnCount, filters, !negated);
    }
    if ((op == AND && !negated) || (op == OR && negated)) {
        nlohmann::json l = extractFiltersFromRemainingFilter(node["left"], columnCount, filters, negated);
        nlohmann::json r = extractFiltersFromRemainingFilter(node["right"], columnCount, filters, negated);
        return andJson(std::move(l), std::move(r));
    }
    if ((op == OR && !negated) || (op == AND && negated)) {
        return handleDisjunction(node, columnCount, filters, negated);
    }
    if (op == TRUE) {
        return negated ? wrapNeg(node, true) : nlohmann::json();
    }
    if (op == FALSE) {
        return negated ? nlohmann::json() : node;
    }

    int col = -1;
    FilterPtr leaf = leafToColumnFilter(node, columnCount, negated, col);
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
        nlohmann::json residual = extractFiltersFromRemainingFilter(cond, n, filters, /*negated*/ false);
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
