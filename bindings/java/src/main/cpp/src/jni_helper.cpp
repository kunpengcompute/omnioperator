/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: JNI Operator Operations Source File
 */
#include "jni_helper.h"
#include "operator/hash_util.h"
#include "vector/mixed_vector.h"

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computePartitionIds(JNIEnv *env,
    jclass jClass, jlongArray vecAddrArray, jint partitionNum, jint rowCount)
{
    if (partitionNum == 0) {
        env->ThrowNew(omniRuntimeExceptionClass, "PartitionNum should not be 0");
        return 0;
    }
    jsize length = env->GetArrayLength(vecAddrArray);
    jlong *addrs = (*env).GetLongArrayElements(vecAddrArray, nullptr);
    std::vector<omniruntime::vec::BaseVector *> vecs;
    for (int i = 0; i < length; ++i) {
        auto vec = reinterpret_cast<omniruntime::vec::BaseVector *>(addrs[i]);
        vecs.push_back(vec);
    }
    env->ReleaseLongArrayElements(vecAddrArray, addrs, JNI_ABORT);
    auto ret = omniruntime::op::HashUtil::ComputePartitionIds(vecs, partitionNum, rowCount);
    return (jlong)ret.release();
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computeMixedPartitionIds(JNIEnv *env,
    jclass jClass, jlong jVecBatchAddress, jint partitionNum, jint rowCount)
{
    if (partitionNum == 0) {
        env->ThrowNew(omniRuntimeExceptionClass, "PartitionNum should not be 0");
        return 0;
    }
    auto mixedVectorBatch = reinterpret_cast<omniruntime::vec::MixedVectorBatch *>(jVecBatchAddress);
    auto ret = omniruntime::op::HashUtil::ComputePartitionIdsMixed(*mixedVectorBatch, partitionNum, rowCount);
    return (jlong)ret.release();
}