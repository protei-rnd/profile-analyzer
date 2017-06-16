//
// Created by nemchenko on 19.4.2017
//
#pragma once

#include <libunwind.h>
#include <libunwind-ptrace.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

struct addr_space_scoped {
    addr_space_scoped() {
        _addrspace = unw_create_addr_space(&_UPT_accessors, 0);
    }

    explicit operator bool() const {
        return _addrspace != nullptr;
    }

    ~addr_space_scoped() {
        unw_destroy_addr_space(_addrspace);
    }

    unw_addr_space_t _addrspace;
};

struct upt_info_scoped {
    upt_info_scoped(pid_t pid)
        : _pid(pid)
    {
        _uptinfo = (struct UPT_info *)_UPT_create(_pid);
    }

    explicit operator bool() const {
        return _uptinfo != nullptr;
    }

    ~upt_info_scoped() {
        _UPT_destroy(_uptinfo);
    }

    pid_t _pid;
    struct UPT_info * _uptinfo;
};
