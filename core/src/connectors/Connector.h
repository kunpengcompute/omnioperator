/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * We modify this part of the code based on Velox to enable native execution of the Omni Spark operator,
 * modify the corresponding code.
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#pragma once

#include <cstring>
#include <utility>
#include "util/config/ConfigBase.h"
#include <type/data_type.h>
#include <unordered_map>
#include "reader/common/Filter.h"
#include "codegen/expr_evaluator.h"
#include "util/format.h"

// Forward-declare OmniTaskMemoryPool so the connector interface can accept a
// per-task pool without pulling in the heavy memory_pool header here. The pool
// is threaded TableScanOperator → Connector → DataSource → SplitReader to fix
// the cross-pool-system bridge at SplitReader.cpp:79 (partition columns +
// missing-field vectors).
namespace omniruntime::mem {
class OmniTaskMemoryPool;
} // namespace omniruntime::mem

namespace omniruntime::connector {

class DataSource;

// A split represents a chunk of data that a connector should load and return
// as a RowVectorPtr, potentially after processing pushdowns.
struct ConnectorSplit {
    const std::string connectorId;
    const int64_t splitWeight{0};
    const bool cacheable{true};

    explicit ConnectorSplit(
        const std::string &_connectorId, int64_t _splitWeight = 0, bool _cacheable = true)
        : connectorId(_connectorId), splitWeight(_splitWeight), cacheable(_cacheable) {}

    virtual ~ConnectorSplit() {}
};

class ColumnHandle {
public:
    virtual ~ColumnHandle() = default;

    virtual const std::string& name() const = 0;
};

using ColumnHandlePtr = std::shared_ptr<const ColumnHandle>;

class ConnectorTableHandle {
public:
    explicit ConnectorTableHandle(std::string connectorId)
        : connectorId_(std::move(connectorId)) {}

    virtual ~ConnectorTableHandle() = default;

    const std::string& connectorId() const
    {
        return connectorId_;
    }

    // Returns true if the connector table handle supports index lookup.
    virtual bool supportsIndexLookup() const
    {
        return false;
    }

private:
    const std::string connectorId_;
};

using ConnectorTableHandlePtr = std::shared_ptr<const ConnectorTableHandle>;

// Represents a request for writing to connector
class ConnectorInsertTableHandle {
public:
    virtual ~ConnectorInsertTableHandle() {}

    // Whether multi-threaded write is supported by this connector. Planner uses
    // this flag to determine number of drivers.
    virtual bool supportsMultiThreading() const
    {
        return false;
    }

    virtual std::string toString() const = 0;
};

// Represents the commit strategy for writing to connector.
enum class CommitStrategy {
    // No more commit actions are needed.
    kNoCommit,
    // Task level commit is needed.
    kTaskCommit
};

// Return a string encoding of the given commit strategy.
std::string commitStrategyToString(CommitStrategy commitStrategy);


// Writes data received from table writer operator into different partitions
// based on the specific table layout. The actual implementation doesn't need
// to be thread-safe.
class DataSink {
public:
    struct Stats {
        uint64_t numWrittenBytes{0};
        uint32_t numWrittenFiles{0};
        uint64_t writeIOTimeUs{0};
        uint64_t numCompressedBytes{0};
        uint64_t recodeTimeNs{0};
        uint64_t compressionTimeNs{0};

        bool empty() const;

        std::string toString() const;
    };

    virtual ~DataSink() = default;

    // Add the next data (vector) to be written. This call is blocking.
    virtual void appendData(vec::VectorBatch input) = 0;

    // Called after all data has been added via possibly multiple calls to
    // appendData() This function finishes the data procesing like sort all the
    // added data and write them to the file writer. The finish might take long
    // time so it returns false to yield in the middle of processing. The
    // function returns true if it has processed all data. This call is blocking.
    virtual bool finish() = 0;

    // Called once after all data has been added via possibly multiple calls to
    // appendData(). The function returns the metadata of written data in string
    // form. We don't expect any appendData() calls on a closed data sink object.
    virtual std::vector<std::string> close() = 0;

