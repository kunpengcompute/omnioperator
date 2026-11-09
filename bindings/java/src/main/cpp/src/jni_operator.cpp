/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2025. All rights reserved.
 * Description: JNI Operator Operations Source File
 */

#include <vector>
#include "securec.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "jni_common_def.h"
#include "operator/operator_factory.h"
#include "operator/aggregation/group_aggregation_expr.h"
#include "vector/omni_row.h"
#include "jni_operator.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;

static std::once_flag loadVecBatchClsFlag;

static jclass vecBatchCls = nullptr;
static jclass rowBatchCls = nullptr;
static jclass rowCls = nullptr;
static jclass omniResultsCls = nullptr;
static jclass rowResultsCls = nullptr;

static jmethodID omniResultsInitMethodId = nullptr;
static jmethodID vecBatchInitMethodId = nullptr;
static jmethodID rowBatchInitMethodId = nullptr;
static jmethodID rowInitMethodId = nullptr;
static jmethodID rowResultsInitMethodId = nullptr;

static void RecordInputVectorsStack(VectorBatch *vectorBatch, JNIEnv *env)
{
#ifdef DEBUG_VECTOR
    jstring jstack = (jstring)env->CallStaticObjectMethod(traceUtilCls, traceUtilStackMethodId);
    auto stackChars = env->GetStringUTFChars(jstack, JNI_FALSE);
    std::string stack(stackChars);
    int32_t vecCount = vectorBatch->GetVectorCount();
    for (int i = 0; i < vecCount; ++i) {
        Vector *vector = vectorBatch->GetVector(i);
        vector->RecordStack(stack, VecOpType::JNI_ADD_INPUT);
    }
    env->ReleaseStringUTFChars(jstack, stackChars);
#endif
}

static void RecordOutputVectorsStack(VectorBatch &outputVecBatch, JNIEnv *env)
{
#ifdef DEBUG_VECTOR
    jstring jstack = (jstring)env->CallStaticObjectMethod(traceUtilCls, traceUtilStackMethodId);
    auto stackChars = env->GetStringUTFChars(jstack, JNI_FALSE);
    std::string stack(stackChars);
    for (int j = 0; j < outputVecBatch.GetVectorCount(); ++j) {
        Vector *vector = outputVecBatch.GetVector(j);
        vector->RecordStack(stack, VecOpType::JNI_GET_OUTPUT);
    }
    env->ReleaseStringUTFChars(jstack, stackChars);
#endif
}

static void LoadVecBatchAndOmniResults(JNIEnv *env)
{
    if (vecBatchCls == nullptr) {
        // the java adaptor maybe only use VecBatch or Vec like ColumnarBroadcastExchangeExec
        // it will load VecBatch class, and then will call System.load to load so
        // so load VecBatch class on demand to avoid deadlock
        vecBatchCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/vector/VecBatch");
        vecBatchInitMethodId = env->GetMethodID(vecBatchCls, "<init>", "(J[J[J[J[J[I[II)V");
        omniResultsCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/operator/OmniResults");

        rowCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/vector/Row");
        rowBatchCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/vector/RowBatch");
        rowResultsCls = CreateGlobalClassRef(env, "nova/hetu/omniruntime/operator/OmniRowResults");

        omniResultsInitMethodId =
            env->GetMethodID(omniResultsCls, "<init>", "(Lnova/hetu/omniruntime/vector/VecBatch;I)V");

        // Row(long dataAddr, int hashPos, int len)
        rowInitMethodId = env->GetMethodID(rowCls, "<init>", "(JI)V");

        // RowBatch(long nativeAddress, Row[] rows, int rowCount)
        rowBatchInitMethodId = env->GetMethodID(rowBatchCls, "<init>", "(J[Lnova/hetu/omniruntime/vector/Row;I)V");

        // OmniRowResults(RowBatch rowBatch, int status)
        rowResultsInitMethodId =
            env->GetMethodID(rowResultsCls, "<init>", "(Lnova/hetu/omniruntime/vector/RowBatch;I)V");
    }
}

