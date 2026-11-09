/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: RegexpExtractAll function implementation
 */
#include <limits>
#include <cstring>
#include <regex>
#include <shared_mutex>
#include <re2/re2.h>

#include "RegexpExtractAll.h"
#include "vector/vector.h"
#include "type/string_Impl.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

    static std::unordered_map<std::string, std::shared_ptr<re2::RE2>> re2_extract_all_cache;
    static std::shared_mutex re2_extract_all_mutex;
    static constexpr size_t MAX_CACHE_SIZE = 128;

void RegexpExtractAllFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                                      BaseVector *&result, ExecutionContext *context) const {
    int32_t argCnt = args.size();
    if (argCnt != 2 && argCnt != 3) {
        OMNI_THROW("RegexpExtractAll function Error", "Requires 2 or 3 arguments, got " + std::to_string(argCnt));
    }

    BaseVector *groupIdxVec = nullptr;
    BaseVector *patternVec = nullptr;
    BaseVector *strVec = nullptr;

    if (argCnt == 3) {
        groupIdxVec = args.top();
        args.pop();
    }
    patternVec = args.top();
    args.pop();
    strVec = args.top();
    args.pop();

    // ExtractAllMatches throws on empty / invalid patterns; guards release the args on that path too.
    using VectorGuard = std::unique_ptr<BaseVector, void (*)(BaseVector *)>;
    auto deleter = [](BaseVector *v) {
        if (!v->GetIsField()) {
            delete v;
        }
    };
    VectorGuard groupIdxGuard(groupIdxVec, deleter);
    VectorGuard patternGuard(patternVec, deleter);
    VectorGuard strGuard(strVec, deleter);

    ApplyRegexpExtractAll(strVec, patternVec, groupIdxVec, result, outputType);
}

void RegexpExtractAllFunction::ApplyRegexpExtractAll(BaseVector *strVec, BaseVector *patternVec,
                                                      BaseVector *groupIdxVec, BaseVector *&result,
                                                      const DataTypePtr &outputType) const {
    auto rowSize = strVec->GetSize();
    result = new ArrayVector(rowSize);
    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    if (!arrayResult) {
        OMNI_THROW("RegexpExtractAll function Error", "Result vector is not an ArrayVector");
    }

    std::vector<std::string> allElementsStorage;
    std::vector<int64_t> offsets;
    offsets.push_back(0);

    for (int32_t row = 0; row < rowSize; ++row) {
        if (strVec->IsNull(row) || patternVec->IsNull(row)) {
            arrayResult->SetNull(row);
            offsets.push_back(offsets.back());
            continue;
        }
        if (groupIdxVec != nullptr && groupIdxVec->IsNull(row)) {
            arrayResult->SetNull(row);
            offsets.push_back(offsets.back());
            continue;
        }

        std::string_view str = GetStringValueFromVector(strVec, row);
        std::string_view pattern = GetStringValueFromVector(patternVec, row);
        int32_t groupIdx = 0;
        if (groupIdxVec != nullptr) {
            if (groupIdxVec->GetTypeId() == OMNI_LONG) {
                int64_t g = GetLongValueFromVector(groupIdxVec, row);
                if (g < 0 || g > std::numeric_limits<int32_t>::max()) {
                    arrayResult->SetNull(row);
                    offsets.push_back(offsets.back());
                    continue;
                }
                groupIdx = static_cast<int32_t>(g);
            } else {
                groupIdx = GetIntValueFromVector(groupIdxVec, row);
            }
        }

        std::vector<std::string> rowMatches;
        ExtractAllMatches(str, pattern, groupIdx, rowMatches);

        int64_t currentOffset = offsets.back();
        offsets.push_back(currentOffset + static_cast<int64_t>(rowMatches.size()));
        for (const auto &s : rowMatches) {
            allElementsStorage.push_back(s);
        }
    }

    size_t totalElements = allElementsStorage.size();
    std::shared_ptr<BaseVector> elementVector = std::make_shared<Vector<LargeStringContainer<std::string_view>>>(
        static_cast<int32_t>(totalElements));
    auto *elemVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(elementVector.get());
    for (size_t i = 0; i < totalElements; ++i) {
        std::string_view sv(allElementsStorage[i]);
        elemVec->SetValue(static_cast<int32_t>(i), sv);
    }

    for (size_t i = 0; i < offsets.size(); ++i) {
        arrayResult->SetOffset(static_cast<int32_t>(i), static_cast<int32_t>(offsets[i]));
    }
    arrayResult->SetElementVector(elementVector);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (!strVec->IsNull(row) && !patternVec->IsNull(row) &&
            (groupIdxVec == nullptr || !groupIdxVec->IsNull(row))) {
            arrayResult->SetNotNull(row);
        }
    }
}

