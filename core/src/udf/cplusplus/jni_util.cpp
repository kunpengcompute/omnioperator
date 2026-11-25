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
 * Description: java udf util.
 */
#include <mutex>
#include "util/debug.h"
#include "jni_util.h"

static const char *HIVE_UDF_EXECUTOR = "omniruntime/udf/HiveUdfExecutor";
static const char *EXEC_SINGLE_SIGNATURE = "(Ljava/lang/String;[Lnova/hetu/omniruntime/type/DataType$DataTypeId;Lnova/"
    "hetu/omniruntime/type/DataType$DataTypeId;JJJJJJ)V";
static const char *EXEC_BATCH_SIGNATURE = "(Ljava/lang/String;[Lnova/hetu/omniruntime/type/DataType$DataTypeId;Lnova/"
    "hetu/omniruntime/type/DataType$DataTypeId;JJJIJJJJ)V";
static jclass hiveUdfExecutorCls;
static jmethodID executeSingleMethod;
static jmethodID executeBatchMethod;

static const char *UDF_UTIL = "omniruntime/udf/UdfUtil";
static const char *THROWABLE_TO_STRING_SIGNATURE = "(Ljava/lang/Throwable;)Ljava/lang/String;";
static jclass udfUtilCls;
static jmethodID throwableToStringMethod;

static const char *DATA_TYPE_ID = "Lnova/hetu/omniruntime/type/DataType$DataTypeId;";
static jclass dataTypeIdCls;
static jfieldID omniNoneField;
static jfieldID omniIntField;
static jfieldID omniLongField;
static jfieldID omniDoubleField;
static jfieldID omniBooleanField;
static jfieldID omniShortField;
static jfieldID omniDecimal64Field;
static jfieldID omniDecimal128Field;
static jfieldID omniData32Field;
static jfieldID omniData64Field;
static jfieldID omniTime32Field;
static jfieldID omniTime64Field;
static jfieldID omniTimestampField;
static jfieldID omniIntervalMonthsField;
static jfieldID omniIntervalDayTimeField;
static jfieldID omniVarcharField;
static jfieldID omniCharField;
static jfieldID omniContainerField;
static jfieldID omniInvalidField;

static std::once_flag initJVMFlag;
static JavaVM *javaVm;
__thread JNIEnv *JniUtil::threadLocalEnv = nullptr;

#define RETURN_ERROR_MSG_IF_EXC(env, msg) \
    do {                                  \
        if ((env)->ExceptionCheck()) {    \
            (env)->ExceptionClear();      \
            return (msg);                 \
        }                                 \
    } while (false)

using namespace omniruntime::op;

ErrorCode JniUtil::Init()
{
    auto env = GetJNIEnv();
    if (env == nullptr) {
        return ErrorCode::JVM_FAILED;
    }

    ErrorCode ret;
    if ((ret = GetGlobalClassRef(env, UDF_UTIL, &udfUtilCls)) != ErrorCode::SUCCESS) {
        return ret;
    }
    throwableToStringMethod = env->GetStaticMethodID(udfUtilCls, "throwableToString", THROWABLE_TO_STRING_SIGNATURE);
    RETURN_ERROR_CODE_IF_EXC(env);

    if ((ret = GetGlobalClassRef(env, HIVE_UDF_EXECUTOR, &hiveUdfExecutorCls)) != ErrorCode::SUCCESS) {
        return ret;
    }
    executeSingleMethod = env->GetStaticMethodID(hiveUdfExecutorCls, "executeSingle", EXEC_SINGLE_SIGNATURE);
    RETURN_ERROR_CODE_IF_EXC(env);
    executeBatchMethod = env->GetStaticMethodID(hiveUdfExecutorCls, "executeBatch", EXEC_BATCH_SIGNATURE);
    RETURN_ERROR_CODE_IF_EXC(env);

    return InitDataTypeIdAndFields(env);
}

ErrorCode JniUtil::InitDataTypeIdAndFields(JNIEnv *env)
{
    ErrorCode ret;
    if ((ret = GetGlobalClassRef(env, DATA_TYPE_ID, &dataTypeIdCls)) != ErrorCode::SUCCESS) {
        return ret;
    }

    omniNoneField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_NONE", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniIntField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_INT", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniLongField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_LONG", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniDoubleField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_DOUBLE", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniBooleanField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_BOOLEAN", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniShortField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_SHORT", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniDecimal64Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_DECIMAL64", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniDecimal128Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_DECIMAL128", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniData32Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_DATE32", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniData64Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_DATE64", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniTime32Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_TIME32", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniTime64Field = env->GetStaticFieldID(dataTypeIdCls, "OMNI_TIME64", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniTimestampField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_TIMESTAMP", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniIntervalMonthsField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_INTERVAL_MONTHS", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniIntervalDayTimeField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_INTERVAL_DAY_TIME", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniVarcharField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_VARCHAR", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniCharField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_CHAR", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniContainerField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_CONTAINER", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);
    omniInvalidField = env->GetStaticFieldID(dataTypeIdCls, "OMNI_INVALID", DATA_TYPE_ID);
    RETURN_ERROR_CODE_IF_EXC(env);

    return ErrorCode::SUCCESS;
}

