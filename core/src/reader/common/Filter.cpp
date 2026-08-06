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

// Filter::mergeWith (Velox-aligned). Range kinds covered; Values(IN) may return nullptr → residual.

#include "Filter.h"

#include <limits>

namespace common {

namespace {

constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

FilterPtr nullOrFalse(bool nullAllowed)
{
    if (nullAllowed) {
        return IsNull::instance();
    }
    return AlwaysFalse::instance();
}

using RangePair = std::pair<int64_t, int64_t>;

FilterPtr combineBigintRanges(std::vector<RangePair> ranges, bool nullAllowed)
{
    if (ranges.empty()) {
        return nullOrFalse(nullAllowed);
    }
    if (ranges.size() == 1) {
        return std::make_shared<BigintRange>(ranges[0].first, ranges[0].second, nullAllowed);
    }
    return std::make_shared<BigintMultiRange>(std::move(ranges), nullAllowed);
}

FilterPtr combineNegatedRangeOnIntRanges(int64_t negatedLower, int64_t negatedUpper,
                                         const std::vector<RangePair> &ranges, bool nullAllowed)
{
    std::vector<RangePair> outRanges;
    outRanges.reserve(ranges.size() + 1);
    for (const auto &r : ranges) {
        if (negatedUpper < r.first || r.second < negatedLower) {
            outRanges.push_back(r);
            continue;
        }
        if (r.first < negatedLower) {
            if (negatedLower > kMin) {
                outRanges.emplace_back(r.first, negatedLower - 1);
            }
        }
        if (negatedUpper < r.second) {
            if (negatedUpper < kMax) {
                outRanges.emplace_back(negatedUpper + 1, r.second);
            }
        }
    }
    return combineBigintRanges(std::move(outRanges), nullAllowed);
}

// Intersect two sorted, non-overlapping range lists in O(n+m).
std::vector<RangePair> intersectSortedRanges(const std::vector<RangePair> &left, const std::vector<RangePair> &right)
{
    std::vector<RangePair> out;
    size_t i = 0;
    size_t j = 0;
    while (i < left.size() && j < right.size()) {
        const int64_t lo = std::max(left[i].first, right[j].first);
        const int64_t hi = std::min(left[i].second, right[j].second);
        if (lo <= hi) {
            out.emplace_back(lo, hi);
        }
        if (left[i].second < right[j].second) {
            ++i;
        } else {
            ++j;
        }
    }
    return out;
}

} // namespace

FilterPtr AlwaysFalse::instance()
{
    static FilterPtr inst = std::make_shared<AlwaysFalse>();
    return inst;
}

FilterPtr AlwaysTrue::instance()
{
    static FilterPtr inst = std::make_shared<AlwaysTrue>();
    return inst;
}

FilterPtr IsNotNull::instance()
{
    static FilterPtr inst = std::make_shared<IsNotNull>();
    return inst;
}

FilterPtr IsNull::instance()
{
    static FilterPtr inst = std::make_shared<IsNull>();
    return inst;
}

FilterPtr AlwaysFalse::mergeWith(const Filter *) const
{
    return instance();
}

FilterPtr AlwaysTrue::mergeWith(const Filter *other) const
{
    return other->clone(other->nullAllowed()); // true AND other == other
}

FilterPtr IsNotNull::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kIsNotNull:
            return instance();
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return AlwaysFalse::instance();
        default:
            return other->mergeWith(this);
    }
}

FilterPtr IsNull::mergeWith(const Filter *other) const
{
    if (other->testNull()) {
        return instance();
    }
    return AlwaysFalse::instance();
}

FilterPtr BoolValue::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(false);
        case FilterKind::kBoolValue: {
            const auto *boolean = static_cast<const BoolValue *>(other);
            const bool acceptsFalse = testBool(false) && boolean->testBool(false);
            const bool acceptsTrue = testBool(true) && boolean->testBool(true);
            const bool allowNull = nullAllowed_ && boolean->nullAllowed();
            if (!acceptsFalse && !acceptsTrue) {
                return nullOrFalse(allowNull);
            }
            if (acceptsFalse && acceptsTrue) {
                return allowNull ? AlwaysTrue::instance() : IsNotNull::instance();
            }
            return std::make_shared<BoolValue>(acceptsTrue, false, allowNull);
        }
        default:
            return nullptr;
    }
}

