#ifndef FABLE_IO_UTILS_H__
#define FABLE_IO_UTILS_H__

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utils/net_io_channel.h>
#include <coproto/Socket/AsioSocket.h>

#include <execinfo.h>

using namespace std::chrono;
using std::cout, std::endl;
struct recordinfo {
  uint64_t counter;
  uint64_t num_rounds;
  std::chrono::time_point<std::chrono::system_clock> start_time;
};

void start_record(sci::NetIO* io, std::string tag);
void end_record(sci::NetIO* io, std::string tag, bool verbose = true);

void start_record(coproto::AsioSocket &chl, std::string tag);
void end_record(coproto::AsioSocket &chl, std::string tag, bool verbose = true);

void start_timing(std::string prefix);
double end_timing(std::string prefix, bool verbose = true);

inline void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

#endif