    // Called to abort this data sink object and we don't expect any appendData()
    // calls on an aborted data sink object.
    virtual void abort() = 0;

    // Returns the stats of this data sink.
    virtual Stats stats() const = 0;
};

class DataSource {
public:
    static constexpr int64_t kUnknownRowSize = -1;
    virtual ~DataSource() = default;

    // Add split to process, then call next multiple times to process the split.
    // A split must be fully processed by next before another split can be
    // added. Next returns nullptr to indicate that current split is fully
    // processed.
    virtual void addSplit(std::shared_ptr<ConnectorSplit> split, uint64_t size) = 0;

    // Process a split added via addSplit. Returns nullptr if split has been
    // fully processed. Returns std::nullopt and sets the 'future' if started
    // asynchronous work and needs to wait for it to complete to continue
    // processing. The caller will wait for the 'future' to complete before
    // calling 'next' again.
    virtual std::optional<vec::VectorBatch *> next(uint64_t size) = 0;

    // Invoked by table scan close to cancel any inflight async operations
    // running inside the data source. This is the best effort and the actual
    // connector implementation decides how to support the cancellation if
    // needed.
    virtual void cancel() {}

    virtual uint64_t getCompletedRows() const
    {
        return 0;
    }

    virtual uint64_t getCompletedBytes() const
    {
        return 0;
    }

    virtual void addDynamicFilter(type::column_index_t /*channel*/, ::common::FilterPtr /*filter*/) {}
};

// Collection of context data for use in a DataSource, IndexSource or DataSink.
// One instance of this per DataSource and DataSink. This may be passed between
// threads but methods must be invoked sequentially. Serializing use is the
// responsibility of the caller.
class ConnectorQueryCtx {
public:
    ConnectorQueryCtx(
        const config::ConfigBase *sessionProperties,
        const op::SpillConfig *spillConfig,
        op::PrefixSortConfig prefixSortConfig,
        const std::string &queryId,
        const std::string &taskId,
        const std::string &planNodeId,
        int driverId,
        const std::string &sessionTimezone,
        bool adjustTimestampToTimezone = false)
        : sessionProperties_(sessionProperties),
        spillConfig_(spillConfig),
        prefixSortConfig_(prefixSortConfig),
        scanId_(Format("{}.{}", taskId, planNodeId)),
        queryId_(queryId),
        taskId_(taskId),
        driverId_(driverId),
        planNodeId_(planNodeId),
        sessionTimezone_(sessionTimezone),
        adjustTimestampToTimezone_(adjustTimestampToTimezone) {}

    const config::ConfigBase* sessionProperties() const
    {
        return sessionProperties_;
    }

    const op::SpillConfig* spillConfig() const
    {
        return spillConfig_;
    }

    const op::PrefixSortConfig& prefixSortConfig() const
    {
        return prefixSortConfig_;
    }

    // This is a combination of task id and the scan's PlanNodeId. This is an
    // id that allows sharing state between different threads of the same
    // scan. This is used for locating a scanTracker, which tracks the read
    // density of columns for prefetch and other memory hierarchy purposes.
    const std::string& scanId() const
    {
        return scanId_;
    }

    const std::string queryId() const
    {
        return queryId_;
    }

    const std::string& taskId() const
    {
        return taskId_;
    }

    int driverId() const
    {
        return driverId_;
    }

    const std::string& planNodeId() const
    {
        return planNodeId_;
    }

    // Session timezone used for reading Timestamp. Stores a string with the
    // actual timezone name. If the session timezone is not set in the
    // QueryConfig, it will return an empty string.
    const std::string& sessionTimezone() const
    {
        return sessionTimezone_;
    }

    // Whether to adjust Timestamp to the timeZone obtained through
    // sessionTimezone(). This is used to be compatible with the
    // old logic of Presto.
    bool adjustTimestampToTimezone() const
    {
        return adjustTimestampToTimezone_;
    }
    
    bool selectiveNimbleReaderEnabled() const
    {
        return selectiveNimbleReaderEnabled_;
    }