template <typename T, FilterKind Kind>
FilterPtr FloatingPointRange<T, Kind>::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(false);
        case FilterKind::kDoubleRange:
        case FilterKind::kFloatRange: {
            // Only ranges with the same physical type can share the typed
            // intersection logic. A cross-type merge must remain residual.
            if (other->kind() != Kind) {
                return nullptr;
            }
            const auto *range = static_cast<const FloatingPointRange<T, Kind> *>(other);
            // Complements can produce two disjoint intervals. Keep these uncommon combinations
            // as residual predicates instead of silently weakening the filter.
            if (negated_ || range->negated()) {
                return nullptr;
            }

            T lower = lower_;
            bool lowerUnbounded = lowerUnbounded_;
            bool lowerExclusive = lowerExclusive_;
            if (lowerUnbounded || (!range->lowerUnbounded() && range->lower() > lower)) {
                lower = range->lower();
                lowerUnbounded = range->lowerUnbounded();
                lowerExclusive = range->lowerExclusive();
            } else if (!range->lowerUnbounded() && range->lower() == lower) {
                lowerExclusive = lowerExclusive || range->lowerExclusive();
            }

            T upper = upper_;
            bool upperUnbounded = upperUnbounded_;
            bool upperExclusive = upperExclusive_;
            if (upperUnbounded || (!range->upperUnbounded() && range->upper() < upper)) {
                upper = range->upper();
                upperUnbounded = range->upperUnbounded();
                upperExclusive = range->upperExclusive();
            } else if (!range->upperUnbounded() && range->upper() == upper) {
                upperExclusive = upperExclusive || range->upperExclusive();
            }

            if (!lowerUnbounded && !upperUnbounded &&
                (lower > upper || (lower == upper && (lowerExclusive || upperExclusive)))) {
                return nullOrFalse(nullAllowed_ && range->nullAllowed());
            }
            return std::make_shared<FloatingPointRange<T, Kind>>(
                lower, lowerUnbounded, lowerExclusive, upper, upperUnbounded, upperExclusive, false,
                nullAllowed_ && range->nullAllowed());
        }
        default:
            return nullptr;
    }
}

template class FloatingPointRange<double, FilterKind::kDoubleRange>;

FilterPtr BigintRange::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return std::make_shared<BigintRange>(lower_, upper_, /*nullAllowed*/ false);
        case FilterKind::kBigintRange: {
            const auto *rb = static_cast<const BigintRange *>(other);
            int64_t lo = std::max(lower_, rb->lower());
            int64_t hi = std::min(upper_, rb->upper());
            bool na = nullAllowed_ && rb->nullAllowed();
            if (lo > hi) {
                return nullOrFalse(na);
            }
            return std::make_shared<BigintRange>(lo, hi, na);
        }
        case FilterKind::kNegatedBigintRange:
            return other->mergeWith(this);
        case FilterKind::kBigintMultiRange: {
            const auto *multi = static_cast<const BigintMultiRange *>(other);
            std::vector<RangePair> newRanges;
            for (const auto &r : multi->ranges()) {
                int64_t lo = std::max(lower_, r.first);
                int64_t hi = std::min(upper_, r.second);
                if (lo <= hi) {
                    newRanges.emplace_back(lo, hi);
                }
            }
            bool bothNullAllowed = nullAllowed_ && other->testNull();
            return combineBigintRanges(std::move(newRanges), bothNullAllowed);
        }
        default:
            return nullptr;
    }
}

FilterPtr NegatedBigintRange::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return std::make_shared<NegatedBigintRange>(lower_, upper_, /*nullAllowed*/ false);
        case FilterKind::kBigintRange: {
            bool bothNullAllowed = nullAllowed_ && other->testNull();
            const auto *rb = static_cast<const BigintRange *>(other);
            std::vector<RangePair> rangeList{{rb->lower(), rb->upper()}};
            return combineNegatedRangeOnIntRanges(lower_, upper_, rangeList, bothNullAllowed);
        }
        case FilterKind::kNegatedBigintRange: {
            bool bothNullAllowed = nullAllowed_ && other->testNull();
            const auto *on = static_cast<const NegatedBigintRange *>(other);
            if (lower_ > on->lower()) {
                return other->mergeWith(this);
            }
            // Disjoint → three positive ranges; adjacent/overlapping → merge negated span
            // (guard upper_+1 overflow).
            const bool disjointOrGap = (upper_ < kMax) && (upper_ + 1 < on->lower());
            if (disjointOrGap) {
                std::vector<RangePair> outRanges;
                if (lower_ > kMin) {
                    outRanges.emplace_back(kMin, lower_ - 1);
                }
                if (upper_ < kMax && on->lower() > kMin) {
                    outRanges.emplace_back(upper_ + 1, on->lower() - 1);
                }
                if (on->upper() < kMax) {
                    outRanges.emplace_back(on->upper() + 1, kMax);
                }
                return combineBigintRanges(std::move(outRanges), bothNullAllowed);
            }
            return std::make_shared<NegatedBigintRange>(lower_, std::max(upper_, on->upper()),
                                                        bothNullAllowed);
        }
        case FilterKind::kBigintMultiRange: {
            bool bothNullAllowed = nullAllowed_ && other->testNull();
            const auto *multi = static_cast<const BigintMultiRange *>(other);
            return combineNegatedRangeOnIntRanges(lower_, upper_, multi->ranges(), bothNullAllowed);
        }
        default:
            return nullptr;
    }
}

