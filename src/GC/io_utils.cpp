#include "io_utils.h"
#include <chrono>
#include <fmt/format.h>

std::map<string, recordinfo> record; 

void start_record(sci::NetIO* io, string tag) {
    recordinfo info;
    info.counter = io->counter;
    info.num_rounds = io->num_rounds;
    info.num_ands = circ_exec->num_and();
    info.start_time = std::chrono::system_clock::now();
    record[tag] = info;
}

void end_record(sci::NetIO* io, string tag) {
    auto end_time = std::chrono::system_clock::now();
    auto start_time = record[tag].start_time;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << fmt::format("{}: \n    elapsed {} ms,\n    sent {:.3f} MB in {} rounds, \n    #AND={}. ", tag, duration.count(), (io->counter - record[tag].counter) / (1.0 * (1ULL << 20)), (io->num_rounds - record[tag].num_rounds), circ_exec->num_and() - record[tag].num_ands) << std::endl;
    record.erase(tag);
}