static jobject Transform(JNIEnv *env, VectorBatch &result)
{
    int32_t vecCount = result.GetVectorCount();
    int64_t vecAddresses[vecCount];
    int32_t encodings[vecCount];
    int32_t dataTypeIds[vecCount];
    int64_t valueBufAddrs[vecCount];
    int64_t nullBufAddrs[vecCount];
    int64_t offsetsBufAddrs[vecCount];
    for (int32_t i = 0; i < vecCount; ++i) {
        BaseVector *vector = result.Get(i);
        vecAddresses[i] = reinterpret_cast<uintptr_t>(vector);
        dataTypeIds[i] = vector->GetTypeId();
        encodings[i] = vector->GetEncoding();
        // By default, all 3 buf arrays will have a value,
        // if not, it will be 0, which means a null pointer.
        valueBufAddrs[i] = reinterpret_cast<uintptr_t>(VectorHelper::UnsafeGetValues(vector));
        nullBufAddrs[i] = reinterpret_cast<uintptr_t>(omniruntime::vec::unsafe::UnsafeBaseVector::GetNulls(vector));
        offsetsBufAddrs[i] = reinterpret_cast<uintptr_t>(VectorHelper::UnsafeGetOffsetsAddr(vector));
    }

    // set vector addresses parameter to vector batch construct.
    jlongArray jVecAddresses = env->NewLongArray(vecCount);
    env->SetLongArrayRegion(jVecAddresses, 0, vecCount, vecAddresses);

    // set vector encoding
    jintArray jVecEncodingIds = env->NewIntArray(vecCount);
    env->SetIntArrayRegion(jVecEncodingIds, 0, vecCount, encodings);

    // set vector type ids parameter to vector batch construct.
    jintArray jDataTypeIds = env->NewIntArray(vecCount);
    env->SetIntArrayRegion(jDataTypeIds, 0, vecCount, dataTypeIds);

    // set vector value buf address
    jlongArray jVecValueBufAddrs = env->NewLongArray(vecCount);
    env->SetLongArrayRegion(jVecValueBufAddrs, 0, vecCount, valueBufAddrs);

    // set vec null buf address
    jlongArray jVecNullBufAddrs = env->NewLongArray(vecCount);
    env->SetLongArrayRegion(jVecNullBufAddrs, 0, vecCount, nullBufAddrs);

    // set vec offsets buf address
    jlongArray jVecOffsetsBufAddrs = env->NewLongArray(vecCount);
    env->SetLongArrayRegion(jVecOffsetsBufAddrs, 0, vecCount, offsetsBufAddrs);

    // create vector batch java object.
    jobject obj = env->NewObject(vecBatchCls, vecBatchInitMethodId, (jlong)((int64_t)(&result)), jVecAddresses,
        jVecValueBufAddrs, jVecNullBufAddrs, jVecOffsetsBufAddrs, jVecEncodingIds, jDataTypeIds, result.GetRowCount());
    return obj;
}

static jobject TransformFromRow(JNIEnv *env, RowBatch &result)
{
    int32_t rowCount = result.GetRowCount();
    jobjectArray resultArray = env->NewObjectArray(rowCount, rowCls, nullptr);
    for (int32_t i = 0; i < rowCount; ++i) {
        RowInfo *row = result.Get(i);
        jobject rowObject = env->NewObject(rowCls, rowInitMethodId, reinterpret_cast<long>(row->row), row->length);
        env->SetObjectArrayElement(resultArray, i, rowObject);
        env->DeleteLocalRef(rowObject);
    }

    // create vector batch java object.
    jobject obj = env->NewObject(rowBatchCls, rowBatchInitMethodId, (jlong)(&result), resultArray, rowCount);
    env->DeleteLocalRef(resultArray);
    return obj;
}

JNIEXPORT jint JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_addInputNative(JNIEnv *env, jobject jObj,
    jlong jOperatorAddress, jlong jVecBatchAddress)
{
    int32_t errNo = 0;
    auto *vecBatch = reinterpret_cast<VectorBatch *>(jVecBatchAddress);
    auto *nativeOperator = reinterpret_cast<op::Operator *>(jOperatorAddress);
    JNI_METHOD_START
    RecordInputVectorsStack(vecBatch, env);
    nativeOperator->SetInputVecBatch(vecBatch);
    errNo = nativeOperator->AddInput(vecBatch);
    JNI_METHOD_END_WITH_VECBATCH(errNo, nativeOperator->GetInputVecBatch())
    return errNo;
}

