/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: java udf functions.
 */
#include <jni.h>
#include "jni_util.h"
#include "util/error_code.h"
#include "util/type_util.h"
#include "codegen/context_helper.h"
#include "java_udf_functions.h"

namespace omniruntime {
namespace udf {
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

const std::string GET_ENV_FAILED = "Get env from JVM failed.";
const std::string JVM_OOM = "The JVM is likely OOM.";

struct OutputState {
    int32_t outputValueCapacity;
    int32_t rowIdx;
};

omniruntime::op::ErrorCode InitUdf()
{
    return JniUtil::Init();
}

static jobjectArray CreateInputTypeArray(JNIEnv *env, jclass dataTypeIdCls, const int32_t *inputTypes, int32_t vecCount)
{
    jobjectArray paramTypes = env->NewObjectArray(vecCount, dataTypeIdCls, nullptr);
    for (int32_t i = 0; i < vecCount; i++) {
        auto fieldId = JniUtil::GetFieldId(inputTypes[i]);
        auto paramObj = env->GetStaticObjectField(dataTypeIdCls, fieldId);
        env->SetObjectArrayElement(paramTypes, i, paramObj);
    }
    return paramTypes;
}

void ExecuteHiveUdfSingle(int64_t contextPtr, const char *udfClass, int32_t *inputTypes, int32_t retType,
    int32_t vecCount, int64_t inputValueAddr, int64_t inputNullAddr, int64_t inputLengthAddr, int64_t outputValueAddr,
    int64_t outputNullAddr, int64_t outputLengthAddr)
{
    auto executorCls = JniUtil::GetHiveUdfExecutorCls();
    auto executeSingleMethod = JniUtil::GetExecuteSingleMethod();

    // prepare udf name for jni call
    auto env = JniUtil::GetJNIEnv();
    if (env == nullptr) {
        SetError(contextPtr, GET_ENV_FAILED);
        return;
    }
    jstring jUdfClassName = env->NewStringUTF(udfClass);
    if (jUdfClassName == nullptr) {
        SetError(contextPtr, JVM_OOM);
        return;
    }

    // prepare input types and return type for jni call
    auto dataTypeIdCls = JniUtil::GetDataTypeIdCls();
    jobjectArray jParamTypes = CreateInputTypeArray(env, dataTypeIdCls, inputTypes, vecCount);
    auto jRetType = env->GetStaticObjectField(dataTypeIdCls, JniUtil::GetFieldId(retType));

    env->CallStaticVoidMethod(executorCls, executeSingleMethod, jUdfClassName, jParamTypes, jRetType, inputValueAddr,
        inputNullAddr, inputLengthAddr, outputValueAddr, outputNullAddr, outputLengthAddr);
    if (env->ExceptionCheck()) {
        auto msg = JniUtil::GetExceptionMsg(env);
        SetError(contextPtr, msg);
    }
    env->DeleteLocalRef(jUdfClassName);
}

static void ExecHiveUdfOutputString(int64_t contextPtr, const char *udfClass, int32_t *inputTypes, int32_t retType,
    int32_t vecCount, int32_t rowCount, int64_t *inputValues, int64_t *inputNulls, int64_t *inputLengths,
    int64_t outputValueAddr, int64_t outputNullAddr, int64_t outputLengthAddr)
{
    auto executorCls = JniUtil::GetHiveUdfExecutorCls();
    auto executeBatchMethod = JniUtil::GetExecuteBatchMethod();

    // prepare udf name for jni call
    auto env = JniUtil::GetJNIEnv();
    if (env == nullptr) {
        SetError(contextPtr, GET_ENV_FAILED);
        return;
    }
    jstring jUdfClassName = env->NewStringUTF(udfClass);
    if (jUdfClassName == nullptr) {
        SetError(contextPtr, JVM_OOM);
        return;
    }

    // prepare input types and return type for jni call
    auto dataTypeIdCls = JniUtil::GetDataTypeIdCls();
    jobjectArray jParamTypes = CreateInputTypeArray(env, dataTypeIdCls, inputTypes, vecCount);
    auto jRetType = env->GetStaticObjectField(dataTypeIdCls, JniUtil::GetFieldId(retType));

    auto outputValueAddrArr = reinterpret_cast<int64_t *>(outputValueAddr);
    auto outputLengthArr = reinterpret_cast<int32_t *>(outputLengthAddr);

    auto outputState = new OutputState { 0, 0 };
    auto outputStateAddr = reinterpret_cast<int64_t>(outputState);
    int32_t outputValueCapacity = 1024;
    int32_t start = 0;
    while (outputState->rowIdx < rowCount) {
        // handle remaining data
        auto outputValuePtr = ArenaAllocatorMalloc(contextPtr, outputValueCapacity);
        outputState->outputValueCapacity = outputValueCapacity;
        env->CallStaticVoidMethod(executorCls, executeBatchMethod, jUdfClassName, jParamTypes, jRetType,
            reinterpret_cast<int64_t>(inputValues), reinterpret_cast<int64_t>(inputNulls),
            reinterpret_cast<int64_t>(inputLengths), rowCount, outputValuePtr, outputNullAddr, outputLengthAddr,
            outputStateAddr);
        if (env->ExceptionCheck()) {
            auto msg = JniUtil::GetExceptionMsg(env);
            SetError(contextPtr, msg);
            env->DeleteLocalRef(jUdfClassName);
            delete outputState;
            return;
        }

        // transform output value to multiple addresses
        int32_t offset = 0;
        for (int32_t i = start; i < outputState->rowIdx; i++) {
            outputValueAddrArr[i] = reinterpret_cast<int64_t>(outputValuePtr + offset);
            offset += outputLengthArr[i];
        }
        outputValueCapacity *= 2;
        start = outputState->rowIdx;
    }
    env->DeleteLocalRef(jUdfClassName);
    delete outputState;
}

static void ExecHiveUdfOutputNonString(int64_t contextPtr, const char *udfClass, int32_t *inputTypes, int32_t retType,
    int32_t vecCount, int32_t rowCount, int64_t *inputValues, int64_t *inputNulls, int64_t *inputLengths,
    int64_t outputValueAddr, int64_t outputNullAddr, int64_t outputLengthAddr)
{
    // prepare udf name for jni call
    auto env = JniUtil::GetJNIEnv();
    if (env == nullptr) {
        SetError(contextPtr, GET_ENV_FAILED);
        return;
    }
    jstring jUdfClassName = env->NewStringUTF(udfClass);
    if (jUdfClassName == nullptr) {
        SetError(contextPtr, JVM_OOM);
        return;
    }

    // prepare input types and return type for jni call
    auto dataTypeIdCls = JniUtil::GetDataTypeIdCls();
    jobjectArray jParamTypes = CreateInputTypeArray(env, dataTypeIdCls, inputTypes, vecCount);
    auto jRetType = env->GetStaticObjectField(dataTypeIdCls, JniUtil::GetFieldId(retType));

    env->CallStaticVoidMethod(JniUtil::GetHiveUdfExecutorCls(), JniUtil::GetExecuteBatchMethod(), jUdfClassName,
        jParamTypes, jRetType, reinterpret_cast<int64_t>(inputValues), reinterpret_cast<int64_t>(inputNulls),
        reinterpret_cast<int64_t>(inputLengths), rowCount, outputValueAddr, outputNullAddr, outputLengthAddr, 0);
    if (env->ExceptionCheck()) {
        auto msg = JniUtil::GetExceptionMsg(env);
        SetError(contextPtr, msg);
    }
    env->DeleteLocalRef(jUdfClassName);
}

void ExecuteHiveUdfBatch(int64_t contextPtr, const char *udfClass, int32_t *inputTypes, int32_t retType,
    int32_t vecCount, int32_t rowCount, int64_t *inputValues, int64_t *inputNulls, int64_t *inputLengths,
    int64_t outputValueAddr, int64_t outputNullAddr, int64_t outputLengthAddr)
{
    if (TypeUtil::IsStringType(static_cast<type::DataTypeId>(retType))) {
        ExecHiveUdfOutputString(contextPtr, udfClass, inputTypes, retType, vecCount, rowCount, inputValues, inputNulls,
            inputLengths, outputValueAddr, outputNullAddr, outputLengthAddr);
    } else {
        ExecHiveUdfOutputNonString(contextPtr, udfClass, inputTypes, retType, vecCount, rowCount, inputValues,
            inputNulls, inputLengths, outputValueAddr, outputNullAddr, outputLengthAddr);
    }
}
}
}