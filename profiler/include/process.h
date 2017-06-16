//
// Created by nemchenko on 28.4.2017
//
#pragma once

#include <vector>
#include <sys/types.h>
#include <limits>
#include <fstream>

#include <thread_stack.h>

namespace profiler {
    enum class state {
        e_idle,
        e_stop_in_process,
    //    e_cont_in_process,
        e_stopped
    };

    enum class ret_process_stack {
        e_success,
        e_too_deeply_stack,
        e_unw_init_failed,
        e_unw_get_reg_failed,
        e_unw_step_failed,
        e_fail
    };

    enum class ret_stop {
        e_stopped,
        e_not_stopped,
        e_parent_not_exist,
        e_not_exist,
        e_before_exit
    };

    struct tid_state {
        explicit tid_state(pid_t tid, state state_ = state::e_idle)
            : _tid(tid)
            , _state(state_)
        {}

        bool operator<(tid_state const & rhs) const {
            return _tid < rhs._tid;
        }

        bool operator==(tid_state const & rhs) const {
            return _tid == rhs._tid;
        }

        pid_t _tid;
        state _state;
    };


    struct process {
        process();

        void deinit();

        pid_t init(char ** argv, uint32_t freq, const std::string & output_file);

        pid_t init(pid_t pid, uint32_t freq, const std::string & output_file);

        void dump_maps();

        int attach_to(size_t idx);

        static int stopped(pid_t tid);

        ret_stop stop(size_t idx, bool disable_signal);

        int get_all_tids(std::vector<tid_state>& tids);

        bool is_main_pid(size_t idx);

        ret_process_stack process_stack(size_t idx);

        void remove_tid(size_t idx);

        int continue_thread(size_t idx);

        int dump_stacktrace();

        bool _exist_stopped_thread = false;
        std::vector<tid_state> _tids;
        pid_t _pid;

        enum {
            e_cnt_threads = std::numeric_limits<uint16_t>::max(),
            e_2KB = 65536
        };

        thread_stack _stacks[e_cnt_threads];
        timeval _tv;

        std::ofstream _pdb_stream;
        uint8_t _buffer[e_2KB] = {0};

        uint8_t * _bfirst = _buffer;
        uint8_t * const _blast = _bfirst + sizeof(_buffer);

        bool _deinited = false;
        int _cnt_interrupts = 0;

        uint32_t _cnt_milli_sec_for_sleep = 10; // == 1000 / sample frequency
        bool _from_path_inited = false;
    };

    extern std::ofstream LOG;
}
