/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
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
 * Description: JNI Vector Operations Source File
 */
#include "jni_vector.h"
#include <cstdint>
#include "memory/memory_pool.h"
#include "vector/vector_batch.h"
#include "vector/unsafe_vector.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "jni_common_def.h"
#include "operator/aggregation/container_vector.h"
#include "type/data_type_serializer.h"
#include "memory/thread_memory_manager.h"
#include "util/data_type_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::mem;

static ALWAYS_INLINE BaseVector *TransformVector(long vectorAddr) { return reinterpret_cast<BaseVector *>(vectorAddr); }

#ifdef TRACE
static void RecordStack(BaseVector *vector, JNIEnv *env)
{
    jstring jstack = (jstring)env->CallStaticObjectMethod(traceUtilCls, traceUtilStackMethodId);
    auto stackChars = env->GetStringUTFChars(jstack, JNI_FALSE);
    std::string stack(stackChars);
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    // replace c++ stack with java stack after vector is created.
    threadMemoryTrace->ReplaceVectorTracedLog(reinterpret_cast<uintptr_t>(vector), stack);
    env->ReleaseStringUTFChars(jstack, stackChars);
}
#endif

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_newVectorNative(JNIEnv *env, jclass jcls,
                                                                              jint jValueCount, jint jVectorEncodingId,
                                                                              jint jVectorTypeId, jint jCapacityInBytes)
{
    BaseVector *vector = nullptr;
    JNI_METHOD_START
    vector = VectorHelper::CreateVector(jVectorEncodingId, jVectorTypeId, jValueCount, jCapacityInBytes);
    if (UNLIKELY(vector == nullptr)) {
        throw omniruntime::exception::OmniException("CREATE_FLAT_VECTOR_FAILED",
                                                    "return a null pointer when creating flat vector");
    }
    JNI_METHOD_END(0)
#ifdef TRACE
    RecordStack(vector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(vector));
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_newDictionaryVectorNative(JNIEnv *env, jclass jcls,
                                                                                        jlong jDictionaryNativeVector,
                                                                                        jintArray jIds, jint size,
                                                                                        jint dataTypeId)
{
    BaseVector *dictionaryVector = TransformVector(jDictionaryNativeVector);
    jint idsArray[size];
    env->GetIntArrayRegion(jIds, 0, size, idsArray);
    jint *ids = idsArray;
    BaseVector *vector = nullptr;
    JNI_METHOD_START
    vector = VectorHelper::CreateDictionaryVector(ids, size, dictionaryVector, dataTypeId);
    if (UNLIKELY(vector == nullptr)) {
        throw omniruntime::exception::OmniException("CREATE_DICTIONARY_VECTOR_FAILED",
                                                    "return a null pointer when creating dictionary vector");
    }
    JNI_METHOD_END(0)
#ifdef TRACE
    RecordStack(vector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(vector));
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_sliceVectorNative(JNIEnv *env, jclass jcls,
                                                                                jlong jNativeVector, jint jStartIndex,
                                                                                jint jLength)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    BaseVector *sliceVector = nullptr;
    JNI_METHOD_START
    sliceVector = VectorHelper::SliceVector(nativeVector, jStartIndex, jLength);
    JNI_METHOD_END(0)
#ifdef TRACE
    RecordStack(sliceVector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(sliceVector));
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_copyPositionsNative(JNIEnv *env, jclass jcls,
                                                                                  jlong jNativeVector,
                                                                                  jintArray jPositions, jint jOffset,
                                                                                  jint jLength)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    jint positionArray[jLength];
    env->GetIntArrayRegion(jPositions, jOffset, jLength, positionArray);
    jint *positions = positionArray;
    BaseVector *copyVector = nullptr;
    JNI_METHOD_START
    copyVector = VectorHelper::CopyPositionsVector(nativeVector, reinterpret_cast<int *>(positions), 0, jLength);
    JNI_METHOD_END(0)
#ifdef TRACE
    RecordStack(copyVector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(copyVector));
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_freeVectorNative(JNIEnv *env, jclass jcls,
                                                                              jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    if (nativeVector == nullptr) {
        std::cerr << "free vector native vector is null:" << jNativeVector << std::endl;
    }
    delete nativeVector;
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getCapacityInBytesNative(JNIEnv *env, jclass jcls,
                                                                                      jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    if (nativeVector == nullptr) {
        throw omniruntime::exception::OmniException("getCapacityInBytesNative failed", "native vector is null");
    }
    DataTypeId typeId = nativeVector->GetTypeId();
    if (typeId != omniruntime::type::OMNI_VARCHAR && typeId != omniruntime::type::OMNI_CHAR) {
        throw omniruntime::exception::OmniException("vector type is no supported",
                                                    "the interface only supports varchar/char vector.");
    }
    auto *varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(nativeVector);
    return omniruntime::vec::unsafe::UnsafeStringVector::GetContainer(varCharVector)->GetCapacityInBytes();
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getSizeNative(JNIEnv *env, jclass jcls,
                                                                           jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    return nativeVector->GetSize();
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_setSizeNative(JNIEnv *env, jclass jcls,
                                                                           jlong jNativeVector, jint jSize)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    if (jSize < 0 || jSize > nativeVector->GetSize()) {
        std::cerr << "size is error, the range is[0," << nativeVector->GetSize() << "]" << std::endl;
        return jSize;
    }
    omniruntime::vec::unsafe::UnsafeBaseVector::SetSize(nativeVector, jSize);
    return jSize;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_getValuesNative(JNIEnv *env, jclass jlcls,
                                                                              jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    return reinterpret_cast<uintptr_t>(VectorHelper::UnsafeGetValues(nativeVector));
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_getValueNullsNative(JNIEnv *env, jclass jcls,
                                                                                  jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    return reinterpret_cast<uintptr_t>(omniruntime::vec::unsafe::UnsafeBaseVector::GetNulls(nativeVector));
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_getPositionNative(JNIEnv *env, jclass jcls,
                                                                                        jlong jNativeVector)
{
    ContainerVector *containerVec = reinterpret_cast<ContainerVector *>(jNativeVector);
    return containerVec->GetSize();
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_setDataTypesNative(JNIEnv *env, jclass jcls,
                                                                                         jlong jNativeVector,
                                                                                         jstring dataTypes)
{
    ContainerVector *containerVec = reinterpret_cast<ContainerVector *>(jNativeVector);
    auto dataTypeString = env->GetStringUTFChars(dataTypes, JNI_FALSE);
    containerVec->SetDataTypes(omniruntime::type::Deserialize(dataTypeString).Get());
    env->ReleaseStringUTFChars(dataTypes, dataTypeString);
}

JNIEXPORT jstring JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_getDataTypesNative(JNIEnv *env, jclass jcls,
                                                                                            jlong jNativeVector)
{
    ContainerVector *containerVec = reinterpret_cast<ContainerVector *>(jNativeVector);
    auto &DataTypes = containerVec->GetDataTypes();
    return env->NewStringUTF(Serialize(DataTypes).data());
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_appendVectorNative(JNIEnv *env, jclass jcls,
                                                                                jlong jNativeVectorDest, jint jOffSet,
                                                                                jlong jNativeVectorSrc, jint jLength)
{
    BaseVector *nativeVectorSrc = TransformVector(jNativeVectorSrc);
    BaseVector *nativeVectorDest = TransformVector(jNativeVectorDest);
    JNI_METHOD_START
    VectorHelper::AppendVector(nativeVectorDest, (int32_t)jOffSet, nativeVectorSrc, (int32_t)jLength);
    JNI_METHOD_END()
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VariableWidthVec_getValueOffsetsNative(JNIEnv *env,
                                                                                                 jclass jcls,
                                                                                                 jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    auto offsetsAddr = VectorHelper::UnsafeGetOffsetsAddr(nativeVector);
    if (UNLIKELY(offsetsAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_OFFSETS_FAILED",
                                                    "return a null pointer when getting offsets address");
    }
    return reinterpret_cast<uintptr_t>(offsetsAddr);
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getValueOffsetsNative(JNIEnv *env, jclass jcls,
                                                                                         jlong jNativeVector)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    auto offsetsAddr = nativeVector->GetOffsets();
    if (UNLIKELY(offsetsAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_OFFSETS_FAILED",
                                                    "return a null pointer when getting offsets address");
    }
    return reinterpret_cast<uintptr_t>(offsetsAddr);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_setGlobalMemoryLimitNative(JNIEnv *env,
                                                                                                  jclass jcls,
                                                                                                  jlong jLimit)
{
    omniruntime::mem::MemoryManager::SetGlobalMemoryLimit(jLimit);
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_getAllocatedMemoryNative(JNIEnv *env,
                                                                                                 jclass jcls)
{
    auto threadMemoryManager = omniruntime::mem::ThreadMemoryManager::GetThreadMemoryManager();
    int64_t accountedMemory = threadMemoryManager->GetThreadAccountedMemory();
    int64_t untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    return accountedMemory + untrackedMemory;
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_memoryClearNative(JNIEnv *env, jclass jcls)
{
    auto threadMemoryManager = omniruntime::mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_memoryReclamationNative(JNIEnv *env, jclass jcls)
{
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    if (threadMemoryTrace->HasMemoryLeak()) {
        threadMemoryTrace->FreeLeakedMemory();
    }
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_newVectorBatchNative(JNIEnv *env, jclass jcls,
                                                                                        jlongArray jVectorAddresses,
                                                                                        jint rRowCount)
{
    jlong *vecAddresses = env->GetLongArrayElements(jVectorAddresses, JNI_FALSE);
    jsize vecCount = env->GetArrayLength(jVectorAddresses);
    VectorBatch *vecBatch = new VectorBatch(rRowCount);
    for (int i = 0; i < vecCount; ++i) {
        vecBatch->Append(reinterpret_cast<BaseVector *>(vecAddresses[i]));
    }
    env->ReleaseLongArrayElements(jVectorAddresses, vecAddresses, JNI_ABORT);
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(vecBatch));
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_freeVectorBatchNative(JNIEnv *env, jclass jcls,
                                                                                        jlong jVecBatchAddress)
{
    VectorBatch *vecBatch = reinterpret_cast<VectorBatch *>(jVecBatchAddress);
    vecBatch->ClearVectors();
    delete vecBatch;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_DictionaryVec_getDictionaryNative(JNIEnv *env, jclass jcls,
                                                                                            jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    auto dictionaryAddr = VectorHelper::UnsafeGetDictionary(nativeVector);
    if (UNLIKELY(dictionaryAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_DICTIONARY_NATIVE_FAILED",
                                                    "return a null pointer when getting dictionary address");
    }
    return reinterpret_cast<uintptr_t>(dictionaryAddr);
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getVecEncodingNative(JNIEnv *env, jclass jcls,
                                                                                  jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    return nativeVector->GetEncoding();
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VarcharVec_expandDataCapacity(JNIEnv *env, jclass jcls,
                                                                                        jlong jNativeVector,
                                                                                        jint jToCapacityInBytes)
{
    auto nativeVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(jNativeVector);
    char *newBuffAddress =
        omniruntime::vec::unsafe::UnsafeStringVector::ExpandStringBuffer(nativeVector, jToCapacityInBytes);
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(newBuffAddress));
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_setNullFlagNative(JNIEnv *env, jclass jcls,
                                                                               jlong jNativeVector, jboolean jHasNull)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    nativeVector->SetNullFlag(jHasNull);
}

JNIEXPORT jboolean JNICALL Java_nova_hetu_omniruntime_vector_Vec_hasNullNative(JNIEnv *env, jclass jcls,
                                                                               jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    return nativeVector->HasNull();
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexCapacityNative(JNIEnv *env, jclass jcls,
                                                                                             jlong jNativeVector,
                                                                                             jint jVecEncoding)
{
    return 0;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newComplexVectorNative(JNIEnv *env, jclass jcls,
                                                                                            jint jSize,
                                                                                            jint jVectorEncodingId,
                                                                                            jobjectArray jDataTypes)
{
    BaseVector *vector = nullptr;
    DataType *dataType = nullptr;
    JNI_METHOD_START
    jsize len = env->GetArrayLength(jDataTypes);
    if (len == 0) {
        throw omniruntime::exception::OmniException("INVALID_ARGUMENT", "DataType array is empty");
    }

    std::vector<std::shared_ptr<DataType>> children = DataTypeUtil::ConvertJavaDataTypesToCpp(env, jDataTypes);
    if (jVectorEncodingId == OMNI_ENCODING_ARRAY) {
        dataType = new ArrayType(children[0]);
    } else {
        std::string omniExceptionInfo =
            "In function CreateVector, no such encoding type " + std::to_string(jVectorEncodingId);
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }

    vector = VectorHelper::CreateComplexVector(dataType, jSize);
    if (UNLIKELY(vector == nullptr)) {
        throw omniruntime::exception::OmniException("CREATE_COMPLEX_VECTOR_FAILED",
                                                    "return a null pointer when creating complex vector");
    }
    delete dataType;
    JNI_METHOD_END_WITH_COMPLEX_VECTOR(0, dataType, vector)
#ifdef TRACE
    RecordStack(vector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(vector));
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newEmptyComplexVectorNative(
    JNIEnv *env, jclass jcls, jint jSize, jint jVectorEncodingId, jobjectArray jDataTypes)
{
    BaseVector *vector = nullptr;
    DataType *dataType = nullptr;
    JNI_METHOD_START
    jsize len = env->GetArrayLength(jDataTypes);
    if (len == 0) {
        throw omniruntime::exception::OmniException("INVALID_ARGUMENT", "DataType array is empty");
    }

    std::vector<std::shared_ptr<DataType>> children = DataTypeUtil::ConvertJavaDataTypesToCpp(env, jDataTypes);
    if (jVectorEncodingId == OMNI_ENCODING_ARRAY) {
        dataType = new ArrayType(children[0]);
    } else {
        std::string omniExceptionInfo =
            "In function CreateVector, no such encoding type " + std::to_string(jVectorEncodingId);
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
    }

    vector = VectorHelper::CreateEmptyComplexVector(dataType, jSize);
    if (UNLIKELY(vector == nullptr)) {
        throw omniruntime::exception::OmniException("CREATE_COMPLEX_VECTOR_FAILED",
                                                    "return a null pointer when creating complex vector");
    }
    delete dataType;
    JNI_METHOD_END_WITH_COMPLEX_VECTOR(0, dataType, vector)
#ifdef TRACE
    RecordStack(vector, env);
#endif
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(vector));
}

static void LoadDataTypeCls(JNIEnv *env)
{
    if (dataTypeCls == nullptr) {
        dataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/DataType");
        createMethodId = env->GetStaticMethodID(dataTypeCls, "create", "(I)Lnova/hetu/omniruntime/type/DataType;");

        arrayDataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/ArrayDataType");
        arrayDataTypeInitMethodId =
            env->GetMethodID(arrayDataTypeCls, "<init>", "(Lnova/hetu/omniruntime/type/DataType;)V");
    }
}

static jobject GetDataType(JNIEnv *env, BaseVector *vec)
{
    if (UNLIKELY(vec == nullptr)) {
        throw omniruntime::exception::OmniException("GET_DATA_TYPE_FAILED", "GetDataType received null vector");
    }
    auto id = vec->GetTypeId();
    switch (id) {
        case OMNI_ARRAY: {
            auto arrayVec = reinterpret_cast<ArrayVector *>(vec);
            BaseVector *elementVec = arrayVec->GetElementVector().get();
            jobject elementType = GetDataType(env, elementVec);
            jobject arrayType = env->NewObject(arrayDataTypeCls, arrayDataTypeInitMethodId, elementType);
            env->DeleteLocalRef(elementType);
            return arrayType;
        }
        default: {
            jobject flatType = env->CallStaticObjectMethod(dataTypeCls, createMethodId, (jint)id);
            return flatType;
        }
    }
}

JNIEXPORT jobject JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexDataTypeNative(JNIEnv *env,
                                                                                                jclass jcls,
                                                                                                jlong jNativeVector)
{
    // create java DataType
    std::call_once(loadDataTypeClsFlag, LoadDataTypeCls, env);

    auto vec = reinterpret_cast<BaseVector *>(jNativeVector);
    jobject dataType = GetDataType(env, vec);
    return dataType;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_setSizeByIndexNative(JNIEnv *env, jclass jcls,
                                                                                        jlong jNativeVector, jint index,
                                                                                        jint size)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    nativeVector->SetSize(index, size);
    return 0;
}

extern "C" JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getElementsAddrNative(JNIEnv *env,
                                                                                                    jclass jcls,
                                                                                                    jlong jNativeVector)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return reinterpret_cast<uintptr_t>(nativeVector->GetElementVector().get());
}

extern "C" JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getOffsetNative(JNIEnv *env, jclass jcls,
                                                                                              jlong jNativeVector,
                                                                                              jlong rowId)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return nativeVector->GetOffset(rowId);
}

extern "C" JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getSizeNative(JNIEnv *env, jclass jcls,
                                                                                            jlong jNativeVector,
                                                                                            jlong rowId)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return nativeVector->GetSize(rowId);
}

extern "C" JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addElementsNative(JNIEnv *env, jclass jcls,
                                                                                               jlong arrayVecAddr,
                                                                                               jlong elementsAddr)
{
    auto arrayVec = reinterpret_cast<ArrayVector *>(arrayVecAddr);
    auto elements = reinterpret_cast<BaseVector *>(elementsAddr);
    arrayVec->AddElements(elements);
}

extern "C" JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addOffsetsNative(JNIEnv *env, jclass jcls,
                                                                                              jlong arrayVecAddr,
                                                                                              jintArray offsetsAddr)
{
    auto arrayVec = reinterpret_cast<ArrayVector *>(arrayVecAddr);
    jsize length = env->GetArrayLength(offsetsAddr);
    jint *elements = env->GetIntArrayElements(offsetsAddr, nullptr);
    if (UNLIKELY(elements == nullptr)) {
        throw omniruntime::exception::OmniException("GET_ELEMENTS_FAILED", "GetIntArrayElements failed");
    }
    for (jsize i = 0; i < length; i++) {
        arrayVec->SetOffset(i, elements[i]);
    }
}
