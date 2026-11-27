/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: Type Operator Factory Header
 */
#ifndef JNI_OPERATOR_FACTORY_H
#define JNI_OPERATOR_FACTORY_H

#include <jni.h>
#include "expression/parserhelper.h"
#include "expression/jsonparser/jsonparser.h"
#include "codegen/expr_evaluator.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperatorFactory
 * Method:    createOperatorNative
 * Signature: (JJ)J
 *
 * The trailing jlong nmmHandle is the per-task OmniTaskMemoryPool* address.
 * The .cpp had been updated to the 4-arg signature but the .h was left at the
 * 2-arg form, breaking the extern "C" decl/def linkage match: the .cpp symbol
 * got C++-name-mangled instead of the plain Java_... form, causing
 * UnsatisfiedLinkError at runtime. Keep all four declarations (Java native,
 * .h extern "C" decl, .cpp def, bundle jar) byte-identical.
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_OmniOperatorFactory_createOperatorNative(JNIEnv *env,
    jobject jObj, jlong jNativeFactoryObj, jlong nmmHandle);

/*
 * Class:     nova_hetu_omniruntime_operator_OmniOperatorFactory
 * Method:    closeNativeOperatorFactory
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_nova_hetu_omniruntime_operator_OmniOperatorFactory_closeNativeOperatorFactory(JNIEnv *env,
    jclass jclz, jlong jNativeOperatorFactory);

/*
 * Class:     nova_hetu_omniruntime_operator_sort_OmniSortOperatorFactory
 * Method:    createSortOperatorFactory
 * Signature: (Ljava/lang/String;[I[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_sort_OmniSortOperatorFactory_createSortOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jintArray jOutputCols, jobjectArray jSortCols,
    jintArray jAscendings, jintArray jNullFirsts, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_aggregator_OmniHashAggregationOperatorFactory
 * Method:    createHashAggregationOperatorFactory
 * Signature: ([Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;[I[ILjava/lang/String;ZZ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_aggregator_OmniHashAggregationOperatorFactory_createHashAggregationOperatorFactory(
    JNIEnv *env, jclass jObj, jobjectArray jGroupByChannel, jstring jGroupByType, jobjectArray jAggChannel,
    jstring jAggType, jintArray jAggFuncType, jintArray jMaskCols, jstring jOutPutTye, jboolean inputRaw,
    jboolean outputPartial, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_aggregator_OmniAggregationOperatorFactory
 * Method:    createAggregationOperatorFactory
 * Signature: (Ljava/lang/String;[I[I[ILjava/lang/String;ZZ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_aggregator_OmniAggregationOperatorFactory_createAggregationOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jintArray jAggFuncTypes, jintArray jAggInputCols,
    jintArray jMaskCols, jstring jAggOutputTypes, jboolean inputRaw, jboolean outputPartial);

/*
 * Class:     nova_hetu_omniruntime_operator_filter_OmniFilterAndProjectOperatorFactory
 * Method:    createFilterAndProjectOperatorFactory
 * Signature: (Ljava/lang/String;ILjava/lang/String;[Ljava/lang/Object;IIZ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_filter_OmniFilterAndProjectOperatorFactory_createFilterAndProjectOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jInputTypes, jint jInputLength, jstring jExpression, jobjectArray jProjections,
    jint jProjectLength, jint jParseFormat, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_project_OmniProjectOperatorFactory
 * Method:    createProjectOperatorFactory
 * Signature: (Ljava/lang/String;I[Ljava/lang/Object;IIZ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_project_OmniProjectOperatorFactory_createProjectOperatorFactory(JNIEnv *env,
    jclass jobj, jstring jInputTypes, jint jInputLength, jobjectArray jExprs, jint jExprsLength, jint jParseFormat,
    jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_window_OmniWindowOperatorFactory
 * Method:    createWindowOperatorFactory
 * Signature: (Ljava/lang/String;[I[I[I[I[I[I[III[ILjava/lang/String;[I[I[I[I[I)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_window_OmniWindowOperatorFactory_createWindowOperatorFactory(JNIEnv *env,
    jobject jObj, jstring jSourceTypes, jintArray jOutputChannels, jintArray jWindowFunction,
    jintArray jPartitionChannels, jintArray JPreGroupedChannels, jintArray jSortChannels, jintArray jSortOrder,
    jintArray jSortNullFirsts, jint preSortedChannelPrefix, jint expectedPositions, jintArray jArgumentChannels,
    jstring jWindowFunctionReturnType, jintArray jWindowFrameTypes, jintArray jWindowFrameStartTypes,
    jintArray jWindowFrameStartChannels, jintArray jWindowFrameEndTypes, jintArray jWindowFrameEndChannels,
    jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_topn_OmniTopNOperatorFactory
 * Method:    createTopNOperatorFactory
 * Signature: (Ljava/lang/String;I[Ljava/lang/String;[I[I)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_topn_OmniTopNOperatorFactory_createTopNOperatorFactory(JNIEnv *env, jclass jObj,
    jstring jSourceTypes, jint jN, jint jOffset, jobjectArray jSortCols, jintArray jSortAsc, jintArray jSortNullFirsts);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniHashBuilderOperatorFactory
 * Method:    createHashBuilderOperatorFactory
 * Signature: (Ljava/lang/String;[IILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniHashBuilderOperatorFactory_createHashBuilderOperatorFactory(JNIEnv *env,
    jclass jObj, jint jJoinType, jstring jBuildTypes, jintArray jBuildHashCols, jint jOperatorCount,
    jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniLookupJoinOperatorFactory
 * Method:    createLookupJoinOperatorFactory
 * Signature: (Ljava/lang/String;[I[I[ILjava/lang/String;IJLjava/lang/String;Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniLookupJoinOperatorFactory_createLookupJoinOperatorFactory(JNIEnv *env,
    jclass jObj, jstring jProbeTypes, jintArray jProbeOutputCols, jintArray jProbeHashCols, jintArray jBuildOutputCols,
    jstring jBuildOutputTypes, jlong jHashBuilderOperatorFactory,
    jstring jFilter, jboolean isShuffleExchangeBuildPlan, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_partitionedoutput_OmniPartitionedOutPutOperatorFactory
 * Method:    createPartitionedOutputOperatorFactory
 * Signature: (Ljava/lang/String;ZI[II[IZLjava/lang/String;[I)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_partitionedoutput_OmniPartitionedOutPutOperatorFactory_createPartitionedOutputOperatorFactory(
    JNIEnv *env, jobject jObj, jstring jSourceTypes, jboolean jReplicatesAnyRow, jint jNullChannel,
    jintArray jPartitionChannels, jint jPartitionCount, jintArray jBucketToPartition, jboolean isHashPrecomputed,
    jstring jHashChannelTypes, jintArray jHashChannels);

/*
 * Class:     nova_hetu_omniruntime_operator_union_OmniUnionOperatorFactory
 * Method:    createUnionOperatorFactory
 * Signature: (Ljava/lang/String;Z)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_union_OmniUnionOperatorFactory_createUnionOperatorFactory(
    JNIEnv *env, jobject jObj, jstring jSourceTypes, jboolean jDistinct);

/*
 * Class:     nova_hetu_omniruntime_operator_sort_OmniSortWithExprOperatorFactory
 * Method:    createSortWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[I[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_sort_OmniSortWithExprOperatorFactory_createSortWithExprOperatorFactory(JNIEnv *env,
    jclass jObj, jstring jSourceTypes, jintArray jOutputCols, jobjectArray jSortKeys, jintArray jAscendings,
    jintArray jNullFirsts, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniHashBuilderWithExprOperatorFactory
 * Method:    createHashBuilderWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[Ljava/lang/String;ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniHashBuilderWithExprOperatorFactory_createHashBuilderWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jint jJoinType, jint jBuildSide, jstring jBuildTypes, jobjectArray jBuildHashKeys,
    jint jHashTableCount, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniLookupJoinWithExprOperatorFactory
 * Method:    createLookupJoinWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[I[Ljava/lang/String;[ILjava/lang/String;IJLjava/lang/String;Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniLookupJoinWithExprOperatorFactory_createLookupJoinWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jProbeTypes, jintArray jProbeOutputCols, jobjectArray jProbeHashKeys,
    jintArray jBuildOutputCols, jstring jBuildOutputTypes, jlong jHashBuilderOperatorFactory,
    jstring jFilter, jboolean isShuffleExchangeBuildPlan, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniLookupOuterJoinWithExprOperatorFactory
 * Method:    createLookupOuterJoinWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[I[Ljava/lang/String;[ILjava/lang/String;J)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniLookupOuterJoinWithExprOperatorFactory_createLookupOuterJoinWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jProbeTypes, jintArray jProbeOutputCols, jobjectArray jProbeHashKeys,
    jintArray jBuildOutputCols, jstring jBuildOutputTypes, jlong jHashBuilderOperatorFactory);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniLookupOuterJoinOperatorFactory
 * Method:    createLookupOuterJoinOperatorFactory
 * Signature: (Ljava/lang/String;[I[ILjava/lang/String;J)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniLookupOuterJoinOperatorFactory_createLookupOuterJoinOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jProbeTypes, jintArray jProbeOutputCols, jintArray jBuildOutputCols,
    jstring jBuildOutputTypes, jlong jHashBuilderOperatorFactory);

/*
 * Class:     nova_hetu_omniruntime_operator_window_OmniWindowWithExprOperatorFactory
 * Method:    createWindowWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[I[I[I[I[I[I[III[Ljava/lang/String;Ljava/lang/String;[I[I[I[I[I)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_window_OmniWindowWithExprOperatorFactory_createWindowWithExprOperatorFactory(
    JNIEnv *env, jobject jObj, jstring jSourceTypes, jintArray jOutputChannels, jintArray jWindowFunction,
    jintArray jPartitionChannels, jintArray JPreGroupedChannels, jintArray jSortChannels, jintArray jSortOrder,
    jintArray jSortNullFirsts, jint preSortedChannelPrefix, jint expectedPositions, jobjectArray jArgumentKeys,
    jstring jWindowFunctionReturnType, jintArray jWindowFrameTypes, jintArray jWindowFrameStartTypes,
    jintArray jWindowFrameStartChannels, jintArray jWindowFrameEndTypes, jintArray jWindowFrameEndChannels,
    jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_aggregator_OmniHashAggregationWithExprOperatorFactory
 * Method:    createHashAggregationWithExprOperatorFactory
 * Signature: ([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;[I[ILjava/lang/String;ZZ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_aggregator_OmniHashAggregationWithExprOperatorFactory_createHashAggregationWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jobjectArray jGroupByChannel, jobjectArray jAggChannels, jobjectArray jAggChannelsFilter,
    jstring jSourceType, jintArray jAggFuncType, jintArray jMaskCols, jobjectArray jOutputType,
    jbooleanArray jInputRaws, jbooleanArray jOutputPartials, jstring jOperatorConfig);

JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_aggregator_OmniAggregationWithExprOperatorFactory_createAggregationWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jobjectArray jGroupByChannel, jobjectArray jAggChannels, jobjectArray jAggChannelsFilter,
    jstring jSourceType, jintArray jAggFuncType, jintArray jMaskCols, jobjectArray jOutputType,
    jbooleanArray jInputRaws, jbooleanArray jOutputPartials, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_topn_OmniTopNWithExprOperatorFactory
 * Method:    createTopNWithExprOperatorFactory
 * Signature: (Ljava/lang/String;I[Ljava/lang/String;[I[I)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_topn_OmniTopNWithExprOperatorFactory_createTopNWithExprOperatorFactory(JNIEnv *env,
    jclass jObj, jstring jSourceTypes, jint jN, jint jOffset, jobjectArray jSortKeys, jintArray jSortAsc,
    jintArray jSortNullFirsts, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_limit_OmniLimitOperatorFactory
 * Method:    createLimitOperatorFactory
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_limit_OmniLimitOperatorFactory_createLimitOperatorFactory(
    JNIEnv *env, jclass jObj, jint jLimit, jint jOffset);

/*
 * Class:     nova_hetu_omniruntime_operator_limit_OmniDistinctLimitOperatorFactory
 * Method:    createDistinctLimitOperatorFactory
 * Signature: (Ljava/lang/String;[IIJ)J
 * Note: out put seq as below:
 * 1. distinct cols
 * 2. normal cols
 * 3. hash col(jHashChannel)
 * Note: put jHashChannel to -1 if no precomputed hash value for distinct cols
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_limit_OmniDistinctLimitOperatorFactory_createDistinctLimitOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jSoureTypes, jintArray jDistinctChannel, jint jHashChannel, jlong jLimit);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniSmjStreamedTableWithExprOperatorFactory
 * Method:    createSmjStreamedTableWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[Ljava/lang/String;[IILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniSmjStreamedTableWithExprOperatorFactory_createSmjStreamedTableWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jobjectArray jEqualKeyExprs, jintArray jOutputChannels,
    jint jJoinType, jstring jFilter, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniSmjBufferedTableWithExprOperatorFactory
 * Method:    createSmjBufferedTableWithExprOperatorFactory
 * Signature: (Ljava/lang/String;[Ljava/lang/String;[IJ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniSmjBufferedTableWithExprOperatorFactory_createSmjBufferedTableWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jobjectArray jEqualKeyExprs, jintArray jOutputChannels,
    jlong jSmjStreamedTableWithExprOperatorFactory, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniSmjStreamedTableWithExprOperatorFactoryV3
 * Method:    createSmjStreamedTableWithExprOperatorFactoryV3
 * Signature: (Ljava/lang/String;[Ljava/lang/String;[IILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniSmjStreamedTableWithExprOperatorFactoryV3_createSmjStreamedTableWithExprOperatorFactoryV3(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jobjectArray jEqualKeyExprs, jintArray jOutputChannels,
    jint jJoinType, jstring jFilter, jstring jOperatorConfig);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniSmjBufferedTableWithExprOperatorFactoryV3
 * Method:    createSmjBufferedTableWithExprOperatorFactoryV3
 * Signature: (Ljava/lang/String;[Ljava/lang/String;[IJ)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniSmjBufferedTableWithExprOperatorFactoryV3_createSmjBufferedTableWithExprOperatorFactoryV3(
    JNIEnv *env, jclass jObj, jstring jSourceTypes, jobjectArray jEqualKeyExprs, jintArray jOutputChannels,
    jlong jSmjStreamedTableWithExprOperatorFactoryV3, jstring jOperatorConfig);


/*
 * Class:     nova_hetu_omniruntime_operator_OmniExprVerify
 * Method:    exprVerify
 * Signature: (Ljava/lang/String;ILjava/lang/String;[Ljava/lang/Object;II)J
 */
