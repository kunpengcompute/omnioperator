/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include <string>
#include "../functions/String.h"
#include "../functions/SplitFunction.h"
#include "../functions/Cast.h"
// #include "../functions/Switch.h"
#include "../functions/RegexpExtract.h"
#include "../functions/Like.h"
#include "../functions/EqualStringFunction.h"
#include "../functions/ConcatFunction.h"
#ifdef STRINGVIEW_ENABLE
#include "../functions/ConcatStringViewFunction.h"
#endif
#include "../functions/ReverseFunction.h"
#ifdef STRINGVIEW_ENABLE
#include "../functions/StringViewSliceFunctions.h"
#endif
#include "../functions/FusedMd5ConcatWsFunction.h"
#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
#include "../functions/Md5VectorFunction.h"
#endif
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterStringFunctions(const std::string &prefix)
{
    RegisterString<StartsWithFunction>({prefix + "StartsWith"});
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<StartsWithFunction, bool, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "StartsWith", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_BOOLEAN);
    RegisterFunction<StartsWithFunction, bool, omniruntime::vec::StringView, std::string_view>(
        prefix + "StartsWith", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_BOOLEAN);
#endif
    RegisterString<EndsWithFunction>({prefix + "EndsWith"});
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<EndsWithFunction, bool, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "EndsWith", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_BOOLEAN);
    RegisterFunction<EndsWithFunction, bool, omniruntime::vec::StringView, std::string_view>(
        prefix + "EndsWith", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_BOOLEAN);
#endif
    RegisterString<ContainsFunction>({prefix + "Contains"});
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<ContainsFunction, bool, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "Contains", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_BOOLEAN);
    RegisterFunction<ContainsFunction, bool, omniruntime::vec::StringView, std::string_view>(
        prefix + "Contains", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_BOOLEAN);
