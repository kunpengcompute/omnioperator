/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: JNI Vector Operations Header
 */
#ifndef JNI_VECTOR_H
#define JNI_VECTOR_H
#include <jni.h>
#include <mutex>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    newVectorNative
 * Signature: (IIII)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_newVectorNative(JNIEnv *, jclass, jint, jint, jint, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    newDictionaryVectorNative
 * Signature: (J[III)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_newDictionaryVectorNative(JNIEnv *, jclass, jlong,
    jintArray, jint, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    sliceVectorNative
 * Signature: (JII)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_sliceVectorNative(JNIEnv *, jclass, jlong, jint, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    copyPositionsNative
 * Signature: (J[III)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_copyPositionsNative(JNIEnv *, jclass, jlong, jintArray,
    jint, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    freeVectorNative
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_freeVectorNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    getCapacityInBytesNative
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getCapacityInBytesNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    getSizeNative
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getSizeNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    setSizeNative
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_setSizeNative(JNIEnv *, jclass, jlong, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    getValuesNative
 * Signature: (J)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_getValuesNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    getValueNullsNative
 * Signature: (J)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_Vec_getValueNullsNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    appendVectorNative
 * Signature: (JIJI)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_appendVectorNative(JNIEnv *, jclass, jlong, jint, jlong,
    jint);

/*
 * Class:     nova_hetu_omniruntime_vector_ContainerVec
 * Method:    getPositionNative
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_getPositionNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_ContainerVec
 * Method:    setDataTypesNative
 * Signature: (JLjava/lang/Sting;)V;
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_setDataTypesNative(JNIEnv *, jclass, jlong,
    jstring);

/*
 * Class:     nova_hetu_omniruntime_vector_ContainerVec
 * Method:    getDataTypesNative
 * Signature: (J)Ljava/lang/Sting;
 */
JNIEXPORT jstring JNICALL Java_nova_hetu_omniruntime_vector_ContainerVec_getDataTypesNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_VariableWidthVec
 * Method:    getValueOffsetsNative
 * Signature: (J)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VariableWidthVec_getValueOffsetsNative(JNIEnv *, jclass,
    jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_ArrayVec
 * Method:    getValueOffsetsNative
 * Signature: (J)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_getValueOffsetsNative(JNIEnv *, jclass,
    jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_MapVec
 * Method:    getValueOffsetsNative
 * Signature: (J)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_getValueOffsetsNative(JNIEnv *, jclass,
    jlong);

/*
 * Class:     Java_nova_hetu_omniruntime_memory_MemoryManager
 * Method:    setGlobalMemoryLimitNative
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_setGlobalMemoryLimitNative(JNIEnv *,
    jclass, jlong);

/*
 * Class:     Java_nova_hetu_omniruntime_memory_MemoryManager
 * Method:    getAllocatedMemoryNative
 * Signature: (V)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_getAllocatedMemoryNative(JNIEnv *,
    jclass);

/*
 * Class:     Java_nova_hetu_omniruntime_memory_MemoryManager
 * Method:    memoryClearNative
 * Signature: (V)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_memoryClearNative(JNIEnv *env,
    jclass jcls);

/*
 * Class:     Java_nova_hetu_omniruntime_memory_MemoryManager
 * Method:    memoryReclamationNative
 * Signature: (V)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_memory_MemoryManager_memoryReclamationNative(JNIEnv *env,
    jclass jcls);

/*
 * Class:     nova_hetu_omniruntime_vector_VecBatch
 * Method:    newVectorBatchNative
 * Signature: ([JI)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_newVectorBatchNative(JNIEnv *, jclass, jlongArray,
    jint);

/*
 * Class:     nova_hetu_omniruntime_vector_VecBatch
 * Method:    newMixedBatchNative
 * Signature: ([JI)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_newMixedBatchNative(JNIEnv *, jclass, jlongArray,
    jint);

/*
 * Class:     nova_hetu_omniruntime_vector_VecBatch
 * Method:    freeVectorBatchNative
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_freeVectorBatchNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_VecBatch
 * Method:    nativeMixType
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_VecBatch_nativeMixType(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_DictionaryVec
 * Method:    getDictionaryNative
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_DictionaryVec_getDictionaryNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    getVecEncodingNative
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_Vec_getVecEncodingNative(JNIEnv *, jclass, jlong);
/*
 * Class:     nova_hetu_omniruntime_vector_VarcharVec
 * Method:    expandDataCapacity
 * Signature: (JI)J;
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_VarcharVec_expandDataCapacity(JNIEnv *, jclass,
    jlong, jint);
/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    setNullFlagNative
 * Signature: (JZ)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_Vec_setNullFlagNative(JNIEnv *, jclass, jlong, jboolean);

/*
 * Class:     nova_hetu_omniruntime_vector_Vec
 * Method:    hasNullNative
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_nova_hetu_omniruntime_vector_Vec_hasNullNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_ComplexVec
 * Method:    getComplexCapacityNative
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexCapacityNative
        (JNIEnv *, jclass, jlong, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_ComplexVec
 * Method:    newComplexVectorNative
 * Signature: (II[Lnova/hetu/omniruntime/type/DataType;)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newComplexVectorNative
        (JNIEnv *, jclass, jint, jint, jobjectArray);

/*
* Class:     nova_hetu_omniruntime_vector_StructVec
* Method:    setChildNative
* Signature: (JIJ)V
*/
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_StructVec_setChildNative
        (JNIEnv *env, jclass jcls, jlong rowVecAddr, jint index, jlong vecAddr);

/*
* Class:     nova_hetu_omniruntime_vector_MapVec
* Method:    AddKeysNative
* Signature: (JJ)V
*/
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddKeysNative
        (JNIEnv *env, jclass jcls, jlong mapVecAddr, jlong keysAddr);


/*
* Class:     nova_hetu_omniruntime_vector_MapVec
* Method:    AddValuesNative
* Signature: (JJ)V
*/
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddValuesNative
        (JNIEnv *env, jclass jcls, jlong mapVecAddr, jlong valuesAddr);

/*
* Class:     nova_hetu_omniruntime_vector_MapVec
* Method:    AddOffsetsNative
* Signature: (J[I)V
*/
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MapVec_AddOffsetsNative
        (JNIEnv *env, jclass jcls, jlong mapVecAddr, jintArray offsetsAddr);


JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_newEmptyComplexVectorNative
        (JNIEnv *env, jclass jcls, jint jSize, jint jVectorEncodingId, jobjectArray jDataTypes);

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_StructVec_getChildAddrNative
        (JNIEnv *env, jclass jcls, jlong jNativeVector, jint index);

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_StructVec_addChildNative
    (JNIEnv *env, jclass jcls, jlong jNativeVector, jlong addedVecAddr);

/*
 * Class:     nova_hetu_omniruntime_vector_ComplexVec
 * Method:    getComplexDataTypeNative
 * Signature: (J)Lnova/hetu/omniruntime/type/DataType;
 */
JNIEXPORT jobject JNICALL Java_nova_hetu_omniruntime_vector_ComplexVec_getComplexDataTypeNative
        (JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_MapVec
 * Method:    setSizeByIndexNative
 * Signature: (JII)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_MapVec_setSizeByIndexNative
        (JNIEnv *, jclass, jlong, jint, jint);

/*
 * Class:     nova_hetu_omniruntime_vector_ArrayVec
 * Method:    setSizeByIndexNative
 * Signature: (JII)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_setSizeByIndexNative
        (JNIEnv *, jclass, jlong, jint, jint);

static std::once_flag loadDataTypeClsFlag;

static jclass dataTypeCls = nullptr;
static jclass structDataTypeCls = nullptr;
static jclass mapDataTypeCls = nullptr;
static jclass arrayDataTypeCls = nullptr;
static jclass decimal64DataTypeCls = nullptr;
static jclass decimal128DataTypeCls = nullptr;

static jmethodID createMethodId = nullptr;
static jmethodID structDataTypeInitMethodId = nullptr;
static jmethodID mapDataTypeInitMethodId = nullptr;
static jmethodID arrayDataTypeInitMethodId = nullptr;
static jmethodID decimal64DataTypeInitMethodId = nullptr;
static jmethodID decimal128DataTypeInitMethodId = nullptr;

/*
 * Class:     nova_hetu_omniruntime_vector_ArrayVec
 * Method:    addElementsNative
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addElementsNative(JNIEnv *env, jclass jcls, jlong arrayVecAddr,
    jlong elementsAddr);

/*
 * Class:     nova_hetu_omniruntime_vector_ArrayVec
 * Method:    addOffsetsNative
 * Signature: (J[I)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_ArrayVec_addOffsetsNative(JNIEnv *env, jclass jcls, jlong arrayVecAddr,
    jintArray offsetsAddr);


/*
 * Class:     nova_hetu_omniruntime_vector_ConstVec
 * Method:    getConstValueAsLongNative
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstValueAsLongNative(JNIEnv *, jclass, jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_ConstVec
 * Method:    getConstValueAsBytesNative
 * Signature: (J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstValueAsBytesNative(JNIEnv *, jclass,
    jlong);

/*
 * Class:     nova_hetu_omniruntime_vector_ConstVec
 * Method:    getConstDecimal128BytesNative
 * Signature: (J)[B
 */
JNIEXPORT jbyteArray JNICALL Java_nova_hetu_omniruntime_vector_ConstVec_getConstDecimal128BytesNative(JNIEnv *, jclass,
    jlong);

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_MixedVec_freeMixedVectorBatchNative(JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif
#endif
