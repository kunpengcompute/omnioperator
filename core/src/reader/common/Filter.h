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

// Pushdown filter abstraction aligned with Velox common::Filter. T0 covers the
// integer family, A1a covers bytes, and A1b adds BOOLEAN and DOUBLE filters.

#ifndef OMNI_READER_COMMON_FILTER_H
#define OMNI_READER_COMMON_FILTER_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace common {

// Filter kinds aligned with Velox. Not every declared kind is pushable yet.
enum class FilterKind : int8_t {
    kAlwaysFalse,
    kAlwaysTrue,
    kIsNull,
    kIsNotNull,
    kBoolValue,
    kBigintRange,                        // Closed [lower, upper]; covers =, >, >=, <, <=, BETWEEN
    kBigintValuesUsingHashTable,         // IN (v1, v2, ...) via hash table
    kBigintValuesUsingBitmask,
    kNegatedBigintRange,
    kNegatedBigintValuesUsingHashTable,
    kNegatedBigintValuesUsingBitmask,
    kDoubleRange,
    kFloatRange,
    kBytesRange,
    kNegatedBytesRange,
    kBytesValues,
    kNegatedBytesValues,
    kBigintMultiRange,
    kMultiRange,
    kHugeintRange,
    kTimestampRange,
    kHugeintValuesUsingHashTable,
    kBigintValuesUsingBloomFilter,
};

class Filter;
using FilterPtr = std::shared_ptr<Filter>;

class Filter {
public:
    Filter(FilterKind kind, bool nullAllowed) : kind_(kind), nullAllowed_(nullAllowed) {}
    virtual ~Filter() = default;

    FilterKind kind() const { return kind_; }
    bool is(FilterKind kind) const { return kind_ == kind; }
    bool nullAllowed() const { return nullAllowed_; }

    // Caller must ensure kind matches TFilter.
    template <typename TFilter> TFilter *as() { return static_cast<TFilter *>(this); }
    template <typename TFilter> const TFilter *as() const { return static_cast<const TFilter *>(this); }

    bool testNull() const { return nullAllowed_; }

    // Value tests used by the currently enabled filter channels.
    virtual bool testNonNull() const { return false; }
    virtual bool testInt64(int64_t /*value*/) const { return false; }
    virtual bool testInt32(int32_t value) const { return testInt64(value); }
    virtual bool testInt16(int16_t value) const { return testInt64(value); }
    virtual bool testDouble(double /*value*/) const { return false; }
    virtual bool testFloat(float /*value*/) const { return false; }
    virtual bool testBool(bool /*value*/) const { return false; }
    virtual bool testBytes(const char * /*value*/, int32_t /*length*/) const { return false; }
    virtual bool testLength(int32_t /*length*/) const { return true; }
    bool testBytes(std::string_view v) const { return testBytes(v.data(), static_cast<int32_t>(v.size())); }

    // Range tests (stats pruning); default keeps the range.
    virtual bool testInt64Range(int64_t /*min*/, int64_t /*max*/, bool /*hasNull*/) const { return true; }
    virtual bool testDoubleRange(double /*min*/, double /*max*/, bool /*hasNull*/) const { return true; }
    virtual bool testBytesRange(const char * /*min*/, int32_t /*minLen*/,
                                const char * /*max*/, int32_t /*maxLen*/, bool /*hasNull*/) const { return true; }

    // AND-merge: contradiction → AlwaysFalse/IsNull; unsupported combo → nullptr (residual).
    virtual FilterPtr mergeWith(const Filter * /*other*/) const { return nullptr; }

    virtual FilterPtr clone(bool nullAllowed) const = 0;

protected:
    FilterKind kind_;
    bool nullAllowed_;
};

