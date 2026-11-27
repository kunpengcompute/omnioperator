/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "jni_mock.h"

using omniruntime::mock::InAndOutputInfos;
using omniruntime::mock::JavaVMMock;
using omniruntime::mock::JNIEnvMock;

static jint AttachCurrentThread(JavaVM *vm, void **pEnv, void *args)
{
    return static_cast<JavaVMMock *>(vm)->AttachCurrentThread(pEnv, args);
}

static jint GetEnv(JavaVM *vm, void **pEnv, jint version)
{
    return static_cast<JavaVMMock *>(vm)->GetEnv(pEnv, version);
}

static struct JNIInvokeInterface_ jniInvokeInterface = {nullptr, nullptr, nullptr, nullptr, AttachCurrentThread,
                                                        nullptr, GetEnv,  nullptr};

JavaVMMock::JavaVMMock() { functions = &jniInvokeInterface; }
JavaVMMock::~JavaVMMock() { functions = nullptr; }

static jclass FindClass(JNIEnv *env, const char *name) { return static_cast<JNIEnvMock *>(env)->FindClass(name); }

static jthrowable ExceptionOccurred(JNIEnv *env) { return static_cast<JNIEnvMock *>(env)->ExceptionOccurred(); }

static void ExceptionClear(JNIEnv *env) { static_cast<JNIEnvMock *>(env)->ExceptionClear(); }

static jobject NewGlobalRef(JNIEnv *env, jobject obj) { return static_cast<JNIEnvMock *>(env)->NewGlobalRef(obj); }

static void DeleteLocalRef(JNIEnv *env, jobject obj) { static_cast<JNIEnvMock *>(env)->DeleteLocalRef(obj); }

static jmethodID GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
    return static_cast<JNIEnvMock *>(env)->GetStaticMethodID(clazz, name, sig);
}

static jobject CallStaticObjectMethodV(JNIEnv *env, jclass cls, jmethodID methodID, va_list args)
{
    return static_cast<JNIEnvMock *>(env)->CallStaticObjectMethodV(cls, methodID, args);
}

static jobject CallStaticObjectMethod(JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
    va_list args;
    va_start(args, methodID);
    jobject result = static_cast<JNIEnvMock *>(env)->CallStaticObjectMethodV(cls, methodID, args);
    va_end(args);
    return result;
}

static void CallStaticVoidMethodV(JNIEnv *env, jclass cls, jmethodID methodID, va_list args)
{
    InAndOutputInfos infos{};
    static_cast<JNIEnvMock *>(env)->CallStaticVoidMethodV(cls, methodID, infos);
}

static void CallStaticVoidMethod(JNIEnv *env, jclass cls, jmethodID methodID, ...)
{
    va_list args;
    va_start(args, methodID);
    CallStaticVoidMethodV(env, cls, methodID, args);
    va_end(args);
}

static jfieldID GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
{
    return static_cast<JNIEnvMock *>(env)->GetStaticFieldID(clazz, name, sig);
}

static jobject GetStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
    return static_cast<JNIEnvMock *>(env)->GetStaticObjectField(clazz, fieldID);
}

static jstring NewStringUTF(JNIEnv *env, const char *bytes)
{
    return static_cast<JNIEnvMock *>(env)->NewStringUTF(bytes);
}

static const char *GetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy)
{
    return static_cast<JNIEnvMock *>(env)->GetStringUTFChars(str, isCopy);
}

static void ReleaseStringUTFChars(JNIEnv *env, jstring str, const char *chars)
{
    return static_cast<JNIEnvMock *>(env)->ReleaseStringUTFChars(str, chars);
}

static jobjectArray NewObjectArray(JNIEnv *env, jsize len, jclass clazz, jobject init)
{
    return static_cast<JNIEnvMock *>(env)->NewObjectArray(len, clazz, init);
}

static void SetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject val)
{
    return static_cast<JNIEnvMock *>(env)->SetObjectArrayElement(array, index, val);
}

static jboolean ExceptionCheck(JNIEnv *env) { return static_cast<JNIEnvMock *>(env)->ExceptionCheck(); }

static struct JNINativeInterface_ jniNativeInterface;

JNIEnvMock::JNIEnvMock()
{
    jniNativeInterface.FindClass = ::FindClass;
    jniNativeInterface.ExceptionOccurred = ::ExceptionOccurred;
    jniNativeInterface.ExceptionClear = ::ExceptionClear;
    jniNativeInterface.NewGlobalRef = ::NewGlobalRef;
    jniNativeInterface.DeleteLocalRef = ::DeleteLocalRef;
    jniNativeInterface.GetStaticMethodID = ::GetStaticMethodID;
    jniNativeInterface.CallStaticObjectMethod = ::CallStaticObjectMethod;
    jniNativeInterface.CallStaticObjectMethodV = ::CallStaticObjectMethodV;
    jniNativeInterface.CallStaticVoidMethod = ::CallStaticVoidMethod;
    jniNativeInterface.CallStaticVoidMethodV = ::CallStaticVoidMethodV;
    jniNativeInterface.GetStaticFieldID = ::GetStaticFieldID;
    jniNativeInterface.GetStaticObjectField = ::GetStaticObjectField;
    jniNativeInterface.NewStringUTF = ::NewStringUTF;
    jniNativeInterface.GetStringUTFChars = ::GetStringUTFChars;
    jniNativeInterface.ReleaseStringUTFChars = ::ReleaseStringUTFChars;
    jniNativeInterface.NewObjectArray = ::NewObjectArray;
    jniNativeInterface.SetObjectArrayElement = ::SetObjectArrayElement;
    jniNativeInterface.ExceptionCheck = ::ExceptionCheck;
    functions = &jniNativeInterface;
}

JNIEnvMock::~JNIEnvMock() { functions = nullptr; }

JavaVMMock *omniruntime::mock::CreateJavaVMMock() { return new JavaVMMock; }
void omniruntime::mock::DestroyJavaVMMock(JavaVMMock *javaVM) { delete javaVM; }
JNIEnvMock *omniruntime::mock::CreateJNIEnvMock() { return new JNIEnvMock; }
void omniruntime::mock::DestroyJNIEnvMock(JNIEnvMock *env) { delete env; }
