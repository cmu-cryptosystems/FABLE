#ifndef BATCHLUT_IO_UTILS_H__
#define BATCHLUT_IO_UTILS_H__

#include "GC/emp-sh2pc.h"
#include <string>

using namespace std::chrono;
using std::cout, std::endl;
struct recordinfo {
  uint64_t counter;
  uint64_t num_rounds;
  uint64_t num_ands;
  std::chrono::time_point<std::chrono::system_clock> start_time;
};

void start_record(sci::NetIO* io, std::string tag);
void end_record(sci::NetIO* io, std::string tag);

void start_timing(string prefix);
double end_timing(string prefix, bool verbose = true);

#endif