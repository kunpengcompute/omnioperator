/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 *
 */
#ifndef OMNI_RUNTIME_OMNI_EXCEPTION_H
#define OMNI_RUNTIME_OMNI_EXCEPTION_H

#include <exception>
#include <iostream>
#include "format.h"
#include "trace_util.h"
#include "debug.h"
#include "util/compiler_util.h"

namespace omniruntime::exception {
class OmniException : public std::exception {
public:
    OmniException(const std::string &errorCode, const std::string &message) : errorCode(errorCode), message(message) {}

    OmniException(const char *errorCode, const char *message) : errorCode(errorCode), message(message) {}

    OmniException(const char *message) : errorCode("RUNTIME_ERROR:"), message(message) {}

    OmniException(const std::string &message) : errorCode("RUNTIME_ERROR:"), message(message) {}

    const char *what() const noexcept override
    {
        FillMessage();
        return elaborateMessage.c_str();
    }

    // Adds execution context at the boundary where the exception escapes a pipeline.
    // The exception is often caught as const before it is propagated through JNI.
    void SetPipelineContext(const std::string &context) const
    {
        pipelineContext = context;
        elaborateMessage.clear();
    }

    bool isUserError() const
    {
        return errorCode == "USER_ERROR";
    }

private:
    void FillMessage() const
    {
        elaborateMessage.clear();
        elaborateMessage += "Error Code: " + errorCode + "\n";
        elaborateMessage += "Reason: " + message + "\n";
        if (!pipelineContext.empty()) {
            elaborateMessage += pipelineContext + "\n";
        }
        elaborateMessage += "Stack: " + TraceUtil::GetStack() + "\n";
    }

    std::string errorCode;
    std::string message;
    mutable std::string pipelineContext;
    mutable std::string elaborateMessage;
};

inline std::exception_ptr toOmniException(const std::exception_ptr& exceptionPtr) {
    try {
        std::rethrow_exception(exceptionPtr);
    } catch (const OmniException&) {
        return exceptionPtr;
    } catch (const std::exception& e) {
        return std::make_exception_ptr(
            OmniException("USER_ERROR", e.what()));
    }
}
}

#define OMNI_THROW(errorCode, ...)                                                                \
    do {                                                                                          \
        throw omniruntime::exception::OmniException(errorCode, omniruntime::Format(__VA_ARGS__)); \
    } while (0)

#define OMNI_CHECK(expr, ...)                                                                              \
    do {                                                                                                   \
        if (__builtin_expect(!(expr), 0)) {                                                                \
            throw omniruntime::exception::OmniException("CHECK_ERROR:", omniruntime::Format(__VA_ARGS__)); \
        }                                                                                                  \
    } while (0)

#define OMNI_FAIL(...)                                                                                   \
    do {                                                                                                 \
        throw omniruntime::exception::OmniException("Runtime error:", omniruntime::Format(__VA_ARGS__)); \
    } while (0)

#define OMNI_USER_FAIL(...)                                                                          \
    do {                                                                                             \
        throw omniruntime::exception::OmniException("USER_ERROR", omniruntime::Format(__VA_ARGS__)); \
    } while (0)

#define OMNI_CHECK_D(expr) OMNI_CHECK(expr, "checkfailed")
#endif // OMNI_RUNTIME_OMNI_EXCEPTION_H
