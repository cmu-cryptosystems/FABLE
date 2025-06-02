#include <map>
#include <vector>
#include <cstdint>
#include "coproto/Socket/AsioSocket.h"

std::vector<uint32_t> SPLUT(const std::vector<uint64_t> &T, std::vector<uint32_t> x, uint64_t l_out, uint64_t l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads = 1);
std::vector<uint32_t> SPLUT(std::map<uint64_t, uint64_t> &T, std::vector<uint32_t> x, uint64_t l_out, uint64_t l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads = 1);