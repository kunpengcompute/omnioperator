/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.utils;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_INNER_ERROR;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.util.ArrayList;
import java.util.List;

/**
 * json serialize/deserialize util.
 *
 * @since 2022-9-22
 */
public class JsonUtils {
    /**
     * Object mapper singleton.
     */
    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    /**
     * transform String array to json
     *
     * @param array String[]
     * @return String
     * @throws OmniRuntimeException omniRuntimeException
     */
    public static String jsonStringArray(String[] array) {
        try {
            return OBJECT_MAPPER.writeValueAsString(array);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Serialization failed.", e);
        }
    }

    /**
     * transform each row in String[][] to json
     *
     * @param array String[][]
     * @return String[]
     */
    public static String[] jsonStringArray(String[][] array) {
        List<String> stringList = new ArrayList<>();
        for (String[] arr : array) {
            stringList.add(jsonStringArray(arr));
        }
        return stringList.toArray(new String[stringList.size()]);
    }

    /**
     * Deserialize a single json.
     *
     * @param json the string need to be deserialization
     * @return String[]
     * @throws OmniRuntimeException omniRuntimeException
     */
    public static String[] deserializeJson(String json) {
        try {
            return OBJECT_MAPPER.readerFor(String[].class).readValue(json);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Deserialization failed.", e);
        }
    }

    /**
     * Deserialize a single json.
     *
     * @param jsons the string[] need to be deserialization
     * @return String[][]
     */
    public static String[][] deserializeJson(String[] jsons) {
        String[][] strings = new String[jsons.length][];
        for (int i = 0; i < jsons.length; i++) {
            strings[i] = deserializeJson(jsons[i]);
        }
        return strings;
    }
}
