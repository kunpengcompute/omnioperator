/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Registration for conditional functions
 */

#include <string>
#include "../functions/Coalesce.h"
#include "../functions/If.h"
#include "../functions/IsAlpha.h"
#include "../functions/IsDecimal.h"
#include "../functions/Nanvl.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterConditionalFunctions(const std::string &prefix)
{
    // Register if for all supported types
    auto ifFunction = std::make_shared<IfFunction>();

    // Boolean type
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_BOOLEAN, OMNI_BOOLEAN}, OMNI_BOOLEAN, ifFunction);

    // Integer types
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_BYTE, OMNI_BYTE}, OMNI_BYTE, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_SHORT, OMNI_SHORT}, OMNI_SHORT, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_INT, OMNI_INT}, OMNI_INT, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_LONG, OMNI_LONG}, OMNI_LONG, ifFunction);

    // Floating point types
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_FLOAT, OMNI_FLOAT}, OMNI_FLOAT, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE, ifFunction);

    // String types
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, ifFunction);

    // Binary type (VARBINARY uses same storage as VARCHAR: LargeStringContainer<std::string_view>)
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_VARBINARY, OMNI_VARBINARY}, OMNI_VARBINARY, ifFunction);

    // Date types
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_DATE32, OMNI_DATE32}, OMNI_DATE32, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_DATE64, OMNI_DATE64}, OMNI_DATE64, ifFunction);

    // Timestamp type (OMNI_TIMESTAMP is equivalent to OMNI_LONG at runtime)
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_TIMESTAMP, OMNI_TIMESTAMP}, OMNI_TIMESTAMP, ifFunction);

    // Decimal types
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_DECIMAL64, OMNI_DECIMAL64}, OMNI_DECIMAL64, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_DECIMAL128, OMNI_DECIMAL128}, OMNI_DECIMAL128, ifFunction);

    // Complex types: ARRAY, MAP, ROW (STRUCT)
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_ARRAY, OMNI_ARRAY}, OMNI_ARRAY, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_MAP, OMNI_MAP}, OMNI_MAP, ifFunction);
    VectorFunction::RegisterVectorFunction("if", {OMNI_BOOLEAN, OMNI_ROW, OMNI_ROW}, OMNI_ROW, ifFunction);

    // Register coalesce - 2 arguments (primitive and string types)
    auto coalesceFunction = std::make_shared<CoalesceFunction>();
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_BOOLEAN, OMNI_BOOLEAN}, OMNI_BOOLEAN, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_BYTE, OMNI_BYTE}, OMNI_BYTE, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_SHORT, OMNI_SHORT}, OMNI_SHORT, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_INT, OMNI_INT}, OMNI_INT, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_LONG, OMNI_LONG}, OMNI_LONG, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_FLOAT, OMNI_FLOAT}, OMNI_FLOAT, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_DATE32, OMNI_DATE32}, OMNI_DATE32, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_DATE64, OMNI_DATE64}, OMNI_DATE64, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_TIMESTAMP, OMNI_TIMESTAMP}, OMNI_TIMESTAMP, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_DECIMAL64, OMNI_DECIMAL64}, OMNI_DECIMAL64, coalesceFunction);

    // Register coalesce - BINARY type (VARBINARY)
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_VARBINARY, OMNI_VARBINARY}, OMNI_VARBINARY, coalesceFunction);

    // Register coalesce - complex types (ARRAY, MAP, ROW/STRUCT)
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_ARRAY, OMNI_ARRAY}, OMNI_ARRAY, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_MAP, OMNI_MAP}, OMNI_MAP, coalesceFunction);
    VectorFunction::RegisterVectorFunction("coalesce", {OMNI_ROW, OMNI_ROW}, OMNI_ROW, coalesceFunction);

        // Register nanvl - conditional function for NaN handling (float and double only)	 
     auto nanvlFunction = std::make_shared<NanvlFunction>();	 
     VectorFunction::RegisterVectorFunction("nanvl", {OMNI_FLOAT, OMNI_FLOAT}, OMNI_FLOAT, nanvlFunction);	 
     VectorFunction::RegisterVectorFunction("nanvl", {OMNI_DOUBLE, OMNI_DOUBLE}, OMNI_DOUBLE, nanvlFunction);

    // Register is_alpha: IS_ALPHA(string) -> boolean.
    // Returns true if the string is non-empty and every character is a Unicode letter;
    // NULL/empty input -> false (output NOT null). Numeric input -> false.
    // Flink SqlFunctionUtils.isAlpha semantics; Path B for NULL->false (non-null) handling.
    RegisterIsAlphaFunction(prefix + "is_alpha");

    // Register is_decimal: IS_DECIMAL(string) -> boolean.
    // Returns true if string can be parsed as a valid numeric (Java Double.parseDouble grammar);
    // NULL/empty input -> false (output NOT null). Non-null numeric input -> true.
    // Flink SqlFunctionUtils.isDecimal semantics; Path B for NULL->false (non-null) handling.
    RegisterIsDecimalFunction(prefix + "is_decimal");
}
}
