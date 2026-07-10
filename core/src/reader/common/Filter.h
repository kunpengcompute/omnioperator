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

// Pushdown filter abstraction (aligned with Velox common::Filter); T0 covers the int64 path.

#ifndef OMNI_READER_COMMON_FILTER_H
#define OMNI_READER_COMMON_FILTER_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace common {

// Aligned with Velox FilterKind; T0 implements Always*/IsNull*/BigintRange/Values/Negated/Multi.
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

    // Value tests; T0 primarily uses testInt64; other signatures are placeholders.
    virtual bool testInt64(int64_t /*value*/) const { return false; }
    virtual bool testInt32(int32_t value) const { return testInt64(value); }
    virtual bool testInt16(int16_t value) const { return testInt64(value); }
    virtual bool testDouble(double /*value*/) const { return false; }
    virtual bool testFloat(float /*value*/) const { return false; }
    virtual bool testBool(bool /*value*/) const { return false; }
    virtual bool testBytes(const char * /*value*/, int32_t /*length*/) const { return false; }
    virtual bool testLength(int32_t /*length*/) const { return true; }

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
    bool testInt64Range(int64_t, int64_t, bool) const override { return false; }
    FilterPtr mergeWith(const Filter *) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class AlwaysTrue final : public Filter {
public:
    AlwaysTrue() : Filter(FilterKind::kAlwaysTrue, true) {}
    static FilterPtr instance();
    bool testInt64(int64_t) const override { return true; }
    bool testInt64Range(int64_t, int64_t, bool) const override { return true; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class IsNotNull final : public Filter {
public:
    IsNotNull() : Filter(FilterKind::kIsNotNull, false) {}
    static FilterPtr instance();
    bool testInt64(int64_t) const override { return true; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

class IsNull final : public Filter {
public:
    IsNull() : Filter(FilterKind::kIsNull, true) {}
    static FilterPtr instance();
    bool testInt64(int64_t) const override { return false; }
    FilterPtr mergeWith(const Filter *other) const override;
    FilterPtr clone(bool) const override { return instance(); }
};

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

} // namespace common

#endif // OMNI_READER_COMMON_FILTER_H
