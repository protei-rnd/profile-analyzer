//
// Created by nemchenko on 20.4.2017
//
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

#include <psc/directory.h>

#include "utils.h"

long tkill(int tid, int sig) {
    return syscall(SYS_tkill, tid, sig);
}

void get_process_threads(pid_t pid, std::vector<profiler::tid_state>& tids) {
    psc::filesystem::directory dir;

    tids.push_back(profiler::tid_state(pid));

    char task_path[1024] = {0};
    snprintf(task_path, 1023, "/proc/%d/task/", pid);

    if ( dir.start_file_enum(task_path) ) {
        do {
            char const * cur = dir.file_name();
            if ( 0 == strcmp(cur, "..") || 0 == strcmp(cur, ".") ) {

            } else {
                if (dir.is_directory()) {
                    int tid = atoi(cur);
                    if (0 < tid && tid != pid) {
                        tids.push_back(profiler::tid_state(tid));
                    }
                }
            }

        } while (dir.next_file());
    }
}
