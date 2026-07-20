/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: JSON function registration
 */

 #include <string>
 #include "../functions/ToJson.h"
 #include "../functions/JsonObjectKeys.h"
 #include "../functions/JsonArrayLength.h"
 #include "../functions/FromJson.h"
 #include "../functions/GetJsonObject.h"
 #include "../functions/IsJson.h"
 #include "RegistrationHelpers.h"
 
 namespace omniruntime::vectorization {
 void RegisterJsonFunctions(const std::string &prefix)
 {
     auto toJsonFunc = std::make_shared<ToJsonFunction>();
     VectorFunction::RegisterVectorFunction(prefix + "to_json", {OMNI_ROW}, OMNI_VARCHAR, toJsonFunc);
     VectorFunction::RegisterVectorFunction(prefix + "to_json", {OMNI_ARRAY}, OMNI_VARCHAR, toJsonFunc);
     VectorFunction::RegisterVectorFunction(prefix + "to_json", {OMNI_MAP}, OMNI_VARCHAR, toJsonFunc);
 
     VectorFunction::RegisterVectorFunction("json_object_keys", {OMNI_VARCHAR}, OMNI_ARRAY,
         std::make_shared<JsonObjectKeysFunction>());
 
     VectorFunction::RegisterVectorFunction("json_array_length", {OMNI_VARCHAR}, OMNI_INT,
         std::make_shared<JsonArrayLengthFunction>());
 
     VectorFunction::RegisterVectorFunction("from_json", {OMNI_VARCHAR}, OMNI_ROW,
         std::make_shared<FromJsonFunction>());
 
     // Register get_json_object function (lowercase version for standard SQL)
     VectorFunction::RegisterVectorFunction("get_json_object", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
         std::make_shared<GetJsonObjectFunction>());
 
     // Also register CamelCase version for Gluten compatibility
     VectorFunction::RegisterVectorFunction("GetJsonObject", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
         std::make_shared<GetJsonObjectFunction>());

     // IS JSON [ { VALUE | SCALAR | ARRAY | OBJECT } ] -> boolean
     // Flink JSON Function: determine whether a string is valid JSON, optionally
     // constraining the top-level value type. Calcite models these as 4 separate
     // postfix operators (IS JSON VALUE/SCALAR/ARRAY/OBJECT), so 4 native function
     // names are registered. NULL input -> FALSE (not NULL); implemented as Path B
     // VectorFunction so NULL rows yield an explicit FALSE (BOOLEAN NOT NULL).
     VectorFunction::RegisterVectorFunction(prefix + "is_json_value", {OMNI_VARCHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::VALUE));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_value", {OMNI_CHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::VALUE));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_scalar", {OMNI_VARCHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::SCALAR));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_scalar", {OMNI_CHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::SCALAR));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_array", {OMNI_VARCHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::ARRAY));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_array", {OMNI_CHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::ARRAY));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_object", {OMNI_VARCHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::OBJECT));
     VectorFunction::RegisterVectorFunction(prefix + "is_json_object", {OMNI_CHAR}, OMNI_BOOLEAN,
         std::make_shared<IsJsonFunction>(JsonType::OBJECT));
 }
 }
 