/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
 * Description: JNI Operator Operations Source File
 */
#include "jni_helper.h"
#include "operator/hash_util.h"

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_utils_ShuffleHashHelper_computePartitionIds(
    JNIEnv *env, jclass jClass, jlongArray vecAddrArray, jint partitionNum, jint rowCount)
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
