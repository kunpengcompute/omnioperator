/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once

#include <tuple>
#include <util/compiler_util.h>
#include <vector>
#include "vectorization/Status.h"
#include "vectorization/SimpleFunction.h"

namespace omniruntime::vectorization {
// Declares a method resolver to be used with has_method.
#define DECLARE_METHOD_RESOLVER(Name, MethodName)              \
  struct Name {                                                \
    template <class __T, typename... __TArgs>                  \
    constexpr auto resolve(__TArgs&&... args) const            \
        -> decltype(std::declval<__T>().MethodName(args...)) { \
      return {};                                               \
    }                                                          \
}

DECLARE_METHOD_RESOLVER(call_method_resolver, call);

DECLARE_METHOD_RESOLVER(callNullable_method_resolver, callNullable);

DECLARE_METHOD_RESOLVER(initialize_method_resolver, initialize);

template <typename T, typename = void>
struct has_initialize : std::false_type {};

template <typename T>
struct has_initialize<T, std::void_t<decltype(&T::initialize)>> : std::true_type {};

// Checks if a class C provides a method which returns TRet and takes TArgs as
// parameters. TResolver is a struct created using DECLARE_METHOD_RESOLVER,
// which contains the name of the method.
template <typename C, class TResolver, typename TRet, typename... TArgs>
struct has_method {
private:
    template <typename T>
    static constexpr auto check(
        T *) -> typename std::is_same<decltype(std::declval<TResolver>().template resolve<T>(std::declval<TArgs>()...)),
        TRet>::type
    {
        return {};
    }

    template <typename>
    static constexpr std::false_type check(...)
    {
        return std::false_type();
    }

    using type = decltype(check<C>(nullptr));

public:
    static constexpr bool value = type::value;
};

// wraps a UDF object to provide the inheritance
// this is basically just boilerplate-avoidance
template <typename Fun, typename TReturn, typename... TArgs>
class FunctionHolder {
    Fun instance_;

public:
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    template <size_t N>
    using type_at = std::tuple_element_t<N, arg_types>;
    using udf_struct_t = Fun;
    using exec_return_type = TReturn;
    using exec_arg_types = std::tuple<TArgs...>;

    static constexpr bool udf_has_initialize = has_method<Fun, initialize_method_resolver, void, const std::vector<
        DataTypeId> &, const config::QueryConfig &, TArgs *...>::value;

    static constexpr bool udf_has_call_return_bool = has_method<Fun, call_method_resolver, bool, exec_return_type, TArgs
        ...>::value;
    static constexpr bool udf_has_call_return_void = has_method<Fun, call_method_resolver, void, exec_return_type, TArgs
        ...>::value;
    static constexpr bool udf_has_call_return_status = has_method<Fun, call_method_resolver, Status, exec_return_type,
        TArgs...>::value;
    static constexpr bool udf_has_call = udf_has_call_return_bool | udf_has_call_return_void |
        udf_has_call_return_status;

    static constexpr bool udf_has_callNullable_return_bool = has_method<Fun, callNullable_method_resolver, bool,
        exec_return_type, TArgs *...>::value;
    static constexpr bool udf_has_callNullable_return_void = has_method<Fun, callNullable_method_resolver, void,
        exec_return_type, TArgs *...>::value;
    static constexpr bool udf_has_callNullable_return_status = has_method<Fun, callNullable_method_resolver, Status,
        exec_return_type, TArgs *...>::value;
    static constexpr bool udf_has_callNullable = udf_has_callNullable_return_bool | udf_has_callNullable_return_void |
        udf_has_callNullable_return_status;

    static constexpr int num_args = std::tuple_size_v<arg_types>;

    ALWAYS_INLINE void initialize(const std::vector<DataTypeId> &inputTypes, const config::QueryConfig &config,
        const TArgs *... constantArgs)
    {
        if constexpr (udf_has_initialize) {
            return instance_.initialize(inputTypes, config, constantArgs...);
        }
    }

    ALWAYS_INLINE Status callImpl(TReturn &out, bool &notNull, const TArgs &... args)
    {
        if constexpr (std::is_same_v<decltype(instance_.call(out, args...)), Status>) {
            notNull = true;
            return instance_.call(out, args...);
        } else if constexpr (std::is_same_v<decltype(instance_.call(out, args...)), bool>) {
            notNull = instance_.call(out, args...);
            return Status::OK();
        } else {
            notNull = true;
            instance_.call(out, args...);
            return Status::OK();
        }
    }

    ALWAYS_INLINE Status callNullableImpl(exec_return_type &out, bool &notNull, const TArgs *... args)
    {
        if constexpr (std::is_same_v<decltype(instance_.callNullable(out, args...)), Status>) {
            notNull = true;
            return instance_.callNullable(out, args...);
        } else if (std::is_same_v<decltype(instance_.callNullable(out, args...)), bool>) {
            notNull = instance_.callNullable(out, args...);
            return Status::OK();
        }
        else {
            notNull = true;
            instance_.callNullable(out, args...);
            return Status::OK();
        }
    }

    ALWAYS_INLINE Status call(TReturn &out, bool &notNull, const TArgs &... args)
    {
        if constexpr (udf_has_call) {
            return callImpl(out, notNull, args...);
        } else if constexpr (udf_has_callNullable) {
            return callNullableImpl(out, notNull, (&args)...);
        } else {
            OMNI_FAIL("call should never be called if the UDF does not implement call or callNullable!");
        }
    }
};
}
