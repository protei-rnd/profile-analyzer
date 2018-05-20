//
// Created by nemchenko on 5/10/17.
//

#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/time.h>
#include <iostream>
#include <set>

#include <psc/process.h>

#include "scoped_structures.h"
#include "utils.h"

#include "process.h"

using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;

namespace profiler {
    struct function_descriptor {
        function_descriptor()
        {}

        function_descriptor(uint64_t start_addr, std::string const & n, uint32_t size)
            : _start_addr(start_addr)
            , _name(n)
            , _size(size)
        { }

        bool operator<(function_descriptor const & rhs) const {
            return _start_addr < rhs._start_addr;
        }

        uint64_t _start_addr;
        std::string _name;
        uint32_t _size = 0;
    };

    typedef function_descriptor fn_desc_t;

    std::ofstream LOG("log.txt", std::ofstream::out | std::ofstream::trunc);

    process::process() {
        memset(&_tv, 0, sizeof(_tv));
    }

    void process::deinit() {
        if (!_deinited) {
            _pdb_stream.write(reinterpret_cast<char *>(_buffer), std::distance(_buffer, _bfirst));
            dump_maps();

            for (int i = int(_tids.size()) - 1; i >= 0; --i) {
                remove_tid(size_t(i));
            }

            _deinited = true;

            if (_from_path_inited) {
                tkill(_pid, SIGTERM);
                std::this_thread::sleep_for(std::chrono::seconds(1));

                tkill(_pid, SIGKILL);

                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::vector<tid_state> new_tids;
                get_process_threads(_pid, new_tids);
                for (int j = int(new_tids.size()) - 1; j >= 0; --j) {
                    waitpid(new_tids[j]._tid, nullptr, WNOHANG);
                }
            }
        }
    }

    pid_t process::init(char ** argv, uint32_t freq, std::string const & output_file) {
        _pid = fork();

        assert(_pid >= 0);

        if (_pid == 0) {
            std::this_thread::sleep_for(seconds(1));
            LOG << "before exec" << std::endl;
            execv(*argv, argv); // TODO: get program name
            LOG << "Maybe bad path: " << *argv << "; execl: " << strerror(errno) << std::endl;
            _exit(-1);
        }

        _from_path_inited = true;

        if (-1 != init(_pid, freq, output_file)) {
            int child_status;
            waitpid(_pid, &child_status, __WALL);
            if (WIFSTOPPED(child_status) && WSTOPSIG(child_status) == SIGTRAP) {
                _tids[0]._state = state::e_stopped;
                continue_thread(0);
            } else {
                LOG << "something go wrong; execv should cause sigtrap" << std::endl;
                return -1;
            }
        } else {
            return -1;
        }

        return _pid;
    }

    pid_t process::init(pid_t pid, uint32_t freq, std::string const & output_file) {
        _pdb_stream.open(output_file);

        _pid = pid;
        _cnt_milli_sec_for_sleep = 1000 / freq;
        _tids.push_back(tid_state(pid));

        if (-1 == attach_to(0)) {
//            deinit();
            return -1;
        }

        continue_thread(0);

        return _pid;
    }

    void parse_fn_desc(std::istream & functions_stream, uint64_t start_addr, uint64_t end_addr, std::vector<fn_desc_t> & fn_descs) {
        static std::set<fn_desc_t> s_fn_descs;
        s_fn_descs.clear();

        while (functions_stream) {
            uint64_t baddr;
            uint32_t bsize;
            std::string str_name;

            functions_stream >> std::hex >> baddr >> std::dec >> bsize;
            if (!functions_stream) {
                functions_stream.clear();
            }

            std::getline(functions_stream, str_name);

            if (0 == baddr /* || 0 == bsize */) {
                continue;
            }

            auto it_for_at = str_name.find('@');

            if (it_for_at != std::string::npos) {
                str_name = str_name.substr(0, it_for_at);
            }


            if (!(start_addr < baddr && baddr < end_addr)) {
                baddr += start_addr;
            }

            if (baddr >= end_addr) {
                continue;
            }

            s_fn_descs.insert(fn_desc_t(baddr, str_name, bsize));
        }

        if (!s_fn_descs.empty()) {
            // because of readelf gets wrong function size,
            // maybe use objdump without demangling?
            auto it_prev = s_fn_descs.begin();
            auto it_end = s_fn_descs.end();

            for (auto it = std::next(it_prev); it != it_end; ++it) {
                const_cast<fn_desc_t &>(*it_prev)._size = it->_start_addr - it_prev->_start_addr;
                it_prev = it;
            }
        }

        fn_descs.insert(fn_descs.end(), s_fn_descs.begin(), s_fn_descs.end());
    }

