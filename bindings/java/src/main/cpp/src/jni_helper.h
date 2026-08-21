/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: JNI Operator Operations Source File
 */

#ifndef JNI_HELPER_H
#define JNI_HELPER_H
#ifdef __cplusplus

#include <jni.h>
#include "jni_common_def.h"

extern "C" {
#endif

/*
 * Class:     nova_hetu_omniruntime_utils_ShuffleHashHelper
 * Method:    ComputePartitionIds
 * Signature: ([JII)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computePartitionIds(JNIEnv *env,
    jclass jClass, jlongArray vecAddrArray, jint partitionNum, jint rowCount);

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computeMixedPartitionIds(JNIEnv *env,
    jclass jClass, jlong jVecBatchAddress, jint partitionNum, jint rowCount);

#ifdef __cplusplus
}
#endif
#endif
