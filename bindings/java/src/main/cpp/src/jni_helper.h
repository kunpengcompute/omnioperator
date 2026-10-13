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
 * Signature: ([JIIJ)J
 *
 * The trailing `jlong nmmHandle` routes hash-util allocations (the partitionIds
 * Vector<int32_t>) to the task pool via the pool-bound Allocator decoded from
 * the handle. nmmHandle==0 falls back to the legacy unscoped allocator path.
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computePartitionIds(JNIEnv *env,
    jclass jClass, jlongArray vecAddrArray, jint partitionNum, jint rowCount, jlong nmmHandle);

#ifdef __cplusplus
}
#endif
#endif
