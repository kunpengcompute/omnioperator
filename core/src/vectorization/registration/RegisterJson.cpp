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
 #include "../functions/JsonExists.h"
 #include "../functions/JsonString.h"
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

     // JSON_EXISTS(jsonValue, path [, onError]) -> boolean. ON ERROR behavior (TRUE/FALSE/
     // UNKNOWN/ERROR, default FALSE) is passed as an optional 3rd VARCHAR literal argument
     // — Flink models ON ERROR as an operand, not operator identity. NULL input -> NULL
     // output (Flink argsNullable=false short-circuit). Path B VectorFunction. path may
     // carry a strict/lax prefix (default STRICT); LAX suppresses errors, STRICT routes
     // missing/invalid to ON ERROR.
     // The path and onError operands are always VARCHAR (CHAR literals are normalized to
     // VARCHAR on the OmniAdaptor side); the jsonValue operand may be a VARCHAR or CHAR
     // column, so both are registered. GetStringValue handles CHAR/VARCHAR uniformly.
     auto jsonExistsFunc = std::make_shared<JsonExistsFunction>();
     // 2-arg form (ON ERROR omitted -> default FALSE)
     VectorFunction::RegisterVectorFunction(prefix + "json_exists", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN,
         jsonExistsFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_exists", {OMNI_CHAR, OMNI_VARCHAR}, OMNI_BOOLEAN,
         jsonExistsFunc);
     // 3-arg form (explicit ON ERROR literal)
     VectorFunction::RegisterVectorFunction(prefix + "json_exists",
         {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN, jsonExistsFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_exists",
         {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN, jsonExistsFunc);

     // json_string: Flink SQL JSON_STRING(value). Serializes a scalar or composite value
     // into a JSON string. NULL input -> NULL output. Same instance handles all registered
     // input types (dispatch happens inside Apply via the vector's type id).
     // Out of scope this version: DECIMAL / VARBINARY / DATE32 / TIMESTAMP.
     auto jsonStringFunc = std::make_shared<JsonStringFunction>();
     // Scalar types
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_BOOLEAN}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_BYTE}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_SHORT}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_INT}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_LONG}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_FLOAT}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_DOUBLE}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_VARCHAR}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_CHAR}, OMNI_VARCHAR, jsonStringFunc);
     // Composite types
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_ARRAY}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_MAP}, OMNI_VARCHAR, jsonStringFunc);
     VectorFunction::RegisterVectorFunction(prefix + "json_string", {OMNI_ROW}, OMNI_VARCHAR, jsonStringFunc);
 }
 }
 