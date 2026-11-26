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

package nova.hetu.omniruntime.utils;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_NATIVE_ERROR;

/**
 * The type Omni runtime exception.
 *
 * @since 2021-06-30
 */
public class OmniRuntimeException extends RuntimeException {
    private static final long serialVersionUID = -4352889723335051173L;

    private final OmniErrorType errorType;

    /**
     * this method for jni method call
     *
     * @param msg error message
     */
    public OmniRuntimeException(String msg) {
        super(msg);
        this.errorType = OMNI_NATIVE_ERROR;
    }

    /**
     * Instantiates a new Omni runtime exception.
     *
     * @param errorType the error type
     * @param msg the msg
     */
    public OmniRuntimeException(OmniErrorType errorType, String msg) {
        super(msg);
        this.errorType = errorType;
    }

    /**
     * Instantiates a new Omni runtime exception.
     *
     * @param errorType the error type
     * @param string the string
     * @param throwable the throwable
     */
    public OmniRuntimeException(OmniErrorType errorType, String string, Throwable throwable) {
        super(string, throwable);
        this.errorType = errorType;
    }

    /**
     * Instantiates a new Omni runtime exception.
     *
     * @param errorType the error type
     * @param throwable the throwable
     */
    public OmniRuntimeException(OmniErrorType errorType, Throwable throwable) {
        super(throwable);
        this.errorType = errorType;
    }
}
