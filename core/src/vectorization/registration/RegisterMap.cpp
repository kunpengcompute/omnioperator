/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: visitor class for expressions
 */

#include "vectorization/functions/SizeFunction.h"
#include "vectorization/functions/SubscriptUtil.h"
#include "RegistrationHelpers.h"
#include "vectorization/functions/MapFromArraysFunction.h"
#include "vectorization/functions/MapKeysAndValues.h"
#include "vectorization/functions/MapEntries.h"
#include "vectorization/functions/StrToMapFunction.h"
#include "vectorization/functions/MapFunction.h"
#include "vectorization/functions/MapConcatFunction.h"
#include "codegen/func_signature.h"

namespace omniruntime::vectorization {
using namespace omniruntime::codegen;

void RegisterMapFunctions(const std::string &prefix)
{
    VectorFunction::RegisterVectorFunction("size", {OMNI_MAP, OMNI_BOOLEAN}, OMNI_INT,
        std::make_shared<SizeFunction>());

    std::vector<DataTypeId> supportValueTypes = {OMNI_BOOLEAN, OMNI_BYTE, OMNI_SHORT, OMNI_INT, OMNI_LONG, OMNI_FLOAT,
        OMNI_DOUBLE, OMNI_VARCHAR,  OMNI_DATE32, OMNI_DATE64, OMNI_TIME32, OMNI_TIME64, OMNI_TIMESTAMP, OMNI_DECIMAL64, OMNI_DECIMAL128};

    std::vector<DataTypeId> elementAtKeyTypes = {OMNI_BOOLEAN, OMNI_BYTE, OMNI_SHORT, OMNI_INT, OMNI_LONG,
        OMNI_FLOAT, OMNI_DOUBLE, OMNI_VARCHAR};
    for (auto keyType : elementAtKeyTypes) {
        for (auto &valueType : supportValueTypes) {
            auto impl = std::make_shared<SubscriptImpl>();
            VectorFunction::RegisterVectorFunction("element_at", {OMNI_MAP, keyType}, valueType, impl);
            VectorFunction::RegisterVectorFunction("try_element_at", {OMNI_MAP, keyType}, valueType, impl);
        }
    }

    VectorFunction::RegisterVectorFunction("map_from_arrays", {OMNI_ARRAY, OMNI_ARRAY}, OMNI_MAP,
        std::make_shared<MapFromArraysFunction>());
    VectorFunction::RegisterVectorFunction("map_keys", {OMNI_MAP}, OMNI_ARRAY,
        std::make_shared<MapKeysAndValuesFunction<true>>());
    VectorFunction::RegisterVectorFunction("map_values", {OMNI_MAP}, OMNI_ARRAY,
        std::make_shared<MapKeysAndValuesFunction<false>>());
    VectorFunction::RegisterVectorFunction("map_entries", {OMNI_MAP}, OMNI_ARRAY,
        std::make_shared<MapEntriesFunction>());
    VectorFunction::RegisterVectorFunction("str_to_map", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_MAP,
        std::make_shared<StrToMapFunction>());

    auto mapFactory = [](const std::string&, const std::vector<DataTypeId>&,
                         const config::QueryConfig&) -> std::shared_ptr<VectorFunction> {
        return std::make_shared<MapFunction>();
    };

    std::vector<DataTypeId> supportKeyTypes = {OMNI_BOOLEAN, OMNI_BYTE, OMNI_SHORT, OMNI_INT, OMNI_LONG,
        OMNI_FLOAT, OMNI_DOUBLE, OMNI_TIMESTAMP, OMNI_VARBINARY, OMNI_ARRAY, OMNI_MAP, OMNI_ROW};

    std::vector<DataTypeId> supportMapValueTypes = {OMNI_BOOLEAN, OMNI_BYTE, OMNI_SHORT, OMNI_INT, OMNI_LONG,
        OMNI_FLOAT, OMNI_DOUBLE, OMNI_TIMESTAMP, OMNI_VARBINARY, OMNI_ARRAY, OMNI_MAP, OMNI_ROW};

    std::vector<std::shared_ptr<FunctionSignature>> mapSigs;
    for (auto &keyType : supportKeyTypes) {
        for (auto &valType : supportMapValueTypes) {
            mapSigs.push_back(std::make_shared<FunctionSignature>(
                prefix + "map", std::vector<DataTypeId>{keyType, valType}, OMNI_MAP));
        }
    }
    VectorFunction::RegisterVectorFunctionFactory(mapSigs, mapFactory);

    VectorFunction::RegisterVectorFunctionFactory(MapConcatSignatures(), makeMapConcat);
}
}
