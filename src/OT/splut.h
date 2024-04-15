#include <vector>
#include <cstdint>
#include "coproto/Socket/AsioSocket.h"

std::vector<uint32_t> SPLUT(const std::vector<uint32_t> &T, std::vector<uint32_t> x, int l_out, int l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads = 1);