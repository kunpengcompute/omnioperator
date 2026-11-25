/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "thread_memory_trace.h"
#include "allocator.h"
#include "vector/vector.h"
#include "memory_trace.h"

namespace omniruntime::mem {
ThreadMemoryTrace::ThreadMemoryTrace()
{
    MemoryTrace *globalMemoryTrace = GetMemoryTrace();
    globalMemoryTrace->AddThreadMemoryTrace(this);
}

ThreadMemoryTrace::~ThreadMemoryTrace()
{
    MemoryTrace *globalMemoryTrace = GetMemoryTrace();
    globalMemoryTrace->SubThreadMemoryTrace(this);
}

void ThreadMemoryTrace::AddVectorMemory(uintptr_t ptr, int64_t size)
{
    vectorTraced.emplace(ptr, size);
#ifdef TRACE
    vectorTracedWithLog.emplace(ptr, std::make_pair(size, TraceUtil::GetStack()));
#endif
}

void ThreadMemoryTrace::RemoveVectorMemory(uintptr_t ptr, int64_t size)
{
    std::unordered_map<uintptr_t, int64_t>::iterator iter;
    if ((iter = vectorTraced.find(ptr)) != vectorTraced.end()) {
        if (iter->second != size) {
            auto message =
                "wrong vector size, alloc: " + std::to_string(iter->second) + ", free: " + std::to_string(size);
            throw exception::OmniException("Memory Trace Error", message);
        }
        vectorTraced.erase(ptr);
#ifdef TRACE
        vectorTracedWithLog.erase(ptr);
#endif
    } else {
        MemoryTrace *globalMemoryTrace = GetMemoryTrace();
        std::unordered_set<ThreadMemoryTrace *> set = globalMemoryTrace->GetThreadMemoryTraceSet();
        std::unordered_set<ThreadMemoryTrace *>::iterator traceIter;
        for (traceIter = set.begin(); traceIter != set.end() && *traceIter != this; ++traceIter) {
            if ((*traceIter)->vectorTraced.find(ptr) != (*traceIter)->vectorTraced.end()) {
                std::string message = "vector allocated by ThreadA, but freed by ThreadB.";
#ifdef TRACE
                auto originStack = (*traceIter)->vectorTracedWithLog.find(ptr)->second.second;
                auto currentStack = this->vectorTracedWithLog.find(ptr)->second.second;
                message.append("\n originStack is: " + originStack + "\n currentStack is: " + currentStack);
#endif
                (*traceIter)->vectorTraced.erase(ptr);
            }
        }
    }
}

void ThreadMemoryTrace::AddArenaMemory(uintptr_t ptr, int64_t size)
{
    arenaTraced.emplace(ptr, size);
#ifdef TRACE
    arenaTracedWithLog.emplace(ptr, std::make_pair(size, TraceUtil::GetStack()));
#endif
}

void ThreadMemoryTrace::RemoveArenaMemory(uintptr_t ptr, int64_t size)
{
    std::unordered_map<uintptr_t, int64_t>::iterator iter;
    if ((iter = arenaTraced.find(ptr)) != arenaTraced.end()) {
        if (iter->second != size) {
            auto message =
                "wrong arena size, alloc: " + std::to_string(iter->second) + ", free: " + std::to_string(size);
            throw exception::OmniException("Memory Trace Error", message);
        }
        arenaTraced.erase(ptr);
#ifdef TRACE
        arenaTracedWithLog.erase(ptr);
#endif
    } else {
        MemoryTrace *globalMemoryTrace = GetMemoryTrace();
        std::unordered_set<ThreadMemoryTrace *> set = globalMemoryTrace->GetThreadMemoryTraceSet();
        std::unordered_set<ThreadMemoryTrace *>::iterator traceIter;
        for (traceIter = set.begin(); traceIter != set.end() && *traceIter != this; ++traceIter) {
            if ((*traceIter)->arenaTraced.find(ptr) != (*traceIter)->arenaTraced.end()) {
                std::string message = "arena allocated by ThreadA, but freed by ThreadB.";
#ifdef TRACE
                auto originStack = (*traceIter)->arenaTracedWithLog.find(ptr)->second.second;
                auto currentStack = this->arenaTracedWithLog.find(ptr)->second.second;
                message.append("\n originStack is: " + originStack + "\n currentStack is: " + currentStack);
#endif
                (*traceIter)->arenaTraced.erase(ptr);
            }
        }
    }
}

std::unordered_map<uintptr_t, int64_t> ThreadMemoryTrace::GetVectorTraced() { return vectorTraced; }

std::unordered_map<uintptr_t, int64_t> ThreadMemoryTrace::GetArenaTraced() { return arenaTraced; }

std::unordered_map<uintptr_t, std::pair<int64_t, std::string>> ThreadMemoryTrace::GetVectorTracedWithLog()
{
    return vectorTracedWithLog;
}

std::unordered_map<uintptr_t, std::pair<int64_t, std::string>> ThreadMemoryTrace::GetArenaTracedWithLog()
{
    return arenaTracedWithLog;
}

/**
 * stack will be replaced if vector is created by jni
 *   */
void ThreadMemoryTrace::ReplaceVectorTracedLog(uintptr_t ptr, const std::string &stack)
{
    std::unordered_map<uintptr_t, std::pair<int64_t, std::string>>::iterator iter;
    if ((iter = vectorTracedWithLog.find(ptr)) != vectorTracedWithLog.end()) {
        iter->second.second = stack;
    } else {
        throw OmniException("Memory Trace Error", "vector create failed!");
    }
}

/**
 * check for memory leak in the thread.
 *   */
bool ThreadMemoryTrace::HasMemoryLeak()
{
#ifdef TRACE
    if (!vectorTracedWithLog.empty()) {
        // print leak stackLog
        std::unordered_map<uintptr_t, std::pair<int64_t, std::string>>::iterator iter;
        for (iter = vectorTracedWithLog.begin(); iter != vectorTracedWithLog.end(); ++iter) {
            std::cout << "vector leaked memory: " << iter->second.first << ", stack is: " << iter->second.second
                      << std::endl;
        }
    }

    if (!arenaTracedWithLog.empty()) {
        // print leak stackLog
        std::unordered_map<uintptr_t, std::pair<int64_t, std::string>>::iterator iter;
        for (iter = arenaTracedWithLog.begin(); iter != arenaTracedWithLog.end(); ++iter) {
            std::cout << "arena leaked memory: " << iter->second.first << ", stack is: " << iter->second.second
                      << std::endl;
        }
    }
#endif
    return !(vectorTraced.empty() && arenaTraced.empty());
}

/**
 * free memory of vector and arena when memory leak happened
 * */
void ThreadMemoryTrace::FreeLeakedMemory()
{
    std::unordered_map<uintptr_t, int64_t>::iterator iter;
    if (!vectorTraced.empty()) {
        std::vector<uintptr_t> vectors;
        for (iter = vectorTraced.begin(); iter != vectorTraced.end(); ++iter) {
            // copy the leaked vector record to avoid invalidating the unordered_map iterator.
            // Because the iterator is traversed at the same time as the map is updated.
            vectors.emplace_back(iter->first);
        }

        for (uint32_t i = 0; i < vectors.size(); ++i) {
            // free vector and buffer if vector type is varchar.
            delete reinterpret_cast<omniruntime::vec::BaseVector *>(vectors.at(i));
        }
    }

    if (!arenaTraced.empty()) {
        Allocator *allocator = Allocator::GetAllocator();
        std::unordered_map<uintptr_t, int64_t> arenas;
        for (iter = arenaTraced.begin(); iter != arenaTraced.end(); ++iter) {
            // copy the leaked arena record to avoid invalidating the unordered_map iterator.
            arenas.emplace(iter->first, iter->second);
        }

        for (iter = arenas.begin(); iter != arenas.end(); ++iter) {
            // free arena ptr
            allocator->Free(reinterpret_cast<uint8_t *>(iter->first), iter->second);
        }
    }
    Clear();
}

void ThreadMemoryTrace::Clear()
{
    vectorTraced.clear();
    arenaTraced.clear();

    vectorTracedWithLog.clear();
    arenaTracedWithLog.clear();
}
} // namespace omniruntime::mem
