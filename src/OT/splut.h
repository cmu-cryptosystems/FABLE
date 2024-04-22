#include <vector>
#include <cstdint>
#include "utils/net_io_channel.h"
#include "silent_ot.h"

struct Setup {
    SilentOTResultServer_N_Compressed server;
    SilentOTResultClient_N client;
    uint64_t bytes_transferred;
};

Setup SPLUT_setup(uint32_t batch_size, int l_out, int l_in, int party, string ip, uint64_t numThreads);

std::vector<uint32_t> SPLUT(Setup& setup, const std::vector<uint32_t> &T, std::vector<uint32_t> x, int l_out, int l_in, int party, sci::NetIO* io, string ip, uint64_t numThreads = 1);