    void read_symbols_from_file(std::string const & path_to_file, uint64_t start_addr, uint64_t end_addr, std::vector<fn_desc_t> & fn_descs) {
        std::string cmd_for_generating_func_txt;
        std::string out_cmd;

        static std::string check_supporting_dyn_syms("readelf --help | grep \"dyn-syms\"");
        int rc = psc::create_process(check_supporting_dyn_syms, "", out_cmd);
        if (-1 == rc) {
            LOG << "check supporting dyn syms" << std::endl;
        }

        if (out_cmd.size() < 6) {
            cmd_for_generating_func_txt = "readelf -s --wide " + path_to_file + R"( | grep -E "FUNC|OBJECT|NOTYPE|WEAK" | awk '{print $2 " " $3 " " $8}' | c++filt )";
            LOG << std::endl << "you have old version of readelf" << std::endl << std::endl;;
        } else {
            cmd_for_generating_func_txt = "readelf -s --dyn-syms --wide " + path_to_file + R"( | grep -E "FUNC|OBJECT|NOTYPE|WEAK" | awk '{print $2 " " $3 " " $8}' | c++filt )";
        }

        rc = psc::create_process(cmd_for_generating_func_txt, "", out_cmd);

        if (-1 == rc) {
            LOG << "some problem in readelf or c++filt, check that utilities" << std::endl;
        }

        std::stringstream ss(out_cmd);
        parse_fn_desc(ss, start_addr, end_addr, fn_descs);
    }

    std::vector<fn_desc_t> read_maps(std::string const & filename) {
        std::ifstream maps_file(filename);
        LOG << "read_maps: filename: " << filename << std::endl;

        std::vector<fn_desc_t> fn_descs;
        if (maps_file) {
            size_t i = 0;
            while (maps_file) {
                std::string unused_str;
                std::string path_to_lib;
                std::string permissions;
                uint64_t start_addr;
                uint64_t end_addr;
                uint32_t inode;
                char unused_chr;

                maps_file >> std::hex >> start_addr
                          >> unused_chr
                          >> std::hex >> end_addr >> std::dec
                          >> permissions >> unused_str >> unused_str
                          >> inode;

                if (!maps_file) {
                    break;
                }

                std::getline(maps_file, path_to_lib);

                if (0 != inode && "r-xp" == permissions) {
                    trim(path_to_lib);

                    profiler::LOG << "lib: " << path_to_lib << " "
                                  << std::hex << start_addr << " " << end_addr << " "
                                  << std::dec << std::endl;

                    read_symbols_from_file(path_to_lib, start_addr, end_addr, fn_descs);
//                    std::cerr << path_to_lib << " " << fn_descs.size() << std::endl;
                }

                ++i;
            }
        } else {
            LOG << "error open file: " << filename << std::endl;
        }

        return fn_descs;
    }

    void process::dump_maps() {
        pid_t spliter = -1;
        _pdb_stream.write(reinterpret_cast<char *>(&spliter), sizeof(spliter));

        LOG << "/proc/" << _pid << "/maps" << std::endl;
        std::vector<fn_desc_t> descs = read_maps("/proc/" + std::to_string(_pid) + "/maps");

        LOG << "cnt symbols: " << descs.size() << std::endl;
        for (fn_desc_t & desc: descs) {
            // desc._name already contains ` `
            _pdb_stream << desc._start_addr << " " << desc._size /* << " " */ << desc._name << "\n";
        }
        _pdb_stream.close();

//        for (size_t i = 0; i < 10; ++i) {
//            std::cerr << std::hex << "0x" << descs[i]._start_addr << " " << std::dec << descs[i]._size << " " << descs[i]._name << std::endl;
//        }
//
//        std::cerr << "size: " << descs.size() << std::endl;
//        exit(0);


//        std::ifstream  src("/proc/" + std::to_string(_pid) + "/maps", std::ios::binary);
//        std::ofstream  dst("maps.txt", std::ios::binary);
//
//        bool res = bool(dst << src.rdbuf());
//        int x = 0;
    }

    int process::attach_to(size_t idx) {
        pid_t tid = _tids[idx]._tid;
        long rc = ptrace(PTRACE_ATTACH, tid, 0, 0);

        if (-1 == rc) {
            LOG << "can't attach: rc = " << rc << "; " << strerror(errno) << "; errno = " << errno << std::endl;

            if (!is_main_pid(idx)) {
                _tids[idx] = _tids.back();
                _tids.pop_back();
            }
        } else {
            if (is_main_pid(idx)) {
                int waitstatus;
                int ret = waitpid(tid, &waitstatus, __WALL);

                _tids[idx]._state = state::e_stopped;

                if (tid == ret && WIFSTOPPED(waitstatus)) {
                    if (-1 == (rc = ptrace(PTRACE_SETOPTIONS, tid, 0, PTRACE_O_TRACEEXIT))) {
                        LOG << "fail set options: errno = " << errno << "; str = " << strerror(errno) << std::endl;
                    }
                } else {
                    LOG << "can't stop process with pid: " << tid << std::endl;
                    rc = -1;
                }
            } else {
                _tids[idx]._state = state::e_stop_in_process;
            }
        }

        if (-1 != rc) {
            LOG << "succesfull attach to tid: " << tid << std::endl;
        }

        return int(rc);
    }

