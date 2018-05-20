//
// Created by nemchenko on 28.4.2017
//
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <ctime>
#include <time.h>

#include "process.h"

volatile bool proceed = true;

using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;

void sig_handler(int signum) {
    profiler::LOG << "handler: " << signum << std::endl;
    proceed = false;

    // TODO: ??
}

void print_help() {
    std::cerr << "\n usage: ./profile_analyzer [options] [COMMAND [args for command]]\n\n"
                 "-p, pid number\n"
                 "-f, frequency (default = 100)\n"
                 "-o, output file path (default = 'pdb.data')" << std::endl;
}

int init_process_from_args(profiler::process & process, int argc, char *argv[]) {
    pid_t pid = -1;
    uint32_t freq = 100;
    std::string output_file_path = "pdb.data";

    int i = 1;
    for (; i < argc; ++i) {
        if ('-' == argv[i][0]) {
            switch (argv[i][1]) {
                case 'p':
                    pid = atoi(argv[++i]);
                    break;

                case 'f':
                    freq = uint32_t(atoi(argv[++i]));
                    break;

                case 'o':
                    output_file_path = std::string(argv[++i]);
                    break;

                case 'h':
                    print_help();
                    return -1;

                default:
                    break;
            }
        } else {
            break;
        }
    }

    profiler::LOG << "count samples in second: " << freq << std::endl;
    if (-1 != pid) {
        return process.init(pid, freq, output_file_path);
    }

    if (i < argc) {
        return process.init(argv + i, freq, output_file_path);
    }

    print_help();
    return -1;
}

profiler::process process;

int main(int argc, char *argv[]) {
    std::signal(SIGTERM, sig_handler);
    std::signal(SIGINT, sig_handler);

    auto start = high_resolution_clock::now();

    time_t cur_time = high_resolution_clock::to_time_t(start);
    profiler::LOG << "start: " << ctime(&cur_time) << std::endl;

    if (-1 == init_process_from_args(process, argc, argv)) {
        profiler::LOG << "can't init" << std::endl;
        return EXIT_FAILURE;
    }

    size_t i = 0;
    for (; proceed && !process._deinited; ++i) {
//        auto start_stacktrace = high_resolution_clock::now();
        if (-1 == process.dump_stacktrace()) {
            break;
        }

//        auto end_stacktrace = high_resolution_clock::now();

//        profiler::LOG << "time stacktrace: " << duration_cast<nanoseconds>(end_stacktrace - start_stacktrace).count() << std::endl;

        if (!process._exist_stopped_thread) {
            std::this_thread::sleep_for(milliseconds(process._cnt_milli_sec_for_sleep));
        }
    }
    proceed = false;

    auto end = high_resolution_clock::now();
    cur_time = high_resolution_clock::to_time_t(end);
    profiler::LOG << ctime(&cur_time) << std::endl;
    profiler::LOG << "end: " << duration_cast<milliseconds>(end - start).count() * 1.0 / 1000 << std::endl;
    profiler::LOG << "cnt_stacktrace: " << process._cnt_interrupts << std::endl;
    profiler::LOG << "cnt attempts: " << i << std::endl;

    process.deinit();

    return 0;
}
