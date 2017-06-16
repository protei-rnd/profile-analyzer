#pragma once

#include <stdint.h>
#include <iostream>
#include <algorithm>
#include <cxxabi.h>

#include <QTreeWidgetItem>
#include "memory_pool.h"

typedef uint64_t ptr_t; // TODO: it can be uint32_t

#pragma pack(1)
struct thread_stack
{
    bool decode(const uint8_t* in_buf);

    pid_t _tid;
    uint32_t _length;
    uint64_t _ts;
    ptr_t _stack[0];
};
#pragma pack()

struct fn_desc
{
    fn_desc()
    {}

    fn_desc(uint64_t start_addr, std::string const & n, uint32_t size)
        : _start_addr(start_addr)
        , _name(n)
        , _size(size)
    { }

    uint64_t _start_addr;
    std::string _name;
    uint32_t _size = 0;
};


struct fn_desc_node;

typedef std::map<ptr_t, fn_desc_node *> fn_tree_node_t;

struct fn_desc_node
{
    fn_desc_node()
    { }

    fn_desc_node (std::string const & name, uint32_t size)
        : _name(name)
        , _size(size)
    { }

    void upd_ts(uint64_t ts) {
        _lts = std::min(_lts, ts);
        _uts = std::max(_uts, ts);
    }

    uint64_t _lts = std::numeric_limits<uint64_t>::max(); // lower bound
    uint64_t _uts = 0; // upper bound
    std::vector<uint64_t> _tss; // timestamps for leaves will be in asc order

    QTreeWidgetItem * _qt_item = nullptr;
    uint32_t _clipped_size = 0; // clipped size, within time bounds

    std::string _name;
    uint32_t _size = 0;
    fn_tree_node_t _children;
};

extern fn_desc_node * g_cur_root;
extern fn_desc_node * g_root;
extern fn_desc_node * g_root_reversed;
void init_fn_desc (char const * filename);
void read_db_file (char const * filename);