// Stateless filters share a process-wide instance to avoid allocator churn on merge.
class AlwaysFalse final : public Filter {
public:
    AlwaysFalse() : Filter(FilterKind::kAlwaysFalse, false) {}
    static FilterPtr instance();
    bool testInt64(int64_t) const override { return false; }
    bool testBytes(const char *, int32_t) const override { return false; }
    bool testInt64Range(int64_t, int64_t, bool) const override { return false; }
    FilterPtr mergeWith(const Filter *) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class AlwaysTrue final : public Filter {
public:
    AlwaysTrue() : Filter(FilterKind::kAlwaysTrue, true) {}
    static FilterPtr instance();
    bool testNonNull() const override { return true; }
    bool testInt64(int64_t) const override { return true; }
    bool testDouble(double) const override { return true; }
    bool testFloat(float) const override { return true; }
    bool testBool(bool) const override { return true; }
    bool testBytes(const char *, int32_t) const override { return true; }
    bool testInt64Range(int64_t, int64_t, bool) const override { return true; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class IsNotNull final : public Filter {
public:
    IsNotNull() : Filter(FilterKind::kIsNotNull, false) {}
    static FilterPtr instance();
    bool testNonNull() const override { return true; }
    bool testInt64(int64_t) const override { return true; }
    bool testDouble(double) const override { return true; }
    bool testFloat(float) const override { return true; }
    bool testBool(bool) const override { return true; }
    bool testBytes(const char *, int32_t) const override { return true; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class IsNull final : public Filter {
public:
    IsNull() : Filter(FilterKind::kIsNull, true) {}
    static FilterPtr instance();
    bool testInt64(int64_t) const override { return false; }
    bool testBytes(const char *, int32_t) const override { return false; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

// This phase pushes Boolean equality and inequality. Ordering predicates remain on the legacy path.
class BoolValue final : public Filter {
public:
    BoolValue(bool value, bool negated, bool nullAllowed)
        : Filter(FilterKind::kBoolValue, nullAllowed), value_(value), negated_(negated) {}

    bool testBool(bool value) const override { return negated_ ? value != value_ : value == value_; }
    bool value() const { return value_; }
    bool negated() const { return negated_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<BoolValue>(value_, negated_, nullAllowed);
    }

private:
    bool value_;
    bool negated_;
};

// DOUBLE interval. Bounds are kept explicitly inclusive/exclusive because there
// is no safe "literal +/- 1" transformation for floating-point values.
template <typename T, FilterKind Kind>
class FloatingPointRange final : public Filter {
public:
    FloatingPointRange(T lower, bool lowerUnbounded, bool lowerExclusive,
                       T upper, bool upperUnbounded, bool upperExclusive,
                       bool negated, bool nullAllowed)
        : Filter(Kind, nullAllowed),
          lower_(lower),
          upper_(upper),
          lowerUnbounded_(lowerUnbounded),
          upperUnbounded_(upperUnbounded),
          lowerExclusive_(lowerExclusive),
          upperExclusive_(upperExclusive),
          negated_(negated)
    {}

    bool testDouble(double value) const override { return test(static_cast<T>(value)); }
    bool testFloat(float value) const override { return test(static_cast<T>(value)); }

    bool test(T value) const
    {
        bool inside = true;
        if (!lowerUnbounded_) {
            inside = lowerExclusive_ ? value > lower_ : value >= lower_;
        }
        if (inside && !upperUnbounded_) {
            inside = upperExclusive_ ? value < upper_ : value <= upper_;
        }
        return negated_ ? !inside : inside;
    }

    T lower() const { return lower_; }
    T upper() const { return upper_; }
    bool lowerUnbounded() const { return lowerUnbounded_; }
    bool upperUnbounded() const { return upperUnbounded_; }
    bool lowerExclusive() const { return lowerExclusive_; }
    bool upperExclusive() const { return upperExclusive_; }
    bool negated() const { return negated_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<FloatingPointRange<T, Kind>>(
            lower_, lowerUnbounded_, lowerExclusive_, upper_, upperUnbounded_, upperExclusive_, negated_,
            nullAllowed);
    }

private:
    T lower_;
    T upper_;
    bool lowerUnbounded_;
    bool upperUnbounded_;
    bool lowerExclusive_;
    bool upperExclusive_;
    bool negated_;
};

using DoubleRange = FloatingPointRange<double, FilterKind::kDoubleRange>;

// Closed interval [lower, upper] (equality/compare/BETWEEN normalized at construction).
class BigintRange final : public Filter {
public:
    BigintRange(int64_t lower, int64_t upper, bool nullAllowed)
        : Filter(FilterKind::kBigintRange, nullAllowed), lower_(lower), upper_(upper) {}

    bool testInt64(int64_t v) const override { return v >= lower_ && v <= upper_; }

    bool testInt64Range(int64_t mn, int64_t mx, bool hasNull) const override {
        if (hasNull && nullAllowed_) {
            return true;
        }
        return !(mx < lower_ || mn > upper_);
    }

    int64_t lower() const { return lower_; }
    int64_t upper() const { return upper_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<BigintRange>(lower_, upper_, nullAllowed);
    }

private:
    int64_t lower_;
    int64_t upper_;
};

// Value ∉ [lower, upper] (e.g. != / NOT BETWEEN).
class NegatedBigintRange final : public Filter {
public:
    NegatedBigintRange(int64_t lower, int64_t upper, bool nullAllowed)
        : Filter(FilterKind::kNegatedBigintRange, nullAllowed), lower_(lower), upper_(upper) {}

    bool testInt64(int64_t v) const override { return !(v >= lower_ && v <= upper_); }

    bool testInt64Range(int64_t mn, int64_t mx, bool hasNull) const override {
        if (hasNull && nullAllowed_) {
            return true;
        }
        return !(lower_ <= mn && mx <= upper_);
    }

    int64_t lower() const { return lower_; }
    int64_t upper() const { return upper_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<NegatedBigintRange>(lower_, upper_, nullAllowed);
    }

private:
    int64_t lower_;
    int64_t upper_;
};

// Union of ranges (same-column OR); ranges_ must be sorted by lower and non-overlapping.
class BigintMultiRange final : public Filter {
public:
    BigintMultiRange(std::vector<std::pair<int64_t, int64_t>> ranges, bool nullAllowed);

    bool testInt64(int64_t v) const override;
    bool testInt64Range(int64_t mn, int64_t mx, bool hasNull) const override;

    const std::vector<std::pair<int64_t, int64_t>> &ranges() const { return ranges_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override;

private:
    std::vector<std::pair<int64_t, int64_t>> ranges_;
    std::vector<int64_t> lowerBounds_;
};

class BigintValues final : public Filter {
public:
    BigintValues(std::unordered_set<int64_t> values, bool nullAllowed)
        : Filter(FilterKind::kBigintValuesUsingHashTable, nullAllowed), values_(std::move(values)) {}

    bool testInt64(int64_t v) const override { return values_.count(v) != 0; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<BigintValues>(values_, nullAllowed);
    }

private:
    std::unordered_set<int64_t> values_;
};

// Bytes/string range (Velox BytesRange). Equality when lower==upper and both ends closed.
class BytesRange final : public Filter {
public:
    BytesRange(std::string lower, bool lowerUnbounded, bool lowerExclusive, std::string upper, bool upperUnbounded,
               bool upperExclusive, bool nullAllowed)
        : Filter(FilterKind::kBytesRange, nullAllowed),
          lower_(std::move(lower)),
          upper_(std::move(upper)),
          lowerUnbounded_(lowerUnbounded),
          upperUnbounded_(upperUnbounded),
          lowerExclusive_(lowerExclusive),
          upperExclusive_(upperExclusive),
          singleValue_(!lowerUnbounded && !upperUnbounded && !lowerExclusive && !upperExclusive && lower_ == upper_)
    {}

    bool testBytes(const char *value, int32_t length) const override;
    bool testLength(int32_t length) const override
    {
        return !singleValue_ || static_cast<size_t>(length) == lower_.size();
    }

    bool isSingleValue() const { return singleValue_; }
    const std::string &lower() const { return lower_; }
    const std::string &upper() const { return upper_; }
    bool lowerUnbounded() const { return lowerUnbounded_; }
    bool upperUnbounded() const { return upperUnbounded_; }
    bool lowerExclusive() const { return lowerExclusive_; }
    bool upperExclusive() const { return upperExclusive_; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<BytesRange>(lower_, lowerUnbounded_, lowerExclusive_, upper_, upperUnbounded_,
                                            upperExclusive_, nullAllowed);
    }

private:
    std::string lower_;
    std::string upper_;
    bool lowerUnbounded_;
    bool upperUnbounded_;
    bool lowerExclusive_;
    bool upperExclusive_;
    bool singleValue_;
};

class NegatedBytesRange final : public Filter {
public:
    NegatedBytesRange(std::string lower, bool lowerUnbounded, bool lowerExclusive, std::string upper,
                      bool upperUnbounded, bool upperExclusive, bool nullAllowed)
        : Filter(FilterKind::kNegatedBytesRange, nullAllowed),
          nonNegated_(std::make_shared<BytesRange>(std::move(lower), lowerUnbounded, lowerExclusive, std::move(upper),
                                                   upperUnbounded, upperExclusive, nullAllowed))
    {}

    explicit NegatedBytesRange(std::shared_ptr<BytesRange> nonNegated, bool nullAllowed)
        : Filter(FilterKind::kNegatedBytesRange, nullAllowed), nonNegated_(std::move(nonNegated))
    {}

    bool testBytes(const char *value, int32_t length) const override
    {
        return !nonNegated_->testBytes(value, length);
    }
    bool testLength(int32_t length) const override { return true; }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<NegatedBytesRange>(
            std::static_pointer_cast<BytesRange>(nonNegated_->clone(nullAllowed)), nullAllowed);
    }

private:
    std::shared_ptr<BytesRange> nonNegated_;
};

class BytesValues final : public Filter {
public:
    BytesValues(std::unordered_set<std::string> values, bool nullAllowed)
        : Filter(FilterKind::kBytesValues, nullAllowed), values_(std::move(values))
    {}

    bool testBytes(const char *value, int32_t length) const override
    {
        return values_.count(std::string(value, static_cast<size_t>(length))) != 0;
    }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<BytesValues>(values_, nullAllowed);
    }

private:
    std::unordered_set<std::string> values_;
};

class NegatedBytesValues final : public Filter {
public:
    NegatedBytesValues(std::unordered_set<std::string> values, bool nullAllowed)
        : Filter(FilterKind::kNegatedBytesValues, nullAllowed), values_(std::move(values))
    {}

    bool testBytes(const char *value, int32_t length) const override
    {
        return values_.count(std::string(value, static_cast<size_t>(length))) == 0;
    }

    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool nullAllowed) const override
    {
        return std::make_shared<NegatedBytesValues>(values_, nullAllowed);
    }

private:
    std::unordered_set<std::string> values_;
};

} // namespace common

#endif // OMNI_READER_COMMON_FILTER_H
