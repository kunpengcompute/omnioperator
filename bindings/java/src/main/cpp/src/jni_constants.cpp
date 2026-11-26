/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: JNI Constants
 */

#include "jni_constants.h"

JNIEXPORT jstring JNICALL Java_nova_hetu_omniruntime_OmniLibs_getVersion(JNIEnv *env, jclass ignore)
{
    return (*env).NewStringUTF("Product Name: Kunpeng BoostKit\nProduct Version: 26.1.0\nComponent Name: "
                               "BoostKit-omniop\nComponent Version: 2.2.0");
}