BigintMultiRange::BigintMultiRange(std::vector<std::pair<int64_t, int64_t>> ranges, bool nullAllowed)
    : Filter(FilterKind::kBigintMultiRange, nullAllowed), ranges_(std::move(ranges))
{
    lowerBounds_.reserve(ranges_.size());
    for (const auto &r : ranges_) {
        lowerBounds_.push_back(r.first);
    }
}

bool BigintMultiRange::testInt64(int64_t v) const
{
    auto it = std::upper_bound(lowerBounds_.begin(), lowerBounds_.end(), v);
    if (it == lowerBounds_.begin()) {
        return false;
    }
    size_t idx = static_cast<size_t>(it - lowerBounds_.begin()) - 1;
    return v <= ranges_[idx].second;
}

bool BigintMultiRange::testInt64Range(int64_t mn, int64_t mx, bool hasNull) const
{
    if (hasNull && nullAllowed_) {
        return true;
    }
    for (const auto &r : ranges_) {
        if (!(mx < r.first || mn > r.second)) {
            return true;
        }
    }
    return false;
}

FilterPtr BigintMultiRange::clone(bool nullAllowed) const
{
    return std::make_shared<BigintMultiRange>(ranges_, nullAllowed);
}

FilterPtr BigintMultiRange::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return std::make_shared<BigintMultiRange>(ranges_, /*nullAllowed*/ false);
        case FilterKind::kBigintRange:
        case FilterKind::kNegatedBigintRange:
            return other->mergeWith(this);
        case FilterKind::kBigintMultiRange: {
            const auto *multi = static_cast<const BigintMultiRange *>(other);
            auto newRanges = intersectSortedRanges(ranges_, multi->ranges());
            bool bothNullAllowed = nullAllowed_ && other->testNull();
            return combineBigintRanges(std::move(newRanges), bothNullAllowed);
        }
        default:
            return nullptr;
    }
}

FilterPtr BigintValues::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return std::make_shared<BigintValues>(values_, /*nullAllowed*/ false);
        default:
            return nullptr; // IN not merged yet; leave to residual
    }
}

namespace {

int CompareBytes(std::string_view a, std::string_view b)
{
    return a.compare(b);
}

} // namespace

bool BytesRange::testBytes(const char *value, int32_t length) const
{
    std::string_view view(value, static_cast<size_t>(length));
    if (singleValue_) {
        return view == std::string_view(lower_);
    }
    if (!lowerUnbounded_) {
        int c = CompareBytes(view, lower_);
        if (lowerExclusive_ ? c <= 0 : c < 0) {
            return false;
        }
    }
    if (!upperUnbounded_) {
        int c = CompareBytes(view, upper_);
        if (upperExclusive_ ? c >= 0 : c > 0) {
            return false;
        }
    }
    return true;
}

FilterPtr BytesRange::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(/*nullAllowed*/ false);
        case FilterKind::kBytesRange: {
            const auto *o = static_cast<const BytesRange *>(other);
            if (singleValue_ && o->singleValue_) {
                if (lower_ == o->lower_) {
                    return clone(nullAllowed_ && o->nullAllowed_);
                }
                return nullOrFalse(nullAllowed_ && o->nullAllowed_);
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

FilterPtr NegatedBytesRange::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(/*nullAllowed*/ false);
        default:
            return nullptr;
    }
}

FilterPtr BytesValues::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(/*nullAllowed*/ false);
        default:
            return nullptr;
    }
}

FilterPtr NegatedBytesValues::mergeWith(const Filter *other) const
{
    switch (other->kind()) {
        case FilterKind::kAlwaysTrue:
        case FilterKind::kAlwaysFalse:
        case FilterKind::kIsNull:
            return other->mergeWith(this);
        case FilterKind::kIsNotNull:
            return clone(/*nullAllowed*/ false);
        default:
            return nullptr;
    }
}

} // namespace common