JNIEXPORT jlong JNICALL Java_nova_hetu_omniruntime_operator_OmniExprVerify_exprVerify(JNIEnv *env, jclass jObj,
    jstring jInputTypes, jint jInputLength, jstring jExpression, jobjectArray jProjections, jint jProjectLength,
    jint jParseFormat);

/*
 * Class:     nova_hetu_omniruntime_operator_filter_OmniBloomFilterOperatorFactory
 * Method:    createBloomFilterOperatorFactory
 * Signature: ([J[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_filter_OmniBloomFilterOperatorFactory_createBloomFilterOperatorFactory(JNIEnv *env,
    jclass jObj, jint jInputVersion);

/*
 * Class:     nova_hetu_omniruntime_operator_topnsort_OmniTopNSortWithExprOperatorFactory
 * Method:    createTopNSortWithExprOperatorFactory
 * Signature: (Ljava/lang/String;IZ[Ljava/lang/String;[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_topnsort_OmniTopNSortWithExprOperatorFactory_createTopNSortWithExprOperatorFactory(
    JNIEnv *, jclass, jstring, jint, jboolean, jobjectArray, jobjectArray, jintArray, jintArray, jstring);

/*
 * Class:     nova_hetu_omniruntime_operator_window_OmniWindowGroupLimitWithExprOperatorFactory
 * Method:    createWindowGroupLimitWithExprOperatorFactory
 * Signature: (Ljava/lang/String;IZ[Ljava/lang/String;[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_window_OmniWindowGroupLimitWithExprOperatorFactory_createWindowGroupLimitWithExprOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jInputTypes, jint jN, jstring jFuncName, jobjectArray jPartitionKeys,
    jobjectArray jSortKeys, jintArray jSortAsc, jintArray jSortNullFirsts, jstring jOperatorConfig);


/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniNestedLoopJoinBuildOperatorFactory
 * Method:    createNestedLoopJoinBuildOperatorFactory
 * Signature: (Ljava/lang/String;IZ[Ljava/lang/String;[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniNestedLoopJoinBuildOperatorFactory_createNestedLoopJoinBuildOperatorFactory(
    JNIEnv *env, jclass jObj, jstring jBuildTypes, jintArray jBuildOutputCols);

/*
 * Class:     nova_hetu_omniruntime_operator_join_OmniNestedLoopJoinLookupOperatorFactory
 * Method:    createNestedLoopJoinLookupOperatorFactory
 * Signature: (Ljava/lang/String;IZ[Ljava/lang/String;[Ljava/lang/String;[I[ILjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_nova_hetu_omniruntime_operator_join_OmniNestedLoopJoinLookupOperatorFactory_createNestedLoopJoinLookupOperatorFactory(
    JNIEnv *env, jclass jObj, jint jJoinType, jstring jProbeTypes, jintArray jProbeOutputCols, jstring jFilter,
    jlong jNestedLoopJoinBuildOperatorFactory, jstring jOperatorConfig);

#ifdef __cplusplus
}
#endif
#endif