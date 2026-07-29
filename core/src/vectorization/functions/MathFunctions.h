
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Mathematical functions for vectorization framework
 */

#pragma once
#include "util/compiler_util.h"
#include "vectorization/Status.h"
#include <cmath>
#include <iostream>
#include <random>
#include <utility>
#include "type/decimal128.h"
#include "type/data_type.h"
#include "vectorization/functions/Arithmetic.h"
#include "util/config/QueryConfig.h"

namespace omniruntime::vectorization {
    template <typename T>
    struct AbsFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::abs(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct AcoshFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::acosh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct AcosFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::acos(a);
            return Status::OK();
        }
    };

	template <typename T>
    struct AsinFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::asin(a);
            return Status::OK();
        }
    };

	template <typename T>
    struct AsinhFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::asinh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct AtanFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::atan(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct AtanhFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::atanh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct Atan2Function {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a, const TInput &b)
        {
            result = std::atan2(a + 0.0, b + 0.0);
            return Status::OK();
        }
    };

    template <typename T>
    struct CotFunction {
        ALWAYS_INLINE void call(double &result, double a) {
            result = 1 / std::tan(a);
        }
    };

    template <typename T>
    struct CosFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::cos(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct CoshFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::cosh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct SinFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::sin(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct TanFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::tan(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct CbrtFunction {
        ALWAYS_INLINE void call(double &result, double a) {
            result = std::cbrt(a);
        }
    };

	constexpr double kMinDoubleAboveInt64Max = 9223372036854775808.0;

	inline int64_t safeDoubleToInt64(const int64_t& arg) {
		return arg;
	}

	inline int64_t safeDoubleToInt64(const double& arg) {
		if (std::isnan(arg)) {
			return 0;
		}
		static const int64_t kMax = std::numeric_limits<int64_t>::max();
		static const int64_t kMin = std::numeric_limits<int64_t>::min();

		if (arg >= kMinDoubleAboveInt64Max) {
			return kMax;
		}

		if (arg  < kMin) {
		    return kMin;
		}
		return arg;
	}

  	template <typename T>
	struct CeilFunction {
		template <typename TInput>
        ALWAYS_INLINE Status call(T &result, const TInput &a){
			if constexpr (std::is_integral_v<T>) {
				if constexpr (std::is_integral_v<TInput>) {
					result = a;
				} else {
					result = safeDoubleToInt64(std::ceil(a));
				}
			} else {
				result = std::ceil(a);
			}
            return Status::OK();
		}
	};

         /// Floor function
 	     /// floor(x) -> bigint (Spark) or double (Flink)
 	     /// Returns the largest integer less than or equal to x.
 	     /// In Spark, both ceil and floor must return Long type.
 	     /// In Flink, ceil/floor on DOUBLE returns DOUBLE.
 	     /// Supports: long, double
 	     template <typename T>
 	     struct FloorFunction {
 	         template <typename TInput>
 	         ALWAYS_INLINE Status call(T &result, const TInput &a) {

 	             if constexpr (std::is_integral_v<T>) {
 	                 if constexpr (std::is_integral_v<TInput>) {
 	                     // For integral types, floor is identity
 	                     result = a;
 	                 } else {
 	                     // For floating-point types, use std::floor and safe conversion
 	                     result = safeDoubleToInt64(std::floor(a));
 	                 }
 	             } else {
 	                 result = std::floor(a);
 	             }
 	             return Status::OK();
 	    }
 	};


    template <typename T>
        struct NegativeFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = -a;
            return Status::OK();
        }
    };

    template <typename T>
    struct SignFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            if constexpr (std::is_floating_point<TInput>::value) {
               if (std::isnan(a)) {
                   result = std::numeric_limits<TInput>::quiet_NaN();
               } else {
                   result = (a == 0.0) ? 0.0 : (a > 0.0) ? 1.0 : -1.0;
               }
            } else {
               result = (a == 0) ? 0 : (a > 0) ? 1 : -1;
            }
            return Status::OK();
        }
    };

    template <typename T>
    struct SinhFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::sinh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct TanhFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::tanh(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct HypotFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a, const TInput &b)
        {
            result = std::hypot(a, b);
            return Status::OK();
        }
    };

    template <typename T>
    struct SqrtFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::sqrt(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct DegreesFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = a * (180.0 / M_PI);
            return Status::OK();
        }
    };

    template <typename T>
    struct RadiansFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            // radians = degrees * π / 180  (inverse of DegreesFunction)
            result = a * (M_PI / 180.0);
            return Status::OK();
        }
    };

    template <typename T>
    struct ExpFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::exp(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct SecFunction {
        ALWAYS_INLINE void call(double &result, double a) {
            result = 1 / std::cos(a);
        }
    };

    template <typename T>
    struct CscFunction {
        ALWAYS_INLINE void call(double &result, double a) {
            result = 1 / std::sin(a);
        }
    };

    template <typename T>
    struct LnFunction {
        ALWAYS_INLINE bool call(double &result, double a) {
            result = std::log(a);
            return true;
        }
    };
  
    template <typename T>
    struct Log10Function {
        ALWAYS_INLINE bool call(double &result, double a) {
            if (a <= 0.0) {
                return false;
            }
            result = std::log10(a);
            return true;
        }
     };

    template <typename T>
    struct Log1pFunction {
        ALWAYS_INLINE bool call(double &result, double a) {
            if (a <= -1) {
                return false;
            }
            result = std::log1p(a);
            return true;
        }
    };

    template <typename T>
    struct Log2Function {
        ALWAYS_INLINE bool call(double &result, double a) {
            if (a <= 0.0) {
                return false;
            }
            result = std::log2(a);
            return true;
        }
    };

    template <typename T>
    struct LogarithmFunction {
        ALWAYS_INLINE bool call(double &result, double a, double b) {
            if (a <= 0 || b <= 0) {
                return false;
            }
            result = std::log(b) / std::log(a);
            return true;
        }
    };

    template <typename T>
    struct Expm1Function {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            // Use std::expm1 for high precision when x is close to zero
            result = std::expm1(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct PModIntFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput& result, const TInput a, const TInput n) {
            TInput r;
            Status status = RemainderFunction<T>().call(r, a, n);
            if (!status.ok()) {
                return status;
            }
            bool keepR = (r > 0 && n > 0) || (r <= 0 && n < 0);
            result = keepR ? r : (r + n) % n;
            return Status::OK();
        }
    };

    template <typename T>
    struct PModFloatFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput& result, const TInput a, const TInput n) {
            if (n == (TInput)0) {
                return Status::UserError("Division by zero");
            }
            TInput r = std::fmod(a, n);
            bool keepR = (r > 0 && n > 0) || (r <= 0 && n < 0);
            result = keepR ? r : std::fmod(r + n, n);
            return Status::OK();
        }
    };

