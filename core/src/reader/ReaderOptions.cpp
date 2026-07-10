#include "ReaderOptions.h"
#include <boost/algorithm/string.hpp>
#include <memory>
#include <vector>
#include "reader/common/UriInfo.h"
#include "reader/common/JulianGregorianRebase.h"
#include "reader/common/PredicateUtil.h"

using namespace omniruntime::type;
namespace  omniruntime::reader {

static constexpr int32_t MAX_DECIMAL64_DIGITS = 18;
uint64_t batchLen = 0;

// Static helper method to parse comma-separated column names string
static std::vector<std::string> ParseColumnNamesString(const std::string& colsStr) {
    std::vector<std::string> columns;
    std::stringstream ss(colsStr);
    std::string col;
    while (std::getline(ss, col, ',')) {
        // Trim leading whitespace
        col.erase(0, col.find_first_not_of(" \t"));
        // Trim trailing whitespace
        col.erase(col.find_last_not_of(" \t") + 1);
        // Add non-empty columns
        if (!col.empty()) {
            columns.push_back(col);
        }
    }
    return columns;
}

bool StringToBool(const std::string &boolStr)
{
    if (boost::iequals(boolStr, "true")) {
        return true;
    } else if (boost::iequals(boolStr, "false")) {
        return false;
    } else {
        throw std::runtime_error("Invalid input for stringToBool.");
    }
}

int GetLiteral(::orc::Literal &lit, int leafType, const std::string &value)
{
    switch ((::orc::PredicateDataType)leafType) {
        case ::orc::PredicateDataType::LONG: {
            lit = ::orc::Literal(static_cast<int64_t>(std::stol(value)));
            break;
        }
        case ::orc::PredicateDataType::FLOAT: {
            lit = ::orc::Literal(static_cast<double>(std::stod(value)));
            break;
        }
        case ::orc::PredicateDataType::STRING: {
            lit = ::orc::Literal(value.c_str(), value.size());
            break;
        }
        case ::orc::PredicateDataType::DATE: {
            lit = ::orc::Literal(orc::PredicateDataType::DATE, static_cast<int64_t>(std::stol(value)));
            break;
        }
        case ::orc::PredicateDataType::TIMESTAMP: {
            std::vector<std::string> valList;
            std::istringstream timestampStr(value);
            std::string tmpStr;
            while (timestampStr >> tmpStr) {
                valList.push_back(tmpStr);
            }
            lit = ::orc::Literal(std::stoll(valList[0]), std::stoi(valList[1]));
            break;
        }
        case ::orc::PredicateDataType::DECIMAL: {
            std::vector<std::string> valList;
            // Decimal(22, 6) eg: value ("19999999999998,998000 22 6")
            std::istringstream tmpAllStr(value);
            std::string tmpStr;
            while (tmpAllStr >> tmpStr) {
                valList.push_back(tmpStr);
            }
            orc::Decimal decimalVal(valList[0]);
            lit = ::orc::Literal(decimalVal.value, static_cast<int32_t>(std::stoi(valList[1])),
                                 static_cast<int32_t>(std::stoi(valList[2])));
            break;
        }
        case ::orc::PredicateDataType::BOOLEAN: {
            lit = ::orc::Literal(static_cast<bool>(StringToBool(value)));
            break;
        }
        default: {
            throw std::runtime_error("tableScan jni getLiteral unsupported leafType: " + leafType);
        }
    }
    return 0;
}

int BuildLeaves(PredicateOperatorType leafOp, std::vector<orc::Literal> &litList, orc::Literal &lit, const std::string &leafNameString,
                orc::PredicateDataType leafType, orc::SearchArgumentBuilder &builder)
{
    switch (leafOp) {
        case PredicateOperatorType::LESS_THAN: {
            builder.lessThan(leafNameString, leafType, lit);
            break;
        }
        case PredicateOperatorType::LESS_THAN_EQUALS: {
            builder.lessThanEquals(leafNameString, leafType, lit);
            break;
        }
        case PredicateOperatorType::EQUALS: {
            builder.equals(leafNameString, leafType, lit);
            break;
        }
        case PredicateOperatorType::NULL_SAFE_EQUALS: {
            builder.nullSafeEquals(leafNameString, leafType, lit);
            break;
        }
        case PredicateOperatorType::IS_NULL: {
            builder.isNull(leafNameString, leafType);
            break;
        }
        case PredicateOperatorType::IN: {
            if (litList.empty()) {
                std::string emptyString;
                builder.in(emptyString, leafType, litList);
            } else {
                builder.in(leafNameString, leafType, litList);
            }
            break;
        }
        case PredicateOperatorType::BETWEEN: {
            throw std::runtime_error("table scan buildLeaves BETWEEN is not supported!");
        }
        default: {
            throw std::runtime_error("table scan buildLeaves illegal input!");
        }
    }
    return 1;
}

int initLeaves(orc::SearchArgumentBuilder& builder,const nlohmann::json& jsonExp,const nlohmann::json& jsonLeaves) {
    if (!jsonExp.contains("leaf") || !jsonExp["leaf"].is_string()) {
        throw std::runtime_error("Missing or invalid 'leaf' field in expression");
    }
    std::string leafKey = jsonExp["leaf"].get<std::string>();

    if (!jsonLeaves.contains(leafKey)) {
        throw std::runtime_error("Leaf definition not found: " + leafKey);
    }
    const auto& leafJson = jsonLeaves[leafKey];

    if (!leafJson.contains("name") || !leafJson["name"].is_string()) {
        throw std::runtime_error("Leaf missing 'name' field: " + leafKey);
    }
    std::string leafNameString = leafJson["name"].get<std::string>();

    if (!leafJson.contains("op") || !leafJson["op"].is_number_integer()) {
        throw std::runtime_error("Leaf missing or invalid 'op' field: " + leafKey);
    }
    int leafOp = leafJson["op"].get<int>();

    if (!leafJson.contains("type") || !leafJson["type"].is_number_integer()) {
        throw std::runtime_error("Leaf missing or invalid 'type' field: " + leafKey);
    }
    int leafType = leafJson["type"].get<int>();

    orc::Literal lit(0L);
    bool hasLiteral = false;
    if (leafJson.contains("literal") && leafJson["literal"].is_string()) {
        std::string leafValueString = leafJson["literal"].get<std::string>();
        if (!leafValueString.empty() || static_cast<orc::PredicateDataType>(leafType) == orc::PredicateDataType::STRING) {
            GetLiteral(lit, leafType, leafValueString);
            hasLiteral = true;
        }
    }

    std::vector<orc::Literal> litList;
    bool invalidList = false;

    if (leafJson.contains("literalList")) {
        try {
            std::string listStr = leafJson["literalList"].get<std::string>();
            auto literalListJson = nlohmann::json::parse(listStr);

            if (literalListJson.is_array()) {
                for (const auto& elem : literalListJson) {
                    if (elem.is_null()) {
                        litList.clear();
                        invalidList = true;
                        break;
                    } else if (elem.is_string()) {
                        std::string val = elem.get<std::string>();
                        GetLiteral(lit, leafType, val);
                        litList.push_back(lit);
                    } else {
                        throw std::runtime_error("Non-string element in literalList");
                    }
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to parse literalList JSON: " + std::string(e.what()));
        }
    }

    if (invalidList) {
        litList.clear();
    }

    BuildLeaves(
            static_cast<PredicateOperatorType>(leafOp),
            litList,
            lit,
            leafNameString,
            static_cast<orc::PredicateDataType>(leafType),
            builder
    );

    return 1;
}

int initExpressionTree(orc::SearchArgumentBuilder& builder,const nlohmann::json& jsonExp, const nlohmann::json& jsonLeaves) {
    if (!jsonExp.contains("op") || !jsonExp["op"].is_number_integer()) {
        throw std::runtime_error("Missing or invalid 'op' field in expression tree");
    }
    int op = jsonExp["op"].get<int>();

    if (op == static_cast<int>(Operator::LEAF)) {
        return initLeaves(builder, jsonExp, jsonLeaves);
    }

    switch (static_cast<Operator>(op)) {
        case Operator::OR: {
            builder.startOr();
            break;
        }
        case Operator::AND: {
            builder.startAnd();
            break;
        }
        case Operator::NOT: {
            builder.startNot();
            break;
        }
        default: {
            throw std::runtime_error("Unsupported operator in expression tree: " + std::to_string(op));
        }
    }

    if (!jsonExp.contains("child")) {
        builder.end();
        return 0;
    }

    const auto& childNode = jsonExp["child"];

    if (childNode.is_string()) {
        try {
            std::string childStr = childNode.get<std::string>();
            auto childArray = nlohmann::json::parse(childStr);

            if (childArray.is_array()) {
                for (const auto& child : childArray) {
                    initExpressionTree(builder, child, jsonLeaves);
                }
            } else {
                throw std::runtime_error("'child' parsed JSON is not an array");
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to parse 'child' string as JSON: " + std::string(e.what()));
        }
    }
    else if (childNode.is_array()) {
        for (const auto& child : childNode) {
            initExpressionTree(builder, child, jsonLeaves);
        }
    }
    else {
        throw std::runtime_error("'child' must be a string or array");
    }

    builder.end();
    return 0;
}

void ParseJson(nlohmann::json &jsonConfig,
               std::list<std::string>& includedColumnsList,
               std::shared_ptr<common::JulianGregorianRebase>& julianPtr,
               std::shared_ptr<common::PredicateCondition>& predicate,
               std::unique_ptr<::orc::SearchArgument>& searchArgument)
{
    if (jsonConfig.contains("expressionTree") && jsonConfig.contains("leaves")) {
        const auto& exprTree = jsonConfig["expressionTree"];
        const auto& leaves = jsonConfig["leaves"];

        std::unique_ptr<orc::SearchArgumentBuilder> builder = orc::SearchArgumentFactory::newBuilder();
        initExpressionTree(*builder, exprTree, leaves);
        auto sargBuilded = builder->build();
        searchArgument = std::unique_ptr<orc::SearchArgument>(sargBuilded.release());
    }

    if (jsonConfig.contains("tz") && jsonConfig.contains("switches") && jsonConfig.contains("diffs")) {
        julianPtr = common::BuildJulianGregorianRebase(jsonConfig);
    }

    if (jsonConfig.contains("vecPredicateCondition")) {
        predicate = common::BuildVecPredicateCondition(jsonConfig, includedColumnsList.size());
    }
}

nlohmann::json OptimizeJsonPredicate(
        const nlohmann::json& json_predicate,
        const std::unordered_set<std::string>& available_columns) {

    if (json_predicate.contains("field")) {
        std::string field_name = json_predicate["field"];

        if (available_columns.find(field_name) == available_columns.end()) {
            if (!json_predicate.contains("op")) {
                nlohmann::json false_expr;
                false_expr["op"] = static_cast<int>(omniruntime::reader::ParquetPredicateOperator::False);
                return false_expr;
            }

            int op_code = json_predicate["op"];
            auto predicate_op = static_cast<omniruntime::reader::ParquetPredicateOperator>(op_code);

            nlohmann::json constant_expr;
            if (predicate_op == omniruntime::reader::ParquetPredicateOperator::IsNull) {
                constant_expr["op"] = static_cast<int>(omniruntime::reader::ParquetPredicateOperator::True);
            } else {
                constant_expr["op"] = static_cast<int>(omniruntime::reader::ParquetPredicateOperator::False);
            }
            return constant_expr;
        }
        return json_predicate;
    }

    nlohmann::json optimized_json = json_predicate;

    if (json_predicate.contains("left") && json_predicate.contains("right")) {
        optimized_json["left"] = OptimizeJsonPredicate(json_predicate["left"], available_columns);
        optimized_json["right"] = OptimizeJsonPredicate(json_predicate["right"], available_columns);
    } else if (json_predicate.contains("predicate")) {
        optimized_json["predicate"] = OptimizeJsonPredicate(json_predicate["predicate"], available_columns);
    }

    return optimized_json;
}

void ParsePredicateJson(nlohmann::json &jsonConfig, std::shared_ptr<common::PredicateCondition>& predicate,
                        arrow::compute::Expression* pushedFilterArray, std::list<std::string>* includedColumnsList)
{
    if (pushedFilterArray != nullptr && jsonConfig.contains("expressionTree")) {
        std::unordered_set<std::string> available_columns(includedColumnsList->begin(), includedColumnsList->end());

        const nlohmann::json& expr_json = jsonConfig["expressionTree"];

        nlohmann::json optimized_json = OptimizeJsonPredicate(expr_json, available_columns);

        auto parse_result = omniruntime::reader::ParseToArrowExpression(optimized_json);
        if (!parse_result.ok()) {
            throw OmniException(parse_result.status().ToString().c_str());
        }

        *pushedFilterArray = parse_result.MoveValueUnsafe();
    }

    auto new_time_rebase = common::BuildTimeRebaseInfo(jsonConfig);
    if (jsonConfig.contains("vecPredicateCondition")) {
        int32_t columnCount = (includedColumnsList != nullptr) ? includedColumnsList->size() : 0;
        predicate = common::BuildVecPredicateConditionWithRebase(jsonConfig, columnCount, std::move(new_time_rebase));
    }
}

void ReaderOptions::ParseEnhanceJson(const std::string &enhancementJson)
{
    enhancementJson_ = std::make_shared<nlohmann::json>(nlohmann::json::parse(enhancementJson));

    if (enhancementJson_->contains("ugi")) {
        ugiString_ = enhancementJson_->at("ugi").get<std::string>();
    }

    int64_t offset = 0;
    if (enhancementJson_->contains("offset") && enhancementJson_->at("offset").is_number()) {
        offset = enhancementJson_->at("offset").get<int64_t>();
    }

    int64_t length = 0;
    if (enhancementJson_->contains("length") && enhancementJson_->at("length").is_number()) {
        length = enhancementJson_->at("length").get<int64_t>();
    }

    if (enhancementJson_->contains("includedColumns") && enhancementJson_->at("includedColumns").is_string()) {
        std::string colsStr = enhancementJson_->at("includedColumns").get<std::string>();
        auto columns = ParseColumnNamesString(colsStr);
        for (const auto& col : columns) {
            includedColumnsList_.push_back(col);
        }
    }

    // T0 (filter-while-decode) global switch: injected from Gluten Spark config via enhancementJson,
    // default false.
    if (enhancementJson_->contains("filterWhileDecode") && enhancementJson_->at("filterWhileDecode").is_boolean()) {
        enableFilterWhileDecode_ = enhancementJson_->at("filterWhileDecode").get<bool>();
    }
}

void ReaderOptions::ParsePredicate()
{
    ParseJson(*enhancementJson_, includedColumnsList_, julianPtr_, predicatePtr_, searchArgument_);
    if (batchLen_ > 0 && predicatePtr_ != nullptr) {
        predicatePtr_->init(batchLen_);
    }
}

void ReaderOptions::ParseParquetPredicate()
{
    ParsePredicateJson(*enhancementJson_, predicatePtr_, &parquetPushedFilterArray_, &includedColumnsList_);
    if (batchLen_ > 0 && predicatePtr_ != nullptr) {
        predicatePtr_->init(batchLen_);
    }
}

void ReaderOptions::ParseEnhanceJson(const std::string &enhancementJson, FileFormat format)
{
    enhancementJson_ = std::make_shared<nlohmann::json>(nlohmann::json::parse(enhancementJson));

    switch (format) {
        case FileFormat::ORC: {
            int64_t offset = 0;
            if (enhancementJson_->contains("offset") && enhancementJson_->at("offset").is_number()) {
                offset = enhancementJson_->at("offset").get<int64_t>();
            }

            int64_t length = 0;
            if (enhancementJson_->contains("length") && enhancementJson_->at("length").is_number()) {
                length = enhancementJson_->at("length").get<int64_t>();
            }

            if (enhancementJson_->contains("includedColumns") && enhancementJson_->at("includedColumns").is_string()) {
                std::string colsStr = enhancementJson_->at("includedColumns").get<std::string>();
                auto columns = ParseColumnNamesString(colsStr);
                for (const auto& col : columns) {
                    includedColumnsList_.push_back(col);
                }
            }

            if (enhancementJson_->contains("allColumns") && enhancementJson_->at("allColumns").is_string()) {
                std::string colsStr = enhancementJson_->at("allColumns").get<std::string>();
                auto columns = ParseColumnNamesString(colsStr);
                for (const auto& col : columns) {
                    allColumnsList_.push_back(col);
                }
            }
            break;
        }
        case FileFormat::PARQUET: {
            timeRebaseInfo_ = common::BuildTimeRebaseInfo(*enhancementJson_);

            std::vector<std::string> includedColumns;
            if (enhancementJson_->contains("includedColumns") && enhancementJson_->at("includedColumns").is_string()) {
                std::string colsStr = enhancementJson_->at("includedColumns").get<std::string>();
                includedColumns = ParseColumnNamesString(colsStr);
            }
            includedColumnsList_.assign(includedColumns.begin(), includedColumns.end());
            SetParquetIncludedColumns(std::move(includedColumns));
            break;
        }
        default: {
            throw std::runtime_error("Unsupported format");
        }
    }
    if (enhancementJson_->contains("ugi")) {
        ugiString_ = enhancementJson_->at("ugi").get<std::string>();
    }

    // T0 (filter-while-decode) global switch: injected from Gluten Spark config via enhancementJson,
    // default false.
    if (enhancementJson_->contains("filterWhileDecode") && enhancementJson_->at("filterWhileDecode").is_boolean()) {
        enableFilterWhileDecode_ = enhancementJson_->at("filterWhileDecode").get<bool>();
    }
}
}

