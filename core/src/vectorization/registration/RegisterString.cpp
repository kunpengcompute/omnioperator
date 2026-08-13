/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#include <string>
#include <vector>
#include "../functions/String.h"
#include "../functions/SplitFunction.h"
#include "../functions/Cast.h"
// #include "../functions/Switch.h"
#include "../functions/RegexpExtract.h"
#include "../functions/Like.h"
#include "../functions/EqualStringFunction.h"
#include "../functions/ConcatFunction.h"
#include "../functions/ReverseFunction.h"
#include "../functions/FusedMd5ConcatWsFunction.h"
#ifdef OMNI_HAVE_ISAL_CRYPTO_MD5
#include "../functions/Md5VectorFunction.h"
#endif
#include "RegistrationHelpers.h"

namespace omniruntime::vectorization {
void RegisterStringFunctions(const std::string &prefix)
{
    RegisterFunction<InstrFunction, int32_t, std::string_view, std::string_view>(prefix + "instr", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<ReplaceFunction, std::string, std::string_view, std::string_view, std::string_view>(prefix + "replace", {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterString<StartsWithFunction>({prefix + "StartsWith"});
    RegisterString<EndsWithFunction>({prefix + "EndsWith"});
    RegisterString<ContainsFunction>({prefix + "Contains"});
    RegisterFunction<TrimFunction, std::string, std::string_view>(prefix + "Trim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LTrimFunction, std::string, std::string_view>(prefix + "LTrim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RTrimFunction, std::string, std::string_view>(prefix + "RTrim", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<TrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "Trim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LTrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "LTrim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<RTrimWithCharsFunction, std::string, std::string_view, std::string_view>(
        prefix + "RTrim", {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LowerFunction, std::string, std::string_view>(prefix + "lower", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<LowerFunction, std::string, std::string_view>(prefix + "lower", {OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<UpperFunction, std::string, std::string_view>(prefix + "upper", {OMNI_VARCHAR}, OMNI_VARCHAR);
    RegisterFunction<UpperFunction, std::string, std::string_view>(prefix + "upper", {OMNI_CHAR}, OMNI_VARCHAR);
    RegisterFunction<SoundexFunction, std::string, std::string_view>(prefix + "soundex", {OMNI_VARCHAR}, OMNI_VARCHAR);

    // char_length / character_length / length(string) -> integer (character count, Unicode-aware)
    RegisterFunction<CharLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<CharLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_CHAR}, OMNI_INT);
    // length(binary) -> integer (byte count)
    RegisterFunction<BinaryLengthFunction, int32_t, std::string_view>(prefix + "length", {OMNI_VARBINARY}, OMNI_INT);
    // ascii(string) -> int32; align with velox (Varchar), add CHAR for upstream compatibility
    RegisterFunction<AsciiFunction, int32_t, std::string_view>(prefix + "ascii", {OMNI_VARCHAR}, OMNI_INT);
    RegisterFunction<ChrFunction, std::string, int64_t>(prefix + "chr", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<ChrFunction, std::string, int64_t>(prefix + "char", {OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<Base64Function, std::string, std::string_view>(prefix + "base64", {OMNI_VARBINARY}, OMNI_VARCHAR);
    RegisterFunction<Base64Function, std::string, std::string_view>(prefix + "base64", {OMNI_VARCHAR}, OMNI_VARCHAR);
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
    // Spark + Gluten: substr(string, start), substr(string, start, length); Gluten maps "substring" -> "substr"
    RegisterFunction<SubstrFunction, std::string, std::string_view, int32_t>(
        prefix + "substr", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<SubstrFunction, std::string, std::string_view, int32_t, int32_t>(
        prefix + "substr", {OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);

    // left(string, length) / right(string, length) -> varchar
    // Leftmost / rightmost `length` characters; length <= 0 -> empty; Unicode-aware.
    // Support VARCHAR/CHAR string with INT/LONG length (same type matrix as repeat).
    RegisterFunction<LeftFunction, std::string, std::string_view, int32_t>(
        prefix + "left", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<LeftFunction, std::string, std::string_view, int64_t>(
        prefix + "left", {OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<LeftFunction, std::string, std::string_view, int32_t>(
        prefix + "left", {OMNI_CHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<LeftFunction, std::string, std::string_view, int64_t>(
        prefix + "left", {OMNI_CHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<RightFunction, std::string, std::string_view, int32_t>(
        prefix + "right", {OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<RightFunction, std::string, std::string_view, int64_t>(
        prefix + "right", {OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    RegisterFunction<RightFunction, std::string, std::string_view, int32_t>(
        prefix + "right", {OMNI_CHAR, OMNI_INT}, OMNI_VARCHAR);
    RegisterFunction<RightFunction, std::string, std::string_view, int64_t>(
        prefix + "right", {OMNI_CHAR, OMNI_LONG}, OMNI_VARCHAR);

    // Register concat function with variable arity support (2 to 10 arguments).
    // Each arity gets its own instance so that Apply() knows how many operands to pop.
    constexpr size_t maxConcatArity = 10;
    for (size_t arity = 2; arity <= maxConcatArity; ++arity) {
        VectorFunction::RegisterVectorFunction("concat", std::vector<DataTypeId>(arity, OMNI_VARCHAR),
            OMNI_VARCHAR, std::make_shared<ConcatFunction>(arity));
    }

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