// Positive function: returns the input value unchanged (identity function)
// Supports: byte, short, int, long, float, double, decimal64, decimal128
    template <typename T>
    struct PositiveFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = a;
            return Status::OK();
        }
    };

    template <>
    struct PositiveFunction<type::Decimal128> {
		template <typename R, typename A>
		ALWAYS_INLINE Status call(R &&result, A &&a)
        {
            result = a;
            return Status::OK();
        }
    };

    template <typename T>
    struct PowerFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &x, const TInput &y)
        {
            result = std::pow(x, y);
            return Status::OK();
        }
    };

    template <typename T>
    struct RintFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            result = std::rint(a);
            return Status::OK();
        }
    };

    template <typename T>
    struct RandFunction {
        ALWAYS_INLINE Status call(double &result)
        {
            thread_local std::mt19937 gen(std::random_device{}());
            result = std::uniform_real_distribution<double>(0.0, 1.0)(gen);
            return Status::OK();
        }
    };

    /// pi() -> DOUBLE. 0-arg constant function returning the value of PI
    /// (matches Flink `PI()` / Java `Math.PI` = 3.141592653589793).
    /// Uses an explicit literal instead of M_PI for cross-compiler portability.
    template <typename T>
    struct PiFunction {
        ALWAYS_INLINE Status call(double &result)
        {
            result = 3.141592653589793;  // Math.PI
            return Status::OK();
        }
    };

    /// e() -> DOUBLE. 0-arg constant function returning Euler's number
    /// (matches Flink `E()` / Java `Math.E` = 2.718281828459045).
    /// Uses an explicit literal instead of M_E for cross-compiler portability.
    template <typename T>
    struct EFunction {
        ALWAYS_INLINE Status call(double &result)
        {
            result = 2.718281828459045;  // Math.E
            return Status::OK();
        }
    };

    template <typename T>
    struct RandSeedFunctionInt32 {
        void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/, const config::QueryConfig & config,
            const int32_t *seed)
        {
            const int32_t partitionId = config.sparkPartitionId();
            int64_t s = seed ? static_cast<int64_t>(*seed) : 0;
            generator_.seed(static_cast<std::mt19937::result_type>(s + partitionId));
        }
        ALWAYS_INLINE Status callNullable(double &result, const int32_t * /*seedInput*/)
        {
            result = std::uniform_real_distribution<double>(0.0, 1.0)(generator_);
            return Status::OK();
        }
    private:
        std::mt19937 generator_;
    };

    template <typename T>
    struct RandSeedFunctionInt64 {
        void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/, const config::QueryConfig & config,
            const int64_t *seed)
        {
            const int32_t partitionId = config.sparkPartitionId();
            int64_t s = seed ? *seed : 0;
            generator_.seed(static_cast<std::mt19937::result_type>(s + partitionId));
        }
        ALWAYS_INLINE Status callNullable(double &result, const int64_t * /*seedInput*/)
        {
            result = std::uniform_real_distribution<double>(0.0, 1.0)(generator_);
            return Status::OK();
        }
    private:
        std::mt19937 generator_;
    };

    template <typename T>
    struct WidthBucketFunction {
        ALWAYS_INLINE bool callNullable(int64_t &result, const double *value, const double *bound1,
            const double *bound2, const int64_t *numBuckets)
        {

            // NULL input returns NULL
            if (value == nullptr || bound1 == nullptr || bound2 == nullptr || numBuckets == nullptr) {
                return false;
            }

            // Check if should return NULL based on input validation
            if (shouldReturnNull(*value, *bound1, *bound2, *numBuckets)) {
                return false;
            }

            result = computeBucketNumber(*value, *bound1, *bound2, *numBuckets);
            return true;
        }

    private:
        static ALWAYS_INLINE bool shouldReturnNull(double value, double bound1, double bound2, int64_t numBuckets)
        {
            return numBuckets <= 0 ||
                numBuckets == std::numeric_limits<int64_t>::max() ||
                std::isnan(value) ||
                bound1 == bound2 ||
                !std::isfinite(bound1) ||
                !std::isfinite(bound2);
        }

        static ALWAYS_INLINE int64_t computeBucketNumber(double value, double bound1, double bound2, int64_t numBuckets)
        {
            if (bound1 < bound2) {
                // Normal ascending range
                if (value < bound1) {
                    return 0;
                }
                if (value >= bound2) {
                    return numBuckets + 1;
                }
            } else {
                // bound1 > bound2: descending range
                if (value > bound1) {
                    return 0;
                }
                if (value <= bound2) {
                    return numBuckets + 1;
                }
            }
            // Calculate bucket number: floor(numBuckets * (value - bound1) / (bound2 - bound1)) + 1
            return static_cast<int64_t>(numBuckets * (value - bound1) / (bound2 - bound1)) + 1;
        }
    };

    // Round function: round(expr) default scale=0; round(expr, scale). byte/short/int/long/float/double only.
    template <typename T>
    struct RoundFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            return call(result, a, 0);
        }

        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a, const int32_t &scale)
        {
            if constexpr (std::is_integral_v<TInput>) {
                result = a;
            } else {
                const double factor = std::pow(10, scale);
                result = static_cast<TInput>(std::round(a * factor) / factor);
                if (result == static_cast<TInput>(0)) {
                    result = static_cast<TInput>(0);
                }
            }
            return Status::OK();
        }
    };

    // Truncate function: truncate(expr) default scale=0; truncate(expr, scale).
    // byte/short/int/long/float/double only. Mirrors Flink TRUNCATE(numeric, integer): truncates
    // (toward zero, RoundingMode.DOWN) to `scale` decimal places. Negative scale zeros digits
    // left of the decimal point. Integral inputs with scale >= 0 are unchanged (no fractional
    // part); negative scale uses integer division toward zero, e.g. truncate(12345, -1) -> 12340.
    // Differs from RoundFunction only in using std::trunc instead of std::round.
    template <typename T>
    struct TruncateFunction {
        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            return call(result, a, 0);
        }

        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a, const int32_t &scale)
        {
            if constexpr (std::is_integral_v<TInput>) {
                if (scale >= 0) {
                    result = a;
                } else {
                    const int64_t divisor = static_cast<int64_t>(std::pow(10, -scale));
                    const int64_t value = static_cast<int64_t>(a);
                    result = static_cast<TInput>((value / divisor) * divisor);
                }
            } else {
                const double factor = std::pow(10, scale);
                result = static_cast<TInput>(std::trunc(a * factor) / factor);
                if (result == static_cast<TInput>(0)) {
                    result = static_cast<TInput>(0);
                }
            }
            return Status::OK();
        }
    };

    /// Factorial function
    /// factorial(n) -> bigint
    /// Returns the factorial of a non-negative integer n.
    /// Factorial is defined as n! = n × (n-1) × ... × 2 × 1, with 0! = 1.
    /// Supports integers from 0 to 20 (21! overflows int64_t).
    /// Returns NULL for negative numbers or numbers greater than 20.
    template <typename T>
    struct FactorialFunction {
        // Pre-computed factorial lookup table for 0! to 20!
        static constexpr int64_t kFactorials[21] = {
            1LL,                    // 0!
            1LL,                    // 1!
            2LL,                    // 2!
            6LL,                    // 3!
            24LL,                   // 4!
            120LL,                  // 5!
            720LL,                  // 6!
            5040LL,                 // 7!
            40320LL,                // 8!
            362880LL,               // 9!
            3628800LL,              // 10!
            39916800LL,             // 11!
            479001600LL,            // 12!
            6227020800LL,           // 13!
            87178291200LL,          // 14!
            1307674368000LL,        // 15!
            20922789888000LL,       // 16!
            355687428096000LL,      // 17!
            6402373705728000LL,     // 18!
            121645100408832000LL,   // 19!
            2432902008176640000LL   // 20!
        };

        /// call method for non-nullable input
        /// @param result Output: the factorial result
        /// @param input The input integer (int32_t)
        /// @return Status::OK() if successful, Status::UserError if input is out of valid range (returns NULL)
        ALWAYS_INLINE Status call(int64_t &result, const int32_t &input)
        {

            // Check for valid range: 0 <= input <= 20
            if (input < 0 || input > 20) {
                // Out of range, return UserError to indicate NULL result
                return Status::UserError("factorial requires 0 <= input <= 20, got {}", input);
            }
            result = kFactorials[input];
            return Status::OK();
        }

        /// callNullable method for nullable input
        /// @param result Output: the factorial result
        /// @param input Pointer to input integer (nullptr means NULL input)
        /// @return Status::OK() if successful, Status::UserError if input is NULL or out of valid range
        ALWAYS_INLINE Status callNullable(int64_t &result, const int32_t *input)
        {
            if (input == nullptr) {
                // NULL input, return UserError to indicate NULL result
                return Status::UserError("factorial received NULL input");
            }
            return call(result, *input);
        }
    };

    template <typename T>
    struct IntegralDivideFunction {
        template <typename TInput>
        ALWAYS_INLINE bool callNullable(int64_t &result, const TInput *a, const TInput *b)
        {

            // NULL input returns NULL
            if (a == nullptr || b == nullptr) {
                return false;
            }

            // Handle Decimal128 type specially
            if constexpr (std::is_same_v<TInput, type::Decimal128>) {
                // Division by zero returns NULL
                if (b->IsZero()) {
                    return false;
                }

                // Convert to int128_t for integer division, then truncate to int64_t
                type::int128_t aVal = a->ToInt128();
                type::int128_t bVal = b->ToInt128();

                // Perform integer division (truncates towards zero)
                type::int128_t quotient = aVal / bVal;
                result = static_cast<int64_t>(quotient);
                return true;
            } else {
                // For integral types (LONG, DECIMAL64)
                // Division by zero returns NULL
                if (*b == 0) {
                    return false;
                }

                if (*a == std::numeric_limits<int64_t>::min() && *b == static_cast<TInput>(-1)) {
                    result = *a;
                    return true;
                }

                result = static_cast<int64_t>(*a) / static_cast<int64_t>(*b);
                return true;
            }
        }
    };

    template <typename T>
    struct NormalizeNaNAndZero {
        const double DOUBLE_NAN = (0.0 / 0.0);
        const uint64_t DOUBLE_BIT_MASK = ((static_cast<uint64_t>(1) << (sizeof(double) * 8 - 1)) - 1);
        const float FLOAT_NAN = (0.0f / 0.0f);
        const uint32_t FLOAT_BIT_MASK = ((static_cast<uint32_t>(1) << (sizeof(float) * 8 - 1)) - 1);
        double computeDouble(double value) {
            if (std::isnan(value)) {
                return DOUBLE_NAN;
            }
            union {
                uint64_t l;
                double d;
            } u;
            u.d = value;
            if (u.l & DOUBLE_BIT_MASK) {
                return value;
            }
            return 0.0;
        }

        float computeFloat(float value) {
            if (std::isnan(value)) {
                return FLOAT_NAN;
            }
            union {
                uint32_t l;
                float d;
            } u;
            u.d = value;
            if (u.l & FLOAT_BIT_MASK) {
                return value;
            }
            return 0.0f;
        }

        template <typename TInput>
        ALWAYS_INLINE Status call(TInput &result, const TInput &a)
        {
            if (std::is_same_v<TInput, double>) {
                result = computeDouble(a);
            } else if (std::is_same_v<TInput, float>) {
                result = computeFloat(a);
            }
            return Status::OK();
        }
    };

}
