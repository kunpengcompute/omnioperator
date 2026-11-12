/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RegexpExtract function implementation
 */
#include <limits>
#include <cstring>
#include <regex>
#include <shared_mutex>
#include <re2/re2.h>

#include "RegexpExtract.h"
#include "vector/vector.h"
#include "type/string_Impl.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

    static const re2::RE2 kRegex_FixNameGroup("[(][?]<([^>]*)>", re2::RE2::Quiet);
    static std::unordered_map<std::string, std::shared_ptr<re2::RE2>> re2_cache;
    static std::shared_mutex re2_mutex;
    static constexpr size_t MAX_CACHE_SIZE = 128;

    static constexpr std::array<bool, 128> GetMetaCharTable() {
        std::array<bool, 128> arr{};
        const char meta[] = R"(\.+*?^$()[]{}|)";
        for (char c : meta) {
            arr[static_cast<uint8_t>(c)] = true;
        }
        return arr;
    }
    static constexpr std::array<bool, 128> g_re_meta_chars = GetMetaCharTable();

void RegexpExtractFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const {
    BaseVector *groupIdxVec = nullptr;
    BaseVector *patternVec = nullptr;
    BaseVector *strVec = nullptr;
    groupIdxVec = args.top();
    args.pop();
    patternVec = args.top();
    args.pop();
    strVec = args.top();
    args.pop();
    ApplyRegexpExtract(strVec, patternVec, groupIdxVec, result, outputType, context);
    delete groupIdxVec;
    delete patternVec;
    delete strVec;
}

void RegexpExtractFunction::ApplyRegexpExtract(BaseVector *strVec, BaseVector *patternVec,
    BaseVector *groupIdxVec, BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        // If string or pattern is NULL, result is NULL
        if (strVec->IsNull(row) || patternVec->IsNull(row)) {
            result->SetNull(row);
            continue;
        }
        std::string_view str = GetStringValueFromVector(strVec, row);
        std::string_view pattern = GetStringValueFromVector(patternVec, row);
        int32_t groupIdx = GetIntValueFromVector(groupIdxVec, row);

        std::string extracted = ExtractRegex(str, pattern, groupIdx);
        std::string_view extractedView(extracted);
        SetStringValueToVector(result, row, extractedView);
    }
}

std::string RegexpExtractFunction::ExtractRegex(const std::string_view &plainStr, const std::string_view &patternStr,
    int32_t idx) const {
    if(patternStr.size() == 0) {
        throw omniruntime::exception::OmniException("patternStr must not empty");
    }
    const char *str = plainStr.data();
    int32_t strLen = plainStr.length();
    const char *pattern = patternStr.data();
    int32_t patternLen = patternStr.length();
    std::string newPattern(pattern, patternLen);
    std::shared_ptr<const re2::RE2> re_sp;
    {
        std::shared_lock<std::shared_mutex> read_lock(re2_mutex);
        auto cache_iter = re2_cache.find(newPattern);
        if (cache_iter != re2_cache.end()) {
            re_sp = cache_iter->second;
        }
    }
    if (!re_sp) {
       auto re_uptr = std::make_shared<re2::RE2>(stringImpl::toStringPiece(newPattern), re2::RE2::Quiet);
       if (!re_uptr->ok()) {
            OMNI_THROW("regex expr compile failed, patternStr: ", newPattern);
        }
        {
         std::unique_lock<std::shared_mutex> write_lock(re2_mutex);
         if (re2_cache.size() == MAX_CACHE_SIZE) {
             re2_cache.erase(re2_cache.begin());
         }
         re2_cache.emplace(newPattern, re_uptr);
        }
        re_sp = std::move(re_uptr);
     }

    const re2::RE2& re = *re_sp;
    int maxIdx = re.NumberOfCapturingGroups();
    if (idx > maxIdx) {
        return "";
    }
    std::vector<re2::StringPiece> groups(idx + 1);
    auto input = re2::StringPiece(str, strLen);
    if (!re.Match(input, 0, strLen, RE2::UNANCHORED, groups.data(), idx + 1)) {
        return "";
    } else {
        const re2::StringPiece extracted = groups[idx];
        return std::string(extracted.data(), extracted.size());
    }
}

std::string_view RegexpExtractFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("RegexpExtract function Error", "Unsupported encoding type for string");
    }
}

int32_t RegexpExtractFunction::GetIntValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<int32_t> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<int32_t> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<int32_t>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("RegexpExtract function Error", "Unsupported encoding type for integer");
    }
}

void RegexpExtractFunction::SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const {
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
}
}