    void setSelectiveNimbleReaderEnabled(bool value)
    {
        selectiveNimbleReaderEnabled_ = value;
    }

private:
    const config::ConfigBase* const sessionProperties_;
    const op::SpillConfig* const spillConfig_;
    const op::PrefixSortConfig prefixSortConfig_;
    const std::string scanId_;
    const std::string queryId_;
    const std::string taskId_;
    const int driverId_;
    const std::string planNodeId_;
    const std::string sessionTimezone_;
    const bool adjustTimestampToTimezone_;
    bool selectiveNimbleReaderEnabled_{false};
};

class ConnectorMetadata;

class Connector {
public:
    explicit Connector(const std::string& id) : id_(id) {}

    virtual ~Connector() = default;

    const std::string& connectorId() const
    {
        return id_;
    }

    // Returns true if this connector would accept a filter dynamically
    // generated during query execution.
    virtual bool canAddDynamicFilter() const
    {
        return false;
    }

    // Per-task pool is an optional trailing parameter. nullptr falls back to
    // omniruntime::mem::GetMemoryPool() (legacy global path) inside the
    // concrete impl; production callers (TableScanOperator::getSplit()) thread
    // Operator::pool() here.
    virtual std::unique_ptr <DataSource> createDataSource(
        const type::RowTypePtr &outputType,
        const std::shared_ptr <ConnectorTableHandle> &tableHandle,
        const std::unordered_map <
        std::string,
        std::shared_ptr<connector::ColumnHandle>> &columnHandles,
        omniruntime::mem::OmniTaskMemoryPool *taskPool = nullptr) = 0;

    // Returns true if addSplit of DataSource can use 'dataSource' from
    // ConnectorSplit in addSplit(). If so, TableScan can preload splits
    // so that file opening and metadata operations are off the Driver'
    // thread.
    virtual bool supportsSplitPreload()
    {
        return false;
    }

    // Returns true if the connector supports index lookup, otherwise false.
    virtual bool supportsIndexLookup() const
    {
        return false;
    }

private:
    const std::string id_;
};

class ConnectorFactory {
public:
    explicit ConnectorFactory(const char* name) : name_(name) {}

    virtual ~ConnectorFactory() = default;

    const std::string& connectorName() const
    {
        return name_;
    }

    virtual std::shared_ptr<Connector> newConnector(
            const std::string& id,
            std::shared_ptr<const config::ConfigBase> config) = 0;

private:
    const std::string name_;
};

// Adds a factory for creating connectors to the registry using connector
// name as the key. Throws if factor with the same name is already present.
// Always returns true. The return value makes it easy to use with
// FB_ANONYMOUS_VARIABLE.
bool registerConnectorFactory(std::shared_ptr<ConnectorFactory> factory);

// Returns true if a connector with the specified name has been registered,
// false otherwise.
bool hasConnectorFactory(const std::string& connectorName);

// Unregister a connector factory by name.
// Returns true if a connector with the specified name has been
// unregistered, false otherwise.
bool unregisterConnectorFactory(const std::string& connectorName);

// Returns a factory for creating connectors with the specified name.
// Throws if factory doesn't exist.
std::shared_ptr<ConnectorFactory> getConnectorFactory(
    const std::string& connectorName);

// Adds connector instance to the registry using connector ID as the key.
// Throws if connector with the same ID is already present. Always returns
// true. The return value makes it easy to use with FB_ANONYMOUS_VARIABLE.
bool registerConnector(std::shared_ptr<Connector> connector);

// Removes the connector with specified ID from the registry. Returns true
// if connector was removed and false if connector didn't exist.
bool unregisterConnector(const std::string& connectorId);

// Returns a connector with specified ID. Throws if connector doesn't
// exist.
std::shared_ptr<Connector> getConnector(const std::string& connectorId);

// Returns a map of all (connectorId -> connector) pairs currently
// registered.
const std::unordered_map<std::string, std::shared_ptr<Connector>>& getAllConnectors();
} // namespace omniruntime::connector
