/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#pragma once

#include <vector>
#include "type/data_type.h"
#include "reader/common/Filter.h"

namespace omniruntime {
namespace codegen {

// Describes the filtering and value extraction for a
// SelectiveColumnReader. This is owned by the TableScan Operator and
// is passed to SelectiveColumnReaders at construction.  This is
// mutable by readers to reflect filter order and other adaptation.
class ScanSpec {
public:
    enum class ColumnType : int8_t {
        kRegular, // Read from file or constant
        kRowIndex, // Row number in the file starting from 0
        kComposite, // A struct with all children not read from file
    };

    static constexpr type::column_index_t
    kNoChannel = ~0;
    static constexpr const char *kMapKeysFieldName = "keys";
    static constexpr const char *kMapValuesFieldName = "values";
    static constexpr const char *kArrayElementsFieldName = "elements";

    explicit ScanSpec(const std::string &name) : fieldName_(name) {}

    void setColumnType(ColumnType value)
    {
        columnType_ = value;
    }

    // Name of the value in its container, i.e. field name in struct or
    // string key in map. Not all fields of 'this' apply in list/map
    // value cases but the overhead is manageable, the space taken is
    // less than the Subfield path that will in any case exist for each
    // separately named list/map element.
    const std::string &fieldName() const
    {
        return fieldName_;
    }

    void setProjectOut(bool projectOut)
    {
        projectOut_ = projectOut;
    }

    bool projectOut() const
    {
        return projectOut_;
    }

    // Position in the RowVector returned by the top level scan. Applies
    // only to children of the root struct where projectOut_ is true.
    type::column_index_t channel() const
    {
        return channel_;
    }

    void setChannel(type::column_index_t channel)
    {
        channel_ = channel;
    }

    const std::vector <std::shared_ptr<ScanSpec>> &children() const
    {
        return children_;
    }

    /// Returns the ScanSpec corresponding to 'name'. Creates it if needed without
    /// any intermediate level.
    ScanSpec *getOrCreateChild(const std::string &name)
    {
        if (auto it = this->childByFieldName_.find(name);
            it != this->childByFieldName_.end()) {
            return it->second;
        }
        this->children_.push_back(std::make_unique<ScanSpec>(name));
        auto *child = this->children_.back().get();
        this->childByFieldName_[child->fieldName()] = child;
        return child;
    }

    // Add a field to this ScanSpec, with content projected out.
    ScanSpec *addField(const std::string &name, type::column_index_t channel)
    {
        auto child = getOrCreateChild(name);
        child->setProjectOut(true);
        child->setChannel(channel);
        return child;
    }

    // Add a field and its children recursively to this ScanSpec, all projected
    // out.
    ScanSpec *addFieldRecursively(
        const std::string &name,
        const type::DataType &type,
        type::column_index_t channel)
    {
        auto *child = addField(name, channel);
        child->addAllChildFields(type);
        return child;
    }

    // Add all child fields on the type recursively to this ScanSpec, all
    // projected out.
    void addAllChildFields(const type::DataType &type)
    {
        switch (type.GetId()) {
            case type::OMNI_ROW: {
                const type::RowType &rowType = static_cast<const type::RowType &>(type);
                for (auto i = 0; i < rowType.size(); ++i) {
                    addFieldRecursively(rowType.nameOf(i), *rowType.childAt(i), i);
                }
                break;
            }
            default:
                break;
        }
    }

    /// Invoke the function provided on each node of the ScanSpec tree.
    template<typename F>
    void visit(const type::DataType &type, F &&f);

    void setFlatMapAsStruct(bool value)
    {
        isFlatMapAsStruct_ = value;
    }

    /// Disable stats based filter reordering.
    void disableStatsBasedFilterReorder()
    {
        disableStatsBasedFilterReorder_ = true;
        for (auto &child: children_) {
            child->disableStatsBasedFilterReorder();
        }
    }

    ScanSpec *childByName(const std::string &name) const
    {
        auto it = childByFieldName_.find(name);
        if (it == childByFieldName_.end()) {
            return nullptr;
        }
        return it->second;
    }

    // Pushed filter; orthogonal to projectOut_ (filter-only / filter+project).
    void setFilter(::common::FilterPtr filter)
    {
        filter_ = std::move(filter);
    }

    ::common::Filter *filter() const
    {
        return filter_.get();
    }

    bool hasFilter() const
    {
        return filter_ != nullptr;
    }

    // Whether any leaf filter exists in the subtree (capability gate: no pushable → skip new path).
    bool hasAnyLeafFilter() const
    {
        if (filter_ != nullptr) {
            return true;
        }
        for (const auto &child : children_) {
            if (child->hasAnyLeafFilter()) {
                return true;
            }
        }
        return false;
    }

private:
    bool disableStatsBasedFilterReorder_{false};

    ::common::FilterPtr filter_;

    std::unordered_map<std::string, ScanSpec *> childByFieldName_;
    // Column name if this is a struct mamber. String key if this
    // describes an operation on a map value.
    std::string fieldName_;
    // Ordinal position of the extracted value in the containing
    // RowVector. Set only when this describes a struct member.
    type::column_index_t channel_ = kNoChannel;

    bool projectOut_ = false;

    ColumnType columnType_ = ColumnType::kRegular;

    std::vector <std::shared_ptr<ScanSpec>> children_;
    // Read-only copy of children, not subject to reordering. Used when
    // asynchronously constructing reader trees for read-ahead, while
    // 'children_' is reorderable by a running scan.
    std::vector<ScanSpec *> stableChildren_;

    // Used only for bulk reader to project flat map features.
    std::vector <std::string> flatMapFeatureSelection_;

    // This node represents a flat map column that need to be read as struct,
    // i.e. in table schema it is a MAP, but in result vector it is ROW.
    bool isFlatMapAsStruct_ = false;
};

template<typename F>
void ScanSpec::visit(const type::DataType &type, F &&f)
{
    f(type, *this);
    const type::RowType &rowType = static_cast<const type::RowType &>(type);
    for (auto &child: children_) {
        child->visit(*rowType.childAt(child->channel()), std::forward<F>(f));
    }
}

}
} // namespace
