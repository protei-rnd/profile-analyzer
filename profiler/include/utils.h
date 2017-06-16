//
// Created by nemchenko on 20.4.2017
//
#pragma once

#include <inttypes.h>
#include <sys/types.h>
#include <vector>
#include <functional>
#include <locale>
#include <algorithm>

#include "process.h"

long tkill(int tid, int sig);

void get_process_threads(pid_t pid, std::vector<profiler::tid_state> & tids);

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