void RegexpExtractAllFunction::ExtractAllMatches(const std::string_view &plainStr,
                                                const std::string_view &patternStr,
                                                int32_t groupIdx,
                                                std::vector<std::string> &outMatches) const {
    if (patternStr.empty()) {
        throw omniruntime::exception::OmniException("RegexpExtractAll", "pattern must not be empty");
    }
    const char *str = plainStr.data();
    int32_t strLen = static_cast<int32_t>(plainStr.length());
    std::string newPattern(patternStr.data(), patternStr.size());
    std::shared_ptr<const re2::RE2> re_sp;
    {
        std::shared_lock<std::shared_mutex> read_lock(re2_extract_all_mutex);
        auto it = re2_extract_all_cache.find(newPattern);
        if (it != re2_extract_all_cache.end()) {
            re_sp = it->second;
        }
    }
    if (!re_sp) {
        auto re_uptr = std::make_shared<re2::RE2>(stringImpl::toStringPiece(newPattern), re2::RE2::Quiet);
        if (!re_uptr->ok()) {
            OMNI_THROW("RegexpExtractAll regex compile failed, pattern: ", newPattern);
        }
        {
            std::unique_lock<std::shared_mutex> write_lock(re2_extract_all_mutex);
            if (re2_extract_all_cache.size() >= MAX_CACHE_SIZE) {
                re2_extract_all_cache.erase(re2_extract_all_cache.begin());
            }
            re2_extract_all_cache.emplace(newPattern, re_uptr);
        }
        re_sp = re_uptr;
    }

    const re2::RE2 &re = *re_sp;
    int32_t numGroups = re.NumberOfCapturingGroups();
    if (groupIdx < 0 || groupIdx > numGroups) {
        return;
    }
    size_t nGroups = static_cast<size_t>(groupIdx) + 1;
    std::vector<re2::StringPiece> groups(nGroups);
    re2::StringPiece input(str, strLen);
    size_t pos = 0;

    while (re.Match(input, pos, strLen, RE2::UNANCHORED, groups.data(), static_cast<int>(nGroups))) {
        const re2::StringPiece fullMatch = groups[0];
        const re2::StringPiece subMatch = groups[groupIdx];
        if (subMatch.data() != nullptr) {
            outMatches.emplace_back(subMatch.data(), subMatch.size());
        } else {
            outMatches.emplace_back();
        }
        if (fullMatch.data() == nullptr) {
            break;
        }
        pos = fullMatch.data() + fullMatch.size() - input.data();
        if (fullMatch.size() == 0) {
            ++pos;
        }
    }
}

std::string_view RegexpExtractAllFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("RegexpExtractAll function Error", "Unsupported encoding type for string");
}

int32_t RegexpExtractAllFunction::GetIntValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<int32_t> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<int32_t> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<int32_t>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("RegexpExtractAll function Error", "Unsupported encoding type for integer");
}

int64_t RegexpExtractAllFunction::GetLongValueFromVector(BaseVector *vec, int32_t row) const {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<int64_t> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<int64_t> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<int64_t>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("RegexpExtractAll function Error", "Unsupported encoding type for bigint");
}
}
