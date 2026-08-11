/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Flink-semantics random functions for the vectorization framework
 *              (flink_rand, rand_integer). Provides a JavaUtilRandom helper that
 *              faithfully replicates java.util.Random (48-bit LCG) so that seeded
 *              overloads reproduce Flink results value-by-value.
 */

#pragma once
#include "util/compiler_util.h"
#include "vectorization/Status.h"
#include "util/config/QueryConfig.h"
#include "type/data_type.h"
#include <vector>
#include <random>
#include <cstdint>

namespace omniruntime::vectorization {

/// Faithful re-implementation of java.util.Random (48-bit linear congruential
/// generator). Used so that seeded RAND_INTEGER(seed, bound) reproduces the exact
/// sequence produced by Flink (which delegates to java.util.Random).
///
/// Reference (java.util.Random):
///   setSeed(s): seed = (s ^ 0x5DEECE66D) & ((1<<48)-1)
///   next(bits): seed = (seed*0x5DEECE66D + 0xB) & ((1<<48)-1); return (int)(seed >>> (48-bits))
///   nextDouble(): (((long)next(26) << 27) + next(27)) * 2^-53
///   nextInt(bound): power-of-two fast path + rejection sampling for the rest
class JavaUtilRandom {
public:
    JavaUtilRandom() { SetSeed(0); }
    explicit JavaUtilRandom(int64_t seed) { SetSeed(seed); }

    /// Scramble and store the seed exactly as java.util.Random#setSeed does.
    void SetSeed(int64_t seed)
    {
        seed_ = (static_cast<uint64_t>(seed) ^ kMultiplier) & kMask;
    }

    /// Advance the generator and return the top `bits` bits (0 < bits <= 32).
    ALWAYS_INLINE int32_t Next(int32_t bits)
    {
        seed_ = (seed_ * kMultiplier + kAddend) & kMask;
        return static_cast<int32_t>(seed_ >> (48 - bits));
    }

    /// Returns a double in [0.0, 1.0). Matches java.util.Random#nextDouble.
    ALWAYS_INLINE double NextDouble()
    {
        int64_t hi = static_cast<int64_t>(Next(26));
        int64_t lo = static_cast<int64_t>(Next(27));
        return static_cast<double>((hi << 27) + lo) * kInvTwoPow53;
    }

    /// Returns an int in [0, bound). Caller MUST ensure bound > 0.
    /// Matches java.util.Random#nextInt(int bound) bit-for-bit, including the
    /// 32-bit signed overflow used by the rejection loop (emulated with uint32_t).
    ALWAYS_INLINE int32_t NextInt(int32_t bound)
    {
        // Power of two: single multiply + shift.
        if ((bound & -bound) == bound) {
            return static_cast<int32_t>((static_cast<int64_t>(bound) * static_cast<int64_t>(Next(31))) >> 31);
        }
        int32_t bits;
        int32_t val;
        do {
            bits = Next(31);
            val = bits % bound;
        } while (static_cast<int32_t>(static_cast<uint32_t>(bits) - static_cast<uint32_t>(val) +
            static_cast<uint32_t>(bound - 1)) < 0);
        return val;
    }

private:
    static constexpr uint64_t kMultiplier = 0x5DEECE66DULL;
    static constexpr uint64_t kAddend = 0xBULL;
    static constexpr uint64_t kMask = (1ULL << 48) - 1;
    static constexpr double kInvTwoPow53 = 1.0 / static_cast<double>(1ULL << 53);
    uint64_t seed_ = 0;
};

/// Produce a non-deterministic 64-bit seed from std::random_device. Used by the
/// no-seed RAND()/RAND_INTEGER overloads (Flink `new java.util.Random()`).
ALWAYS_INLINE int64_t MakeNonDeterministicSeed()
{
    std::random_device rd;
    return (static_cast<int64_t>(rd()) << 32) ^ static_cast<int64_t>(rd());
}

/// rand(): DOUBLE in [0.0, 1.0), no seed, non-deterministic. Matches Flink RAND().
///
/// Registered under a dedicated name ("flink_rand") so it coexists with the existing
/// (Velox/Spark-flavored) "rand" registration without altering it. OmniAdaptor's RAND
/// maps to "flink_rand".
template <typename T>
struct FlinkRandFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& /*config*/)
    {
        rng_.SetSeed(MakeNonDeterministicSeed());
    }

    ALWAYS_INLINE Status call(double& result)
    {
        result = rng_.NextDouble();
        return Status::OK();
    }