static void CreateJavaVM()
{
    // found created JVM
    jsize vmNum = 0;
    auto res = JNI_GetCreatedJavaVMs(&javaVm, 1, &vmNum);
    if (res == JNI_OK && vmNum > 0) {
        return;
    }

    // create new JVM if not found
    JavaVMOption options[1];
    auto classPath = getenv("OMNI_OPERATOR_CLASSPATH");
    if (classPath == nullptr) {
        LogError("Create JVM failed since OMNI_OPERATOR_CLASSPATH is not set");
        return;
    }
    options[0].optionString = classPath;
    JavaVMInitArgs vmArgs;
    vmArgs.version = JNI_VERSION_1_8;
    vmArgs.nOptions = 1;
    vmArgs.options = options;
    vmArgs.ignoreUnrecognized = JNI_TRUE;

    JNIEnv *env;
    res = JNI_CreateJavaVM(&javaVm, (void **)&env, &vmArgs);
    if (res != 0) {
        LogError("Create JVM failed since %d.", res);
    }
}

JNIEnv *JniUtil::GetNewJNIEnv()
{
    std::call_once(initJVMFlag, CreateJavaVM);
    if (javaVm == nullptr) {
        LogError("The java vm is null.");
        return nullptr;
    }
    auto ret = javaVm->GetEnv((void **)&threadLocalEnv, JNI_VERSION_1_8);
    if (ret == JNI_EDETACHED) {
        ret = javaVm->AttachCurrentThread((void **)&threadLocalEnv, nullptr);
    }
    if (ret != 0 || threadLocalEnv == nullptr) {
        LogError("Get jni evn failed, ret:%d.", ret);
        return nullptr;
    }
    return threadLocalEnv;
}

JNIEnv *JniUtil::GetJNIEnv()
{
    if (threadLocalEnv != nullptr) {
        return threadLocalEnv;
    } else {
        return GetNewJNIEnv();
    }
}

ErrorCode JniUtil::GetGlobalClassRef(JNIEnv *env, const char *className, jclass *globalClassRef)
{
    jclass localClass = env->FindClass(className);
    RETURN_ERROR_CODE_IF_EXC(env);
    *globalClassRef = (jclass)(env->NewGlobalRef(localClass));
    RETURN_ERROR_CODE_IF_EXC(env);
    env->DeleteLocalRef(localClass);
    RETURN_ERROR_CODE_IF_EXC(env);
    return ErrorCode::SUCCESS;
}

jclass JniUtil::GetHiveUdfExecutorCls()
{
    return hiveUdfExecutorCls;
}

jmethodID JniUtil::GetExecuteSingleMethod()
{
    return executeSingleMethod;
}

jmethodID JniUtil::GetExecuteBatchMethod()
{
    return executeBatchMethod;
}

jclass JniUtil::GetDataTypeIdCls()
{
    return dataTypeIdCls;
}

jclass JniUtil::GetUdfUtilCls()
{
    return udfUtilCls;
}

jmethodID JniUtil::GetThrowableToStringMethod()
{
    return throwableToStringMethod;
}

std::string JniUtil::GetExceptionMsg(JNIEnv *env)
{
    if (JniUtil::GetUdfUtilCls() == nullptr) {
        return "Load UdfUtil class failed.";
    }
    auto exception = env->ExceptionOccurred();
    env->ExceptionClear();
    auto msg = env->CallStaticObjectMethod(JniUtil::GetUdfUtilCls(), JniUtil::GetThrowableToStringMethod(), exception);
    RETURN_ERROR_MSG_IF_EXC(env, "Call throwableToString throws an exception. The JVM is likely OOM.");

    env->DeleteLocalRef(exception);
    RETURN_ERROR_MSG_IF_EXC(env, "Delete local ref failed since the JVM is likely OOM.");
    JniUtfChars jniUtfChars;
    if (JniUtfChars::Create(env, static_cast<jstring>(msg), &jniUtfChars) != ErrorCode::SUCCESS) {
        return "Create JniUtfChars failed since the JVM is likely OOM.";
    }
    return jniUtfChars.GetUtfChars();
}

jfieldID JniUtil::GetFieldId(int32_t id)
{
    static jfieldID omniDataTypeIds[] = {
        omniNoneField, omniIntField, omniLongField, omniDoubleField, omniBooleanField, omniShortField,
        omniDecimal64Field, omniDecimal128Field, omniData32Field, omniData64Field, omniTime32Field, omniTime64Field,
        omniTimestampField, omniIntervalMonthsField, omniIntervalDayTimeField, omniVarcharField, omniCharField,
        omniContainerField, omniInvalidField
    };
    return omniDataTypeIds[id];
}

ErrorCode JniUtfChars::Create(JNIEnv *env, jstring jString, JniUtfChars *jniUtfChars)
{
    jboolean isCopy;
    const char *chars = env->GetStringUTFChars(jString, &isCopy);
    bool checkException = env->ExceptionCheck();
    if (chars == nullptr || checkException) {
        if (checkException) {
            env->ExceptionClear();
        }
        if (chars != nullptr) {
            env->ReleaseStringUTFChars(jString, chars);
        }
        return ErrorCode::JVM_FAILED;
    }

    jniUtfChars->env = env;
    jniUtfChars->jString = jString;
    jniUtfChars->utfChars = chars;
    return ErrorCode::SUCCESS;
}
