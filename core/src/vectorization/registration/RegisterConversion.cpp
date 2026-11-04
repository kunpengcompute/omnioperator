/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include <string>

#include "../functions/Cast.h"
#include "../functions/ArrayCastFunction.h"
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterConversionFunctions(const std::string &prefix)
{
    // Register comprehensive cast function for all supported type combinations
    auto castFunction = std::make_shared<CastFunction>();
    auto arrayCastFunction = std::make_shared<ArrayCastFunction>();
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_ARRAY}, OMNI_ARRAY, arrayCastFunction);

    // String to numeric types
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_BOOLEAN, castFunction);
    
    // Numeric to string types
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_VARCHAR, castFunction);
    
    // Numeric to numeric conversions - BYTE
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_BYTE, castFunction);
    
    // Numeric to numeric conversions - SHORT
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_SHORT, castFunction);
    
    // Numeric to numeric conversions - INT
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_INT, castFunction);
    
    // Numeric to numeric conversions - LONG
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_LONG, castFunction);
    
    // Numeric to numeric conversions - FLOAT
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_FLOAT, castFunction);
    
    // Numeric to numeric conversions - DOUBLE
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_DOUBLE, castFunction);
    
    // Boolean conversions
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_DOUBLE, castFunction);
    
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_BOOLEAN, castFunction);
    
    // Same type conversions (identity casts)
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BOOLEAN}, OMNI_BOOLEAN, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_VARCHAR, castFunction);


    // Decimal64 conversions
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL64}, OMNI_DECIMAL128, castFunction);
    
    // Decimal128 conversions
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_BYTE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_SHORT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_FLOAT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_DOUBLE, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DECIMAL128}, OMNI_DECIMAL128, castFunction);
    
    // Numeric to Decimal conversions
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_DECIMAL64, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_DECIMAL64, castFunction);
    
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_BYTE}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_SHORT}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_FLOAT}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DOUBLE}, OMNI_DECIMAL128, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_DECIMAL128, castFunction);

    // Date/Timestamp conversions (treated as numeric types)
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DATE32}, OMNI_INT, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_INT}, OMNI_DATE32, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_TIMESTAMP}, OMNI_LONG, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_LONG}, OMNI_TIMESTAMP, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_DATE32}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_TIMESTAMP}, OMNI_VARCHAR, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_DATE32, castFunction);
    VectorFunction::RegisterVectorFunction("CAST", {OMNI_VARCHAR}, OMNI_TIMESTAMP, castFunction);
}
}
