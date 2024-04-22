#include "io_utils.h"
#include <chrono>
#include <fmt/format.h>


std::map<string, recordinfo> record; 
std::map<string, time_point<system_clock, nanoseconds>> start_timestamps;

void start_record(sci::NetIO* io, std::string tag) {
    recordinfo info;
    info.counter = io->counter;
    info.num_rounds = io->num_rounds;
    info.start_time = std::chrono::system_clock::now();
    record[tag] = info;
}

void end_record(sci::NetIO* io, std::string tag) {
    auto end_time = std::chrono::system_clock::now();
    auto start_time = record[tag].start_time;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << fmt::format("{}: \n    elapsed {} ms,\n    sent {} Bytes in {} rounds. ", tag, duration.count(), io->counter - record[tag].counter, (io->num_rounds - record[tag].num_rounds)) << std::endl;
    record.erase(tag);
}

void start_record(coproto::AsioSocket &chl, std::string tag) {
    recordinfo info;
    info.counter = chl.bytesSent() + chl.bytesReceived();
    info.num_rounds = 0;
    info.start_time = std::chrono::system_clock::now();
    record[tag] = info;
}
void end_record(coproto::AsioSocket &chl, std::string tag) {
    auto end_time = std::chrono::system_clock::now();
    auto start_time = record[tag].start_time;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << fmt::format("{}: \n    elapsed {} ms,\n    sent {} Bytes. ", tag, duration.count(), chl.bytesSent() + chl.bytesReceived() - record[tag].counter) << std::endl;
    record.erase(tag);
}

void start_timing(string prefix) {
    start_timestamps[prefix] = high_resolution_clock::now();
}

double end_timing(string prefix, bool verbose) {
    auto end = high_resolution_clock::now();
    auto duration_init = duration_cast<nanoseconds>(end - start_timestamps[prefix]);
    if (verbose)
        std::cout << fmt::format("{}: {} ms. ", prefix, duration_init.count() * 1.0 / 1e6) << std::endl;
    start_timestamps.erase(prefix);
    return duration_init.count() * 1.0 / 1e6;
}