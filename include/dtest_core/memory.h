/*
 * Copyright (c) 2021 Noah Orensa.
 * Licensed under the MIT license. See LICENSE file in the project root for details.
*/

#pragma once

#include <dtest_core/call_stack.h>
#include <mutex>
#include <map>
#include <unordered_map>
#include <string>
#include <dlfcn.h>

namespace dtest {

class Memory {

    friend class Sandbox;

private:
    std::mutex _mtx;

    struct Allocation {
        size_t size;
        CallStack callstack;
    };

    volatile bool _track = false;
    std::unordered_map<void *, Allocation> _blocks;
    std::map<char *, Allocation> _orderedBlocks;

    size_t _allocateSize = 0;
    size_t _freeSize = 0;
    size_t _allocateCount = 0;
    size_t _freeCount = 0;
    size_t _maxAllocate = 0;
    size_t _maxAllocateCount = 0;

    static thread_local size_t _locked;

    inline bool _enter() {
        if (! _track || _locked) return false;
        ++_locked;
        return true;
    }

    inline void _exit() {
        --_locked;
    }

    bool _canTrackAlloc(const CallStack &callstack);

    bool _canTrackDealloc(const CallStack &callstack);

public:

    static void reinitialize(void *handle = RTLD_DEFAULT);

    Memory();

    inline void trackActivity(bool val) {
        _track = val;
    }

    inline void lock() {
        ++_locked;
    }

    inline void unlock() {
        --_locked;
    }

    void track(void *ptr, size_t size);

    void track_mapped(char *ptr, size_t size);

    void retrack(void *oldPtr, void *newPtr, size_t newSize);

    void retrack_mapped(char *oldPtr, size_t oldSize, char *newPtr, size_t newSize);

    void remove(void *ptr);

    void remove_mapped(char *ptr, size_t size);

    void clear();

    void resetMaxAllocation() {
        _maxAllocate = 0;
    }

    std::string report();
};

}  // end namespace dtest
