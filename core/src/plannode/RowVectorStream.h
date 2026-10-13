/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "compute/ResultIterator.h"
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"

namespace omniruntime {

class ValueStreamNode final : public PlanNode {
public:
    ValueStreamNode(const PlanNodeId &id, const DataTypesPtr &outputType, std::shared_ptr<ResultIterator> iterator)
        : PlanNode(id),
          outputType_(outputType),
          iterator_(std::move(iterator)) {}

    const DataTypesPtr &OutputType() const override
    {
        return outputType_;
    }

    const std::vector<PlanNodePtr> &Sources() const override
    {
        return kEmptySources_;
    }

    ResultIterator *Iterator() const
    {
        return iterator_.get();
    }

    std::string_view Name() const override
    {
        return "ValueStream";
    }

private:
    const DataTypesPtr outputType_;
    std::shared_ptr<ResultIterator> iterator_;
    const std::vector<PlanNodePtr> kEmptySources_;
};

class RowVectorStream {
public:
    RowVectorStream(ResultIterator *iterator, const DataTypesPtr &outputType)
        : outputType_(outputType), iterator_(iterator) {}

    bool HasNext()
    {
        if (finished_) {
            return false;
        }
        bool hasNext = iterator_->HasNext();
        if (!hasNext) {
            finished_ = true;
        }
        return hasNext;
    }

    // Convert arrow batch to row vector and use new output columns
    VectorBatch *Next() const
    {
        if (finished_) {
            return nullptr;
        }
        VectorBatch *cb = nullptr;

        // We are leaving Velox task execution and are probably entering Spark code through JNI. Suspend the current
        // driver to make the current task open to spilling.

        cb = iterator_->Next();
        return cb;
    }

private:
    const DataTypesPtr outputType_;
    ResultIterator *iterator_;
    bool finished_{false};
};

class ValueStream : public op::Operator {
public:
    // Pool-bound ctor chains Operator(pool).
    ValueStream(omniruntime::mem::OmniTaskMemoryPool *pool,
        std::shared_ptr<const ValueStreamNode> valueStreamNode)
        : op::Operator(pool)
    {
        ResultIterator *itr = valueStreamNode->Iterator();
        rvStream_ = std::make_unique<RowVectorStream>(itr, valueStreamNode->OutputType());
    }

    int32_t AddInput(VectorBatch *vecBatch) override
    {
        OMNI_THROW("runtime_error:", "ValueStream_ERROR");
    }

    int32_t GetOutput(VectorBatch **result) override
    {
        if (finished_) {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        if (rvStream_->HasNext()) {
            *result = rvStream_->Next();
            return (*result)->GetRowCount();
        } else {
            finished_ = true;
            return 0;
        }
    }

    bool IsFinished() const
    {
        return finished_;
    }

private:
    bool finished_ = false;
    std::unique_ptr<RowVectorStream> rvStream_;
};

class ValueStreamFactory : public OperatorFactory {
public:
    explicit ValueStreamFactory(std::shared_ptr<const ValueStreamNode> planNode)
        : planNode_(planNode) {}

    ~ValueStreamFactory() override = default;

    static ValueStreamFactory *CreateValueStreamFactory(std::shared_ptr<const ValueStreamNode> planNode)
    {
        return new ValueStreamFactory(planNode);
    }

    op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override
    {
        return new ValueStream(pool, planNode_);
    }

private:
    std::shared_ptr<const ValueStreamNode> planNode_;
};
}
