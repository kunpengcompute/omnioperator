/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: JNI Vector Operations Source File
 */
#include "jni_vector.h"
#include <cstdint>
#include "memory/memory_pool.h"
#include "vector/vector_batch.h"
#include "vector/unsafe_vector.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/mixed_vector.h"
#include "vector/array_vector.h"
#include "jni_common_def.h"
#include "operator/aggregation/container_vector.h"
#include "type/data_type_serializer.h"
#include "memory/thread_memory_manager.h"
#include "util/data_type_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::mem;

static ALWAYS_INLINE BaseVector *TransformVector(long vectorAddr)
{
    return reinterpret_cast<BaseVector *>(vectorAddr);
}

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
    jint jValueCount, jint jVectorEncodingId, jint jVectorTypeId, jint jCapacityInBytes)
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
    jlong jDictionaryNativeVector, jintArray jIds, jint size, jint dataTypeId)
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
    jlong jNativeVector, jint jStartIndex, jint jLength)
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
    jlong jNativeVector, jintArray jPositions, jint jOffset, jint jLength)
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
    JNI_METHOD_START
    BaseVector *nativeVector = TransformVector(jNativeVector);
    DataTypeId typeId = nativeVector->GetTypeId();
    if (typeId != omniruntime::type::OMNI_VARCHAR && typeId != omniruntime::type::OMNI_CHAR
        && typeId != omniruntime::type::OMNI_VARBINARY) {
        throw omniruntime::exception::OmniException("vector type is no supported",
            "the interface only supports varchar/char/varbinary vector.");
    }
    auto *varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(nativeVector);
    return omniruntime::vec::unsafe::UnsafeStringVector::GetContainer(varCharVector)->GetCapacityInBytes();
    JNI_METHOD_END(0)
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
    jlong jNativeVector, jstring dataTypes)
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
    jlong jNativeVectorDest, jint jOffSet, jlong jNativeVectorSrc, jint jLength)
{
    BaseVector *nativeVectorSrc = TransformVector(jNativeVectorSrc);
    BaseVector *nativeVectorDest = TransformVector(jNativeVectorDest);
    JNI_METHOD_START
    VectorHelper::AppendVector(nativeVectorDest, (int32_t)jOffSet, nativeVectorSrc, (int32_t)jLength);
    JNI_METHOD_END()
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VariableWidthVec_getValueOffsetsNative(JNIEnv *env,
    jclass jcls, jlong jNativeVector)
{
    JNI_METHOD_START
    BaseVector *nativeVector = TransformVector(jNativeVector);
    auto offsetsAddr = VectorHelper::UnsafeGetOffsetsAddr(nativeVector);
    if (UNLIKELY(offsetsAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_OFFSETS_FAILED",
            "return a null pointer when getting offsets address");
    }
    return reinterpret_cast<uintptr_t>(offsetsAddr);
    JNI_METHOD_END(0)
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getValueOffsetsNative(JNIEnv *env,
    jclass jcls, jlong jNativeVector)
{
    JNI_METHOD_START
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    auto offsetsAddr = nativeVector->GetOffsets();
    if (UNLIKELY(offsetsAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_OFFSETS_FAILED",
            "return a null pointer when getting offsets address");
    }
    return reinterpret_cast<uintptr_t>(offsetsAddr);
    JNI_METHOD_END(0)
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getValueOffsetsNative(JNIEnv *env,
    jclass jcls, jlong jNativeVector)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);
    auto offsetsAddr = nativeVector->GetOffsets();
    if (UNLIKELY(offsetsAddr == nullptr)) {
        throw omniruntime::exception::OmniException("GET_OFFSETS_FAILED",
        "return a null pointer when getting offsets address");
    }
    return reinterpret_cast<uintptr_t>(offsetsAddr);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_setGlobalMemoryLimitNative(JNIEnv *env,
    jclass jcls, jlong jLimit)
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
    jlongArray jVectorAddresses, jint rRowCount)
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

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_newMixedBatchNative(JNIEnv *env, jclass jcls,
    jlongArray jVectorAddresses, jint rRowCount)
{
    jlong *vecAddresses = env->GetLongArrayElements(jVectorAddresses, JNI_FALSE);
    jsize vecCount = env->GetArrayLength(jVectorAddresses);
    auto batch = reinterpret_cast<MixedVectorBatch *>(vecAddresses[vecCount - 1]);
    if (batch->GetVectorCount() < vecCount) {
        batch->ResizeVectorCount(vecCount - 1);
        for (int i = 0; i < vecCount - 1; ++i) {
            batch->SetVector(i, reinterpret_cast<BaseVector *>(vecAddresses[i]));
        }
    }
    jlong batchHandle = reinterpret_cast<jlong>(batch);
    env->ReleaseLongArrayElements(jVectorAddresses, vecAddresses, JNI_ABORT);
    return batchHandle;
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_freeVectorBatchNative(JNIEnv *env, jclass jcls,
    jlong jVecBatchAddress)
{
    VectorBatch *vecBatch = reinterpret_cast<VectorBatch *>(jVecBatchAddress);
    vecBatch->ClearVectors();
    delete vecBatch;
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_nativeMixType(JNIEnv *env, jclass jcls,
    jlong jVecBatchAddress)
{
    if (jVecBatchAddress == 0) {
        return 0;
    }
    auto batch = reinterpret_cast<MixedVectorBatch *>(jVecBatchAddress);
    return batch->MixType();
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
    jlong jNativeVector, jint jToCapacityInBytes)
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


JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexCapacityNative
        (JNIEnv *env, jclass jcls, jlong jNativeVector, jint jVecEncoding)
{
    return 0;
}


JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newComplexVectorNative
        (JNIEnv *env, jclass jcls, jint jSize, jint jVectorEncodingId, jobjectArray jDataTypes)
{
    BaseVector *vector = nullptr;
    DataType* dataType = nullptr;
    JNI_METHOD_START
        jsize len = env->GetArrayLength(jDataTypes);
        if (len == 0) {
            throw omniruntime::exception::OmniException("INVALID_ARGUMENT", "DataType array is empty");
        }

        std::vector<std::shared_ptr<DataType>> children = DataTypeUtil::ConvertJavaDataTypesToCpp(env, jDataTypes);
        if (jVectorEncodingId == OMNI_ENCODING_MAP) {
            dataType = new MapType(children[0], children[1]);
        } else if (jVectorEncodingId == OMNI_ENCODING_ARRAY) {
            dataType = new ArrayType(children[0]);
        } else if (jVectorEncodingId == OMNI_ENCODING_STRUCT) {
            dataType = new RowType(children);
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

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getValuesAddrNative
    (JNIEnv *env, jclass jlcls, jlong jNativeVector)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);
    return reinterpret_cast<uintptr_t>(nativeVector->GetValueVector().get());
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getKeysAddrNative
    (JNIEnv *env, jclass jlcls, jlong jNativeVector)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);
    return reinterpret_cast<uintptr_t>(nativeVector->GetKeyVector().get());
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getOffsetNative
    (JNIEnv *env, jclass jlcls, jlong jNativeVector, jlong rowId)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);

    return nativeVector->GetOffset(rowId);
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getSizeNative
    (JNIEnv *env, jclass jlcls, jlong jNativeVector, jlong rowId)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);
    return nativeVector->GetSize(rowId);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_StructVec_setChildNative
(JNIEnv *env, jclass jcls, jlong rowVecAddr, jint index, jlong vecAddr)
{
    auto rowVec = reinterpret_cast<RowVector *>(rowVecAddr);
    auto vec =  reinterpret_cast<BaseVector *>(vecAddr);
    rowVec->Set(index, vec);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddKeysNative
(JNIEnv *env, jclass jcls, jlong mapVecAddr, jlong keysAddr) {
    auto mapVec = reinterpret_cast<MapVector *>(mapVecAddr);
    auto keys =  reinterpret_cast<BaseVector *>(keysAddr);
    mapVec->AddKeys(keys);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddValuesNative
  (JNIEnv *env, jclass jcls, jlong mapVecAddr, jlong valuesAddr) {
    auto mapVec = reinterpret_cast<MapVector *>(mapVecAddr);
    auto values =  reinterpret_cast<BaseVector *>(valuesAddr);
    mapVec->AddValues(values);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddOffsetsNative
  (JNIEnv *env, jclass jcls, jlong mapVecAddr, jintArray offsetsAddr) {
    auto mapVec = reinterpret_cast<MapVector *>(mapVecAddr);
    jsize length = env->GetArrayLength(offsetsAddr);
    jint* elements = env->GetIntArrayElements(offsetsAddr, nullptr);
    for (jsize i = 0; i < length; i++) {
        mapVec->SetOffset(i, elements[i]);
    }
}


JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newEmptyComplexVectorNative
    (JNIEnv *env, jclass jcls, jint jSize, jint jVectorEncodingId, jobjectArray jDataTypes)
{
    BaseVector *vector = nullptr;
    DataType* dataType = nullptr;
    JNI_METHOD_START
        jsize len = env->GetArrayLength(jDataTypes);
        if (len == 0) {
            throw omniruntime::exception::OmniException("INVALID_ARGUMENT", "DataType array is empty");
        }

        std::vector<std::shared_ptr<DataType>> children = DataTypeUtil::ConvertJavaDataTypesToCpp(env, jDataTypes);
        if (jVectorEncodingId == OMNI_ENCODING_MAP) {
            dataType = new MapType(children[0], children[1]);
        } else if (jVectorEncodingId == OMNI_ENCODING_ARRAY) {
            dataType = new ArrayType(children[0]);
        } else if (jVectorEncodingId == OMNI_ENCODING_STRUCT) {
            dataType = new RowType(children);
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


JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_StructVec_getChildAddrNative
    (JNIEnv *env, jclass jcls, jlong jNativeVector, jint index)
{
    RowVector *nativeVector = reinterpret_cast<RowVector *>(jNativeVector);
    return reinterpret_cast<uintptr_t>(nativeVector->ChildAt(index).get());
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_StructVec_addChildNative
    (JNIEnv *env, jclass jcls, jlong jNativeVector, jlong addedVecAddr)
{
    auto rowVec = reinterpret_cast<RowVector *>(jNativeVector);
    auto addedVec =  reinterpret_cast<BaseVector *>(addedVecAddr);
    rowVec->AddChild(addedVec);
}

static void LoadDataTypeCls(JNIEnv *env)
{
    if (dataTypeCls == nullptr) {
        dataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/DataType");
        createMethodId = env->GetStaticMethodID(
            dataTypeCls,
            "create",
            "(I)Lnova/hetu/omniruntime/type/DataType;"
        );

        structDataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/StructDataType");
        structDataTypeInitMethodId = env->GetMethodID(
            structDataTypeCls,
            "<init>",
            "([Lnova/hetu/omniruntime/type/DataType;)V"
        );

        mapDataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/MapDataType");
        mapDataTypeInitMethodId = env->GetMethodID(
            mapDataTypeCls,
            "<init>",
            "(Lnova/hetu/omniruntime/type/DataType;Lnova/hetu/omniruntime/type/DataType;)V"
        );

        arrayDataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/ArrayDataType");
        arrayDataTypeInitMethodId = env->GetMethodID(
            arrayDataTypeCls,
            "<init>",
            "(Lnova/hetu/omniruntime/type/DataType;)V"
        );

        decimal64DataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/Decimal64DataType");
        decimal64DataTypeInitMethodId = env->GetMethodID(
            decimal64DataTypeCls,
            "<init>",
            "(II)V"
        );

        decimal128DataTypeCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/type/Decimal128DataType");
        decimal128DataTypeInitMethodId = env->GetMethodID(
            decimal128DataTypeCls,
            "<init>",
            "(II)V"
        );
    }
}

static jobject GetDataType(JNIEnv *env, BaseVector *vec)
{
    if (UNLIKELY(vec == nullptr)) {
        throw omniruntime::exception::OmniException("GET_DATA_TYPE_FAILED",
            "GetDataType received null vector");
    }
    auto id = vec->GetTypeId();
    switch (id) {
        case OMNI_ROW: {
            auto row = static_cast<RowVector*>(vec);
            const jint n = static_cast<jint>(row->ChildSize());
            jobjectArray children = env->NewObjectArray(n, dataTypeCls, nullptr);

            for (jint i = 0; i < n; ++i) {
                BaseVector* child = row->ChildAt(i).get();
                jobject childType = GetDataType(env, child);
                env->SetObjectArrayElement(children, i, childType);
                env->DeleteLocalRef(childType);
            }

            jobject rowType = env->NewObject(structDataTypeCls, structDataTypeInitMethodId, children);
            env->DeleteLocalRef(children);
            return rowType;
        }
        case OMNI_MAP: {
            auto mapVec = reinterpret_cast<MapVector *>(vec);
            jobject keyType = GetDataType(env, mapVec->GetKeyVector().get());
            jobject valueType = GetDataType(env, mapVec->GetValueVector().get());
            jobject mapType = env->NewObject(mapDataTypeCls, mapDataTypeInitMethodId, keyType, valueType);
            env->DeleteLocalRef(keyType);
            env->DeleteLocalRef(valueType);
            return mapType;
        }
        case OMNI_ARRAY: {
            auto arrayVec = reinterpret_cast<ArrayVector *>(vec);
            BaseVector* elementVec = arrayVec->GetElementVector().get();
            jobject elementType = GetDataType(env, elementVec);

            jobject arrayType = env->NewObject(arrayDataTypeCls, arrayDataTypeInitMethodId, elementType);
            env->DeleteLocalRef(elementType);
            return arrayType;
        }
        case OMNI_DECIMAL128: {
            auto dt = vec->GetDataType();
            if (dt != nullptr) {
                auto* decType = static_cast<omniruntime::type::Decimal128DataType*>(dt.get());
                return env->NewObject(decimal128DataTypeCls, decimal128DataTypeInitMethodId,
                    (jint)decType->GetPrecision(), (jint)decType->GetScale());
            }
            return env->CallStaticObjectMethod(dataTypeCls, createMethodId, (jint) id);
        }
        case OMNI_DECIMAL64: {
            auto dt = vec->GetDataType();
            if (dt != nullptr) {
                auto* decType = static_cast<omniruntime::type::Decimal64DataType*>(dt.get());
                return env->NewObject(decimal64DataTypeCls, decimal64DataTypeInitMethodId,
                    (jint)decType->GetPrecision(), (jint)decType->GetScale());
            }
            return env->CallStaticObjectMethod(dataTypeCls, createMethodId, (jint) id);
        }
        default: {
            jobject flatType = env->CallStaticObjectMethod(dataTypeCls, createMethodId, (jint) id);
            return flatType;
        }
    }
}

JNIEXPORT jobject JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexDataTypeNative
        (JNIEnv *env, jclass jcls, jlong jNativeVector)
{
    // create java DataType
    std::call_once(loadDataTypeClsFlag, LoadDataTypeCls, env);

    auto vec = reinterpret_cast<BaseVector *>(jNativeVector);
    jobject dataType = GetDataType(env, vec);
    return dataType;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_setSizeByIndexNative
        (JNIEnv *env, jclass jcls, jlong jNativeVector, jint index, jint size)
{
    MapVector *nativeVector = reinterpret_cast<MapVector *>(jNativeVector);
    nativeVector->SetSize(index, size);
    return 0;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_setSizeByIndexNative
        (JNIEnv *env, jclass jcls, jlong jNativeVector, jint index, jint size)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    nativeVector->SetSize(index, size);
    return 0;
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getElementsAddrNative(JNIEnv *env, jclass jcls, jlong jNativeVector)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return reinterpret_cast<uintptr_t>(nativeVector->GetElementVector().get());
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getOffsetNative(JNIEnv *env, jclass jcls, jlong jNativeVector,
    jlong rowId)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return nativeVector->GetOffset(rowId);
}

extern "C"
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getSizeNative(JNIEnv *env, jclass jcls, jlong jNativeVector,
    jlong rowId)
{
    ArrayVector *nativeVector = reinterpret_cast<ArrayVector *>(jNativeVector);
    return nativeVector->GetSize(rowId);
}

extern "C"
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addElementsNative(JNIEnv *env, jclass jcls, jlong arrayVecAddr,
    jlong elementsAddr)
{
    auto arrayVec = reinterpret_cast<ArrayVector *>(arrayVecAddr);
    auto elements = reinterpret_cast<BaseVector *>(elementsAddr);
    arrayVec->SetElementVectorFromRaw(elements);
}

extern "C"
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addOffsetsNative(JNIEnv *env, jclass jcls, jlong arrayVecAddr,
    jintArray offsetsAddr)
{
    auto arrayVec = reinterpret_cast<ArrayVector *>(arrayVecAddr);
    jsize length = env->GetArrayLength(offsetsAddr);
    jint* elements = env->GetIntArrayElements(offsetsAddr, nullptr);
    for (jsize i = 0; i < length; i++) {
        arrayVec->SetOffset(i, elements[i]);
    }
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstValueAsLongNative(JNIEnv *env, jclass jcls,
    jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    switch (nativeVector->GetTypeId()) {
        case omniruntime::type::OMNI_BYTE:
            return static_cast<jlong>(reinterpret_cast<ConstVector<int8_t> *>(nativeVector)->GetConstValue());
        case omniruntime::type::OMNI_SHORT:
            return static_cast<jlong>(reinterpret_cast<ConstVector<int16_t> *>(nativeVector)->GetConstValue());
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_DATE32:
            return static_cast<jlong>(reinterpret_cast<ConstVector<int32_t> *>(nativeVector)->GetConstValue());
        case omniruntime::type::OMNI_LONG:
        case omniruntime::type::OMNI_TIMESTAMP:
        case omniruntime::type::OMNI_DATE64:
        case omniruntime::type::OMNI_DECIMAL64:
            return static_cast<jlong>(reinterpret_cast<ConstVector<int64_t> *>(nativeVector)->GetConstValue());
        case omniruntime::type::OMNI_BOOLEAN:
            return static_cast<jlong>(reinterpret_cast<ConstVector<bool> *>(nativeVector)->GetConstValue() ? 1 : 0);
        case omniruntime::type::OMNI_FLOAT: {
            float val = reinterpret_cast<ConstVector<float> *>(nativeVector)->GetConstValue();
            int32_t bits;
            memcpy(&bits, &val, sizeof(bits));
            return static_cast<jlong>(bits);
        }
        case omniruntime::type::OMNI_DOUBLE: {
            double val = reinterpret_cast<ConstVector<double> *>(nativeVector)->GetConstValue();
            int64_t bits;
            memcpy(&bits, &val, sizeof(bits));
            return static_cast<jlong>(bits);
        }
        default:
            return 0;
    }
}

JNIEXPORT jbyteArray JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstValueAsBytesNative(JNIEnv *env,
    jclass jcls, jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    // ConstVector for VARCHAR/CHAR is instantiated as ConstVector<std::string_view>,
    // NOT ConstVector<std::string>. They have different memory layouts!
    auto sv = reinterpret_cast<ConstVector<std::string_view> *>(nativeVector)->GetConstValue();
    jbyteArray result = env->NewByteArray(static_cast<jsize>(sv.size()));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(sv.size()),
        reinterpret_cast<const jbyte *>(sv.data()));
    return result;
}

JNIEXPORT jbyteArray JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstDecimal128BytesNative(JNIEnv *env,
    jclass jcls, jlong jNativeVector)
{
    BaseVector *nativeVector = TransformVector(jNativeVector);
    auto val = reinterpret_cast<ConstVector<Decimal128> *>(nativeVector)->GetConstValue();
    jbyteArray result = env->NewByteArray(16);
    env->SetByteArrayRegion(result, 0, 16, reinterpret_cast<const jbyte *>(&val));
    return result;
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MixedVec_freeMixedVectorBatchNative(JNIEnv *env,
    jclass jcls, jlong jBatchHandle)
{
    if (jBatchHandle == 0) {
        return;
    }
    
    MixedVectorBatch *mixedBatch = reinterpret_cast<MixedVectorBatch *>(jBatchHandle);
    // MixedVec is exposed together with the column vectors in a ColumnarBatch.
    // Java column vectors own those BaseVector instances; MixedVec only owns
    // the row payload holder.
    mixedBatch->ClearVectors();
    delete mixedBatch;
}
