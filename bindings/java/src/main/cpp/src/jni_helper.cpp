/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: JNI Operator Operations Source File
 */
#include "jni_helper.h"
#include "operator/hash_util.h"
#include "memory/omni_task_memory_pool.h"   // OmniTaskMemoryPool* (forward use via reinterpret_cast)

// The trailing `jlong nmmHandle` is a raw `omniruntime::mem::OmniTaskMemoryPool*`
// address as a jlong, computed Java-side via
// OmniNativeMemoryManagerJniWrapper.getPoolAddress().
//
// Why the address-not-handle convention: omni/bindings cannot link against
// gluten/cpp/core types (gluten::MemoryManager), so the decoding from a
// gluten handle to a pool happens on the gluten side via the existing
// `omniruntime::ToOmniTaskPoolOrNull` bridge, exposed as a JNI method
// returning a jlong pool address. The omni-side JNI receives the already-
// decoded pool address and just casts it back.
//
// Null pool path: getPoolAddress returns 0 when the gluten manager isn't an
// OmniMemoryManager (test harness / mock). In that case the partitionIds Vector
// falls back to the legacy unbound 2-arg ctor — accounting skipped.
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computePartitionIds(JNIEnv *env,
    jclass jClass, jlongArray vecAddrArray, jint partitionNum, jint rowCount, jlong nmmHandle)
{
    if (partitionNum == 0) {
        env->ThrowNew(omniRuntimeExceptionClass, "PartitionNum should not be 0");
        return 0;
    }
    jlong result = 0;
    JNI_METHOD_START
    jsize length = env->GetArrayLength(vecAddrArray);
    jlong *addrs = (*env).GetLongArrayElements(vecAddrArray, nullptr);
    std::vector<omniruntime::vec::BaseVector *> vecs;
    for (int i = 0; i < length; ++i) {
        auto vec = reinterpret_cast<omniruntime::vec::BaseVector *>(addrs[i]);
        vecs.push_back(vec);
    }
    env->ReleaseLongArrayElements(vecAddrArray, addrs, JNI_ABORT);

    // Decode the pool address. nmmHandle==0 signals test/mock — pass nullptr
    // through; HashUtil::ComputePartitionIds then constructs the partitionIds
    // Vector via the legacy 2-arg ctor.
    auto *taskPool = reinterpret_cast<omniruntime::mem::OmniTaskMemoryPool *>(nmmHandle);
    auto ret = omniruntime::op::HashUtil::ComputePartitionIds(vecs, partitionNum, rowCount, taskPool);
    result = (jlong)ret.release();
    JNI_METHOD_END(0)
    return result;
}