/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperator
 * Method:    getOutputNative
 * Signature: (J)[Lnova/hetu/omniruntime/operator/OMResult;
 */
JNIEXPORT jobject JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_getOutputNative(JNIEnv *env, jobject jObj,
    jlong jOperatorAddr)
{
    std::call_once(loadVecBatchClsFlag, LoadVecBatchAndOmniResults, env);
    if (vecBatchCls == nullptr || omniResultsCls == nullptr) {
        env->ThrowNew(omniRuntimeExceptionClass, "The class VecBatch or OmniResult has not load yet.");
        return nullptr;
    }

    auto *nativeOperator = reinterpret_cast<op::Operator *>(jOperatorAddr);
    VectorBatch *outputVecBatch = nullptr;
    JNI_METHOD_START
    nativeOperator->GetOutput(&outputVecBatch);
    JNI_METHOD_END_WITH_VECBATCH(nullptr, outputVecBatch)
    jobject result = nullptr;
    if (outputVecBatch) {
        RecordOutputVectorsStack(*outputVecBatch, env);
        result = Transform(env, *outputVecBatch);
    }
    return env->NewObject(omniResultsCls, omniResultsInitMethodId, result, nativeOperator->GetStatus());
}

/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperator
 * Method:    close
 * Signature: (J)[Lnova/hetu/omniruntime/operator/void;
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_closeNative(JNIEnv *env, jobject jObj,
    jlong jOperatorAddr)
{
    try {
        auto *nativeOperator = reinterpret_cast<op::Operator *>(jOperatorAddr);
        op::Operator::DeleteOperator(nativeOperator);
    } catch (const std::exception &e) {
        env->ThrowNew(omniRuntimeExceptionClass, e.what());
    }
}

/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperator
 * Method:    getSpilledBytesNative
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_getSpilledBytesNative(JNIEnv *env,
    jobject jObj, jlong jOperatorAddr)
{
    auto *nativeOperator = reinterpret_cast<op::Operator *>(jOperatorAddr);
    return static_cast<jlong>(nativeOperator->GetSpilledBytes());
}

JNIEXPORT jlongArray JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_getMetricsInfoNative(JNIEnv *env,
    jobject jObj, jlong jOperatorAddr)
{
    auto *nativeOperator = reinterpret_cast<op::Operator *>(jOperatorAddr);
    // get simpleMetrics info, used by all operators.
    static const uint64_t metricsLength = 200;
    static const uint64_t boundaryIndex = 100;
    jlongArray metricsInfoArray = env->NewLongArray(metricsLength);
    jlong* elementsSimple = env->GetLongArrayElements(metricsInfoArray, nullptr);
    elementsSimple[0] = static_cast<jlong>(nativeOperator->GetSpilledBytes());
    // get specialMetrics info, every operator is different.
    std::vector<uint64_t> specialMetricsInfoArray = nativeOperator->GetSpecialMetricsInfo();
    long specialMetricsLength = specialMetricsInfoArray.size();
    for (uint64_t i = 0; i < specialMetricsLength; i++) {
        elementsSimple[i + boundaryIndex] = specialMetricsInfoArray[i];
    }

    env->ReleaseLongArrayElements(metricsInfoArray, elementsSimple, 0);
    return metricsInfoArray;
}

JNIEXPORT jobject JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_alignSchemaNative(JNIEnv *env, jobject jObj,
    jlong jOperatorAddr, jlong jVecBatchAddr)
{
    auto nativeOperator = (op::Operator *)jOperatorAddr;
    auto nativeVeBatch = (vec::VectorBatch *)jVecBatchAddr;
    auto outputVecBatch = nativeOperator->AlignSchema(nativeVeBatch);
    jobject result = nullptr;
    if (outputVecBatch) {
        result = Transform(env, *outputVecBatch);
    }
    return result;
}

/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperator
 * Method:    getHashMapUniqueKeysNative
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_OmniOperator_getHashMapUniqueKeysNative(JNIEnv *env,
    jobject jObj, jlong jOperatorAddr)
{
    auto *nativeOperator = (op::Operator *)jOperatorAddr;
    return static_cast<jlong>(nativeOperator->GetHashMapUniqueKeys());
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_RowBatch_freeRowBatchNative(JNIEnv *env, jclass jcls,
    jlong jrowBatchAddress)
{
    auto *rowBatch = reinterpret_cast<RowBatch *>(jrowBatchAddress);
    delete rowBatch;
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_RowBatch_newRowBatchNative(JNIEnv *env, jclass jcls,
    jobjectArray rows, jint rowCount)
{
    jclass rowClass = env->FindClass("nova/hetu/omniruntime/vector/Row");

    jfieldID rowAddrId = env->GetFieldID(rowClass, "nativeRow", "J");
    jfieldID lengthId = env->GetFieldID(rowClass, "length", "I");

    auto *rowBatch = new RowBatch(rowCount);

    for (int i = 0; i < rowCount; ++i) {
        auto obj = env->GetObjectArrayElement(rows, i);
        auto rowAddr = env->GetLongField(obj, rowAddrId);
        auto length = env->GetIntField(obj, lengthId);
        rowBatch->SetRow(i, new RowInfo((uint8_t *)(rowAddr), length));
    }
    return reinterpret_cast<jlong>(rowBatch);
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_RowBatch_transFromVectorBatch(JNIEnv *env, jclass jcls,
    jlong vectorBatch)
{
    auto *vecBatch = reinterpret_cast<VectorBatch *>(vectorBatch);
    int32_t vecCount = vecBatch->GetVectorCount();
    std::vector<type::DataTypeId> typeIds;
    std::vector<Encoding> encodings;
    for (int i = 0; i < vecCount; i++) {
        BaseVector *vector = vecBatch->Get(i);
        typeIds.push_back(vector->GetTypeId());
        encodings.push_back(vector->GetEncoding());
    }

    auto rowBuffer = std::make_unique<RowBuffer>(typeIds, encodings, typeIds.size() - 1);

    auto rowBatch = std::make_unique<RowBatch>(vecBatch->GetRowCount(), typeIds);
    for (int32_t i = 0; i < vecBatch->GetRowCount(); ++i) {
        // 1.get value from vector batch
        rowBuffer->TransValueFromVectorBatch(vecBatch, i);

        // 2.generate one buffer of one row
        auto oneRowLen = rowBuffer->FillBuffer();

        // 3.set one row
        rowBatch->SetRow(i, new RowInfo(rowBuffer->TakeRowBuffer(), oneRowLen, true));
    }
    return reinterpret_cast<jlong>(rowBatch.release());
}

JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_vector_serialize_OmniRowDeserializer_newOmniRowDeserializer(
    JNIEnv *env, jclass jcls, jintArray typeArray, jlongArray vecArray)
{
    jboolean isCopy = false;
    auto *types = env->GetIntArrayElements(typeArray, &isCopy);
    auto *vecs = env->GetLongArrayElements(vecArray, &isCopy);
    auto len = env->GetArrayLength(typeArray);
    auto *parser = new RowParser((type::DataTypeId *)types, vecs, len);
    env->ReleaseLongArrayElements(vecArray, vecs, JNI_ABORT);
    env->ReleaseIntArrayElements(typeArray, types, JNI_ABORT);
    return reinterpret_cast<intptr_t>(parser);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_serialize_OmniRowDeserializer_freeOmniRowDeserializer(
    JNIEnv *env, jclass jcls, jlong parserAddr)
{
    auto *rowParser = reinterpret_cast<RowParser *>(parserAddr);
    delete rowParser;
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_serialize_OmniRowDeserializer_parseOneRow(JNIEnv *env,
    jclass jcls, jlong parserAddr, jbyteArray bytes, jint rowIndex)
{
    jboolean isCopy = false;
    auto *row = env->GetByteArrayElements(bytes, &isCopy);
    auto *parser = reinterpret_cast<RowParser *>(parserAddr);
    parser->ParseOnRow(reinterpret_cast<uint8_t *>(row), rowIndex);
    env->ReleaseByteArrayElements(bytes, row, 0);
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_serialize_OmniRowDeserializer_parseOneRowByAddr(JNIEnv *env,
    jclass jcls, jlong parserAddr, jlong rowAddr, jint rowIndex)
{
    auto *parser = reinterpret_cast<RowParser *>(parserAddr);
    auto *row = reinterpret_cast<uint8_t *>(rowAddr);
    if (row != nullptr) {
        parser->ParseOnRow(row, rowIndex);
    }
}

JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_vector_serialize_OmniRowDeserializer_parseAllRow(JNIEnv *env,
    jclass jcls, jlong parserAddr, jlong rowBatchAddr)
{
    auto *parser = reinterpret_cast<RowParser *>(parserAddr);
    auto *rowBatch = reinterpret_cast<RowBatch *>(rowBatchAddr);

    for (int i = 0; i < rowBatch->GetRowCount(); ++i) {
        parser->ParseOnRow(rowBatch->Get(i)->row, i);
    }
}