private:
    JavaUtilRandom rng_;
};

/// flink_rand(seed: int32_t): DOUBLE in [0.0, 1.0) with an integer seed.
/// Matches Flink 1.16 RandCallGen semantics (java.util.Random):
/// - **Literal seed** (constantInputs[0] is a ConstVector): one reusable Random(seed),
///   each row calls nextDouble() and the sequence advances across rows.
/// - **Column seed** (non-literal): per row `(new Random(rowSeed)).nextDouble()` — only
///   the first draw from a fresh RNG for that row's seed value.
template <typename T>
struct FlinkRandSeedFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& /*config*/, const int32_t* seed)
    {
        // Non-null seed pointer means the planner folded a literal constant into constantInputs.
        seedIsConstant_ = (seed != nullptr);
        if (seedIsConstant_) {
            rng_.SetSeed(static_cast<int64_t>(*seed));
        }
    }

    // Returns bool so the framework maps `false` -> NULL (see FunctionHolder::
    // callNullableImpl). seed NULL => result NULL, matching Flink's
    // generateCallIfArgsNotNull short-circuit on nullable operands.
    ALWAYS_INLINE bool callNullable(double& result, const int32_t* seed)
    {
        if (seedIsConstant_) {
            result = rng_.NextDouble();
            return true;
        }
        // Column (or non-constant) seed: Flink generates (new Random(seedTerm)).nextDouble() per row.
        if (seed == nullptr) {
            return false; // seed NULL -> result NULL
        }
        JavaUtilRandom rowRng(static_cast<int64_t>(*seed));
        result = rowRng.NextDouble();
        return true;
    }

private:
    JavaUtilRandom rng_;
    bool seedIsConstant_ = false;
};

/// rand_integer(bound: int32_t) -> int32_t
/// Returns a pseudorandom integer in [0, bound). Non-deterministic (no seed),
/// matching Flink RAND_INTEGER(bound).
///
/// Edge cases:
/// - bound == NULL  -> NULL output (Flink: nullable arg => nullable result)
/// - bound <= 0     -> NULL output (java.util.Random#nextInt throws; the
///                     vectorized path returns NULL instead of aborting the batch)
template <typename T>
struct FlinkRandIntegerFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& /*config*/, const int32_t* /*bound*/)
    {
        rng_.SetSeed(MakeNonDeterministicSeed());
    }

    ALWAYS_INLINE bool callNullable(int32_t& result, const int32_t* bound)
    {
        if (bound == nullptr || *bound <= 0) {
            return false;
        }
        result = rng_.NextInt(*bound);
        return true;
    }

private:
    JavaUtilRandom rng_;
};

/// rand_integer(seed: int32_t, bound: int32_t) -> int32_t
/// Returns a pseudorandom integer in [0, bound) using a seed argument.
/// Matches Flink 1.16 RandCallGen semantics:
/// - **Literal seed**: one reusable Random(seed), nextInt(bound) advances per row.
/// - **Column seed**: per row `(new Random(rowSeed)).nextInt(bound)`.
template <typename T>
struct FlinkRandIntegerSeedFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId>& /*inputTypes*/,
                    const config::QueryConfig& /*config*/, const int32_t* seed, const int32_t* /*bound*/)
    {
        seedIsConstant_ = (seed != nullptr);
        if (seedIsConstant_) {
            rng_.SetSeed(static_cast<int64_t>(*seed));
        }
    }

    ALWAYS_INLINE bool callNullable(int32_t& result, const int32_t* seed, const int32_t* bound)
    {
        if (bound == nullptr || *bound <= 0) {
            return false;
        }
        if (seedIsConstant_) {
            result = rng_.NextInt(*bound);
            return true;
        }
        if (seed == nullptr) {
            return false;
        }
        JavaUtilRandom rowRng(static_cast<int64_t>(*seed));
        result = rowRng.NextInt(*bound);
        return true;
    }

private:
    JavaUtilRandom rng_;
    bool seedIsConstant_ = false;
};

} // namespace omniruntime::vectorization