#endif
    RegisterFunction<TrimFunction, std::string, std::string_view>(prefix + "Trim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LTrimFunction, std::string, std::string_view>(prefix + "LTrim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RTrimFunction, std::string, std::string_view>(prefix + "RTrim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "Trim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LTrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "LTrim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RTrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "RTrim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<TrimFunction, std::string, omniruntime::vec::StringView>(
        prefix + "Trim", {OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<LTrimFunction, std::string, omniruntime::vec::StringView>(
        prefix + "LTrim", {OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<RTrimFunction, std::string, omniruntime::vec::StringView>(
        prefix + "RTrim", {OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<TrimWithCharsFunction, std::string, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "Trim", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<TrimWithCharsFunction, std::string, std::string_view, omniruntime::vec::StringView>(
        prefix + "Trim", {OMNI_VARCHAR, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<LTrimWithCharsFunction, std::string, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "LTrim", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<LTrimWithCharsFunction, std::string, std::string_view, omniruntime::vec::StringView>(
        prefix + "LTrim", {OMNI_VARCHAR, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<RTrimWithCharsFunction, std::string, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "RTrim", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<RTrimWithCharsFunction, std::string, std::string_view, omniruntime::vec::StringView>(
        prefix + "RTrim", {OMNI_VARCHAR, OMNI_STRING_VIEW}, OMNI_VARCHAR);
#endif
    RegisterFunction<LowerFunction, std::string, std::string_view>(prefix + "lower", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LowerFunction, std::string, std::string_view>(prefix + "lower", {OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<UpperFunction, std::string, std::string_view>(prefix + "upper", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<UpperFunction, std::string, std::string_view>(prefix + "upper", {OMNI_CHAR}, OMNI_VARCHAR);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<LowerFunction, std::string, omniruntime::vec::StringView>(
        prefix + "lower", {OMNI_STRING_VIEW}, OMNI_VARCHAR);
    RegisterFunction<UpperFunction, std::string, omniruntime::vec::StringView>(
        prefix + "upper", {OMNI_STRING_VIEW}, OMNI_VARCHAR);
#endif
    RegisterFunction<SoundexFunction, std::string, std::string_view>(prefix + "soundex", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // char_length / character_length / length(string) -> integer (character count, Unicode-aware)
    RegisterFunction<CharLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<CharLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_CHAR}, OMNI_INT);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<CharLengthFunction, int32_t, omniruntime::vec::StringView>(
        prefix + "length", {OMNI_STRING_VIEW}, OMNI_INT);
#endif
    // length(binary) -> integer (byte count)
    RegisterFunction<BinaryLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_VARBINARY}, OMNI_INT);
    // ascii(string) -> int32; align with velox (Varchar), add CHAR for upstream compatibility
    RegisterFunction<AsciiFunction, int32_t, std::string_view>(prefix + "ascii", {OMNI_VARCHAR}, OMNI_INT);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<AsciiFunction, int32_t, omniruntime::vec::StringView>(
        prefix + "ascii", {OMNI_STRING_VIEW}, OMNI_INT);
#endif
    RegisterFunction<ChrFunction, std::string, int64_t>(prefix + "chr", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<ChrFunction, std::string, int64_t>(prefix + "char", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<Base64Function, std::string, std::string_view>(prefix + "base64", {OMNI_VARBINARY}, OMNI_VARCHAR);
    RegisterFunction<UnBase64Function, std::string, std::string_view>(prefix + "unbase64", {OMNI_VARCHAR}, OMNI_VARBINARY);
    // unhex(string) -> varbinary: converts hex string to binary data
    RegisterFunction<UnhexFunction, std::string, std::string_view>(prefix + "unhex", {OMNI_VARCHAR}, OMNI_VARBINARY);

    VectorFunction::RegisterVectorFunction("split", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_ARRAY,
        std::make_shared<SplitFunction>());
    // VectorFunction::RegisterVectorFunction("equal", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN, std::make_shared<EqualStringFunction>());
    VectorFunction::RegisterVectorFunction("regexp_extract", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT},
        OMNI_VARCHAR, std::make_shared<RegexpExtractFunction>());
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN,
        std::make_shared<LikeFunction>(2));
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN,
        std::make_shared<LikeFunction>(3));
#ifdef STRINGVIEW_ENABLE
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_BOOLEAN,
        std::make_shared<LikeFunction>(2));
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_BOOLEAN,
        std::make_shared<LikeFunction>(2));
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_STRING_VIEW, OMNI_STRING_VIEW, OMNI_STRING_VIEW},
        OMNI_BOOLEAN, std::make_shared<LikeFunction>(3));
    VectorFunction::RegisterVectorFunction("LIKE", {OMNI_STRING_VIEW, OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_BOOLEAN, std::make_shared<LikeFunction>(3));
#endif
    // Spark + Gluten: substr(string, start), substr(string, start, length); Gluten maps "substring" -> "substr"
    RegisterFunction<SubstrFunction, std::string, std::string_view, int32_t>(
        prefix + "substr", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<SubstrFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "substr", {OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<SubstrFunction, std::string, omniruntime::vec::StringView, int32_t>(
        prefix + "substr", {OMNI_STRING_VIEW, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<SubstrFunction, std::string, omniruntime::vec::StringView, int32_t, int32_t>(
        prefix + "substr", {OMNI_STRING_VIEW, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
    auto substrSVOut = std::make_shared<SubstrStringViewFunction>();
    VectorFunction::RegisterVectorFunction(prefix + "substr", {OMNI_STRING_VIEW, OMNI_INT},
        OMNI_STRING_VIEW, substrSVOut);
    VectorFunction::RegisterVectorFunction(prefix + "substr", {OMNI_STRING_VIEW, OMNI_INT, OMNI_INT},
        OMNI_STRING_VIEW, substrSVOut);
    VectorFunction::RegisterVectorFunction(prefix + "Trim", {OMNI_STRING_VIEW}, OMNI_STRING_VIEW,
        std::make_shared<TrimStringViewFunction>(TrimStringViewFunction::Kind::Trim));
    VectorFunction::RegisterVectorFunction(prefix + "LTrim", {OMNI_STRING_VIEW}, OMNI_STRING_VIEW,
        std::make_shared<TrimStringViewFunction>(TrimStringViewFunction::Kind::LTrim));
    VectorFunction::RegisterVectorFunction(prefix + "RTrim", {OMNI_STRING_VIEW}, OMNI_STRING_VIEW,
        std::make_shared<TrimStringViewFunction>(TrimStringViewFunction::Kind::RTrim));
#endif

    // Register concat function with variable arity support
    // Register multiple signatures for different argument counts (2 to 10 arguments)
    auto concatFunction = std::make_shared<ConcatFunction>();
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
#ifdef STRINGVIEW_ENABLE
    VectorFunction::RegisterVectorFunction("concat", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_STRING_VIEW}, OMNI_VARCHAR, concatFunction);
    auto concatSVOut = std::make_shared<ConcatStringViewFunction>();
    VectorFunction::RegisterVectorFunction("concat", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_STRING_VIEW,
        concatSVOut);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_STRING_VIEW, concatSVOut);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_STRING_VIEW}, OMNI_STRING_VIEW, concatSVOut);
#endif
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);
    VectorFunction::RegisterVectorFunction("concat", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR, concatFunction);

    // Register reverse function: reverses a string character by character
    VectorFunction::RegisterVectorFunction("reverse", {OMNI_VARCHAR}, OMNI_VARCHAR,
        std::make_shared<ReverseFunction>());

    // repeat(string, n) -> varchar
    // Returns the string which repeats input n times. Result size <= 1MB. n <= 0 or empty input -> "".
    // Support VARCHAR/CHAR with INT/LONG (all Velox-supported type combinations)
    RegisterFunction<RepeatFunction, std::string, std::string_view, int32_t>(
        prefix + "repeat", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<RepeatFunction, std::string, std::string_view, int64_t>(
        prefix + "repeat", {OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<RepeatFunction, std::string, std::string_view, int32_t>(
        prefix + "repeat", {OMNI_CHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<RepeatFunction, std::string, std::string_view, int64_t>(
        prefix + "repeat", {OMNI_CHAR, OMNI_LONG}, OMNI_VARCHAR);

    // Register locate function with full type support
    // locate(substring, string, start) -> integer
    // Support all combinations of VARCHAR/CHAR string types and INT32/INT64 integer types
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int64_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_CHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int64_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_CHAR, OMNI_LONG}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "locate", {OMNI_CHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int64_t>(
        prefix + "locate", {OMNI_CHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "locate", {OMNI_CHAR, OMNI_CHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, std::string_view, int64_t>(
        prefix + "locate", {OMNI_CHAR, OMNI_CHAR, OMNI_LONG}, OMNI_INT);
#ifdef STRINGVIEW_ENABLE
    RegisterFunction<LocateFunction, int32_t, omniruntime::vec::StringView, omniruntime::vec::StringView, int32_t>(
        prefix + "locate", {OMNI_STRING_VIEW, OMNI_STRING_VIEW, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, omniruntime::vec::StringView, omniruntime::vec::StringView, int64_t>(
        prefix + "locate", {OMNI_STRING_VIEW, OMNI_STRING_VIEW, OMNI_LONG}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, omniruntime::vec::StringView, int32_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_STRING_VIEW, OMNI_INT}, OMNI_INT);
    RegisterFunction<LocateFunction, int32_t, std::string_view, omniruntime::vec::StringView, int64_t>(
        prefix + "locate", {OMNI_VARCHAR, OMNI_STRING_VIEW, OMNI_LONG}, OMNI_INT);
    RegisterFunction<InStrFunction, int32_t, omniruntime::vec::StringView, omniruntime::vec::StringView>(
        prefix + "instr", {OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_INT);
    RegisterFunction<InStrFunction, int32_t, omniruntime::vec::StringView, std::string_view>(
        prefix + "instr", {OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_INT);
#endif
    RegisterFunction<InStrFunction, int32_t, std::string_view, std::string_view>(
        prefix + "instr", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<InStrFunction, int32_t, std::string_view, std::string_view>(
        prefix + "instr", {OMNI_CHAR, OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<InStrFunction, int32_t, std::string_view, std::string_view>(
        prefix + "instr", {OMNI_VARCHAR, OMNI_CHAR}, OMNI_INT);
    RegisterFunction<ReplaceFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "replace", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<ReplaceFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "replace", {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<ReplaceFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "replace", {OMNI_VARCHAR, OMNI_CHAR, OMNI_CHAR}, OMNI_VARCHAR);

    // position(substring, string) -> integer, equivalent to locate(substring, string, 1)
    RegisterFunction<PositionFunction, int32_t, std::string_view, std::string_view>(
        prefix + "position", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<PositionFunction, int32_t, std::string_view, std::string_view>(
        prefix + "position", {OMNI_VARCHAR, OMNI_CHAR}, OMNI_INT);
    RegisterFunction<PositionFunction, int32_t, std::string_view, std::string_view>(
        prefix + "position", {OMNI_CHAR, OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<PositionFunction, int32_t, std::string_view, std::string_view>(
        prefix + "position", {OMNI_CHAR, OMNI_CHAR}, OMNI_INT);

    // bit_length(string/binary) -> integer
    // Returns the bit length of the input (byte length * 8)
    RegisterFunction<BitLengthFunction, int32_t, std::string_view>(
        prefix + "bit_length", {OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<BitLengthFunction, int32_t, std::string_view>(
        prefix + "bit_length", {OMNI_CHAR}, OMNI_INT);
    RegisterFunction<BitLengthFunction, int32_t, std::string_view>(
        prefix + "bit_length", {OMNI_VARBINARY}, OMNI_INT);

    // lpad(string, size, padString) -> varchar
    // Left pads string to size characters with padString
    // Support all combinations of VARCHAR/CHAR string types and INT32/INT64 size types
    RegisterFunction<LPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "lpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "lpad", {OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "lpad", {OMNI_CHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "lpad", {OMNI_CHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "lpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "lpad", {OMNI_VARCHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "lpad", {OMNI_CHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "lpad", {OMNI_CHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);

    // rpad(string, size, padString) -> varchar
    // Right pads string to size characters with padString
    // Support all combinations of VARCHAR/CHAR string types and INT32/INT64 size types
    RegisterFunction<RPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "rpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "rpad", {OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "rpad", {OMNI_CHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "rpad", {OMNI_CHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "rpad", {OMNI_VARCHAR, OMNI_LONG, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "rpad", {OMNI_VARCHAR, OMNI_INT, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int64_t, std::string_view>(
        prefix + "rpad", {OMNI_CHAR, OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RPadFunction, std::string, std::string_view, int32_t, std::string_view>(
        prefix + "rpad", {OMNI_CHAR, OMNI_INT, OMNI_VARCHAR}, OMNI_VARCHAR);

    // overlay(input, replace, pos, len) -> varchar
    RegisterFunction<OverlayFunction, std::string, std::string_view, std::string_view, int32_t, int32_t>(
        prefix + "overlay", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);

    // split_part(string, delimiter, index) -> varchar
    // Splits string on delimiter and returns the part at index (1-based).
    // Returns NULL if index is larger than the number of fields.
    // Supports VARCHAR and CHAR string types with INT64 index type.
    RegisterFunction<SplitPartFunction, std::string, std::string_view, std::string_view, int64_t>(
        prefix + "split_part", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<SplitPartFunction, std::string, std::string_view, std::string_view, int64_t>(
        prefix + "split_part", {OMNI_VARCHAR, OMNI_CHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<SplitPartFunction, std::string, std::string_view, std::string_view, int64_t>(
        prefix + "split_part", {OMNI_CHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<SplitPartFunction, std::string, std::string_view, std::string_view, int64_t>(
        prefix + "split_part", {OMNI_CHAR, OMNI_CHAR, OMNI_LONG}, OMNI_VARCHAR);

    // translate(string, match, replace) -> varchar
    // Translates characters in string that match characters in match to corresponding characters in replace.
    // Support all combinations of VARCHAR/CHAR string types
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_CHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_VARCHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_CHAR, OMNI_CHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_CHAR, OMNI_VARCHAR, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_VARCHAR, OMNI_CHAR, OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<TranslateFunction, std::string, std::string_view, std::string_view, std::string_view>(
        prefix + "translate", {OMNI_CHAR, OMNI_CHAR, OMNI_CHAR}, OMNI_VARCHAR);

    // find_in_set(str, strArray) -> integer
    // Returns 1-based index of str in comma-delimited strArray, 0 if not found.
    RegisterFunction<FindInSetFunction, int32_t, std::string_view, std::string_view>(
        prefix + "find_in_set", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);

    // initcap(string) -> string
    // Capitalizes the first letter of each word; lowercases the rest.
    // Word boundaries are whitespace characters (Spark SQL semantics).
    RegisterFunction<InitCapFunction, std::string, std::string_view>(prefix + "initcap", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // levenshtein(left, right) -> integer
    // levenshtein(left, right, threshold) -> integer
    // Computes Levenshtein edit distance between two strings.
    RegisterFunction<LevenshteinDistanceFunction, int32_t, std::string_view, std::string_view>(
        prefix + "levenshtein", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
    // 3-arg with threshold
    RegisterFunction<LevenshteinDistanceFunction, int32_t, std::string_view, std::string_view, int32_t>(
        prefix + "levenshtein", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT}, OMNI_INT);
    RegisterFunction<LevenshteinDistanceFunction, int32_t, std::string_view, std::string_view, int64_t>(
        prefix + "levenshtein", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_LONG}, OMNI_INT);

    RegisterFunction<Sha1HexStringFunction, std::string, std::string_view>(prefix + "sha1", {OMNI_VARBINARY}, OMNI_VARCHAR);
    RegisterFunction<Sha2HexStringFunction, std::string, std::string_view, int32_t>(prefix + "sha2", {OMNI_VARBINARY, OMNI_INT}, OMNI_VARCHAR);
#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
    VectorFunction::RegisterVectorFunctionFactory(
        prefix + "Md5", {OMNI_VARBINARY}, OMNI_VARCHAR, MakeMd5VectorFunction);
#else
    RegisterFunction<Md5Function, std::string, std::string_view>(
        prefix + "Md5", {OMNI_VARBINARY}, OMNI_VARCHAR);
#endif
    VectorFunction::RegisterVectorFunctionFactory(
        FusedMd5ConcatWsSignatures(prefix + "FusedMd5ConcatWs"), MakeFusedMd5ConcatWsFunction);
    RegisterFunction<StaticInvokeVarcharTypeWriteSideCheckFunction, std::string, std::string_view, int32_t>(prefix + "StaticInvokeVarcharTypeWriteSideCheck", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<StaticInvokeCharTypeWriteSideCheckFunction, std::string, std::string_view, int32_t>(prefix + "StaticInvokeCharTypeWriteSideCheck", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<StaticInvokeCharReadPaddingFunction, std::string, std::string_view, int32_t>(prefix + "StaticInvokeCharReadPadding", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
}
}
