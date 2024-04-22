#ifndef BATCHLUT_IO_UTILS_H__
#define BATCHLUT_IO_UTILS_H__

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utils/net_io_channel.h>
#include <coproto/Socket/AsioSocket.h>

using namespace std::chrono;
using std::cout, std::endl;
struct recordinfo {
  uint64_t counter;
  uint64_t num_rounds;
  std::chrono::time_point<std::chrono::system_clock> start_time;
};

void start_record(sci::NetIO* io, std::string tag);
void end_record(sci::NetIO* io, std::string tag);

void start_record(coproto::AsioSocket &chl, std::string tag);
void end_record(coproto::AsioSocket &chl, std::string tag);

void start_timing(std::string prefix);
double end_timing(std::string prefix, bool verbose = true);

#endif