    int process::stopped(pid_t tid) {
        int waitstatus;
        int ret = waitpid(tid, &waitstatus, WNOHANG | __WALL);

        if (ret > 0) {
            return WSTOPSIG(waitstatus);
        } else {
            return 0;
        }
    }

    ret_stop process::stop(size_t idx, bool disable_signal) {
        pid_t tid = _tids[idx]._tid;

        int stopped_sig = stopped(tid);
        if (SIGSTOP == stopped_sig) {
            _tids[idx]._state = state::e_stopped;
        } else if (SIGTRAP == stopped_sig) {
            return ret_stop::e_before_exit;
        } else if (stopped_sig > 0) {
            LOG << "unexpected signal arrived to tid: " << tid << "; signal = " << stopped_sig << std::endl;

            ptrace(PTRACE_CONT, tid, 0, stopped_sig);
            _tids[idx]._state = state::e_idle;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            return ret_stop ::e_not_stopped;
        }

        switch (_tids[idx]._state) {
            case state::e_stopped:
                return ret_stop::e_stopped;

            case state::e_stop_in_process:
                return ret_stop::e_not_stopped;

            default:
                if (disable_signal) {
                    return ret_stop::e_not_stopped;
                }

                if (-1 == tkill(tid, SIGSTOP)) {
                    if (ESRCH == errno) {
                        return ret_stop::e_not_exist;
                    } else {
                        return ret_stop::e_not_stopped;
                    }
                }

                _tids[idx]._state = state::e_stop_in_process;
                if (stopped(tid)) {
                    _tids[idx]._state = state::e_stopped;

                    return ret_stop::e_stopped;
                }

                return ret_stop::e_not_stopped;
        }
    }

    int process::get_all_tids(std::vector<tid_state> & tids) {
        std::string out;
        tids.push_back(tid_state(_pid));

        if (0 == psc::create_process("ps -aeLf | grep -w " + std::to_string(_pid) + " | awk '{ print $2, $4 }'", "", out)) {
            std::stringstream ss(out);

            pid_t pid, tid;
            while (ss >> pid >> tid) {
                if (_pid == pid && tid != _pid) {
                    tids.push_back(tid_state(tid));
                }
            }

        } else {
            return -1;
        }

        return 0;
    }

    bool process::is_main_pid(size_t idx) {
        return _pid == _tids[idx]._tid;
    }

    ret_process_stack process::process_stack(size_t idx) {
        pid_t tid = _tids[idx]._tid;

        /* Create address space for little endian */
        addr_space_scoped addrspace;
        if (!addrspace) {
            LOG << "unw_create_addr_space failed" << std::endl;
            return ret_process_stack::e_fail;
        }

        upt_info_scoped uptinfo(tid);
        if (!uptinfo) {
            LOG << "_UPT_create failed" << std::endl;
            return ret_process_stack::e_fail;
        }

        errno = 0;
        unw_cursor_t cursor;
        int ret = unw_init_remote(&cursor, addrspace._addrspace, reinterpret_cast<void *>(uptinfo._uptinfo));
        if (ret < 0) {
            LOG << "unw_init_remote failed" << std::endl;
            return ret_process_stack::e_unw_init_failed;
        }

        thread_stack & cur_stack = _stacks[tid];
        cur_stack._length = 0;
        cur_stack._tid = tid;
        cur_stack._ts = uint64_t(_tv.tv_sec) * 1000000 + uint64_t(_tv.tv_usec);

        unw_word_t RIP;
        int step = 0;

        //LOG << "for tid: " << tid << std::endl;
        while (true) {
            if (unw_get_reg(&cursor, UNW_X86_64_RIP, &RIP) < 0) {
                LOG << "unw_get_reg RIP failed" << std::endl;
                return ret_process_stack::e_unw_get_reg_failed;
            }

            if (RIP > 0) {
                //LOG << std::hex << "0x" << RIP  << std::endl << std::dec;
                cur_stack._stack[cur_stack._length++] = RIP;
            }


            //unw_word_t offset;
            //char procname[512] = {0};
            //procname[0] = '\0';
            //unw_get_proc_name (&cursor, procname, sizeof(procname), &offset);
            //if (offset) {
                //size_t len = strlen(procname);
                //if (len >= sizeof(procname) - 32)
                    //len = sizeof(procname) - 32;
                //sprintf((char *)(procname + len), "+0x%016lx", (unsigned long)offset);
            //}
            //LOG <<  procname << std::endl;


            step++;
            ret = unw_step(&cursor);
            if (ret == 0) {
                break;
            } else if (ret < 0) {
                LOG << "unw_step failed. ret: " << ret << std::endl;
                return ret_process_stack::e_unw_step_failed;
            } else if (step > 32) {
                LOG << "Too deeply nested. Breaking out." << std::endl;
                return ret_process_stack::e_too_deeply_stack;
            }
        }
        //LOG << std::endl;

        if (cur_stack._length > 0) {
            if (!cur_stack.encode(_bfirst, _blast)) {
                _pdb_stream.write(reinterpret_cast<char *>(_buffer), std::distance(_buffer, _bfirst));
                _bfirst = _buffer;

                cur_stack.encode(_bfirst, _blast);
            }
        }

        _cnt_interrupts++;

        return ret_process_stack::e_success;
    }

