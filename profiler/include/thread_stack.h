//
// Created by nemchenko on 20.4.2017
//
#pragma once

#include <inttypes.h>
#include <cstdlib>
#include <cstring>
#include <iterator>

typedef uint64_t ptr_t;

#pragma pack(1)
struct thread_stack {
    enum {
        e_stack_length = 128
    };

    uint32_t stack_size() const {
        return _length * sizeof(ptr_t);
    }

    bool encode(uint8_t *& first, uint8_t * last) const {
        uint8_t const * self = reinterpret_cast<uint8_t const *>(this);
        size_t sz_to_write = thread_stack::e_hdr_stack_size + stack_size();

        if (int32_t(sz_to_write) <= std::distance(first, last)) {
            memcpy(first, self, sz_to_write);
            first += sz_to_write;
            return true;
        }

        return false;
    }

    volatile pid_t _tid = 0;
    uint32_t _length;
    uint64_t _ts;
    ptr_t _stack[e_stack_length];

    enum {
        e_hdr_stack_size = sizeof(thread_stack::_tid) + sizeof(thread_stack::_length) + sizeof(thread_stack::_ts)
    };
};
#pragma pack()