    void process::remove_tid(size_t idx) {
        pid_t tid = _tids[idx]._tid;


        if (ret_stop::e_not_stopped == stop(idx, false) && state::e_stop_in_process == _tids[idx]._state)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (-1 == ptrace(PTRACE_DETACH, tid, 0, 0)) {
            LOG << "can't PTRACE_DETACH from tid: " << tid << "; " << strerror(errno) << std::endl;
        } else {
            LOG << "successfully detach from tid: " << tid << "; " << std::endl;
        }

        // sometimes, after PTRACE_DETACH process not continues, maybe due to asynchronous signals
        _tids[idx]._state = state::e_stopped;
        continue_thread(idx);

        _tids[idx] = _tids.back();
        _tids.pop_back();
    }

    int process::continue_thread(size_t idx) {
        if (state::e_idle != _tids[idx]._state) {
            if (-1 == ptrace(PTRACE_CONT, _tids[idx]._tid, 0, 0, 0)) {
                LOG << "can't PTRACE_CONT process: errno = " << errno << std::endl;
                return -1;
            } else {
                _tids[idx]._state = state::e_idle;
            }
        }

        return 0;
    }

    int process::dump_stacktrace() {
        gettimeofday(&_tv, 0);

        if (kill(_pid, 0) != 0) {
            LOG << "process not exist" << std::endl;
            return -1;
        }

//        auto start = high_resolution_clock::now();

        static std::vector<tid_state> new_tids;
        new_tids.clear();
        get_process_threads(_pid, new_tids);

//        auto end = high_resolution_clock::now();
//        LOG << "time get_tids: " << duration_cast<milliseconds>(end - start).count() << std::endl;

        for (tid_state new_tid: new_tids) {
            if (std::find(_tids.begin(), _tids.end(), new_tid) == _tids.end()) {
                _tids.push_back(new_tid);
                attach_to(_tids.size() - 1);
            }
        }

        bool new_exist_stopped_thread = false;
        for (size_t i = 0; i < _tids.size(); ++i) {
            ret_stop stop_state = stop(i, _exist_stopped_thread);

            switch (stop_state) {
                case ret_stop::e_not_exist:
                    if (is_main_pid(i)) {
                        return -1;
                    } else {
                        _tids[i] = _tids.back();
                        _tids.pop_back();
                        i--;
                        continue;
                    }

                case ret_stop::e_before_exit: {
                    deinit();
//                    ptrace(PTRACE_CONT, _pid, 0, 0);
//                    waitpid(_pid, nullptr, 0);

                    new_tids.clear();
                    get_process_threads(_pid, new_tids);

                    for (int j = int(new_tids.size()) - 1; j >= 0; --j) {
                        waitpid(new_tids[j]._tid, nullptr, WNOHANG);
                    }

                    return -1;
                }

                case ret_stop::e_parent_not_exist:
                    return -1;

                case ret_stop::e_not_stopped:
                    break;

                case ret_stop::e_stopped:
//                    LOG << "FOR PID: " << _pid << "; FOR TID: " << _tids[i]._tid << std::endl;
                    if (ret_process_stack::e_success != process_stack(i)) {
                        LOG << "process_stack failed. errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
                        LOG << "tid: " << _tids[i]._tid << std::endl;
                    }

                    continue_thread(i);
                    break;
            }

            if (state::e_stop_in_process == _tids[i]._state) {
                new_exist_stopped_thread = true;
            }
        }

        _exist_stopped_thread = new_exist_stopped_thread;

        return 0;
    }
}
