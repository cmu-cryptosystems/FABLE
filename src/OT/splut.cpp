#include <cstdint>
#include <fmt/format.h>
#include <libOTe/TwoChooseOne/ConfigureCode.h>
#include <libOTe/TwoChooseOne/TcoOtDefines.h>
#include "OT/silent_ot.h"
#include "utils/constants.h"

uint32_t SPLUT(const std::vector<uint32_t> &T, uint32_t x, int l_out, int l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads) {

  auto lut_size = 1ULL << l_in;

  assert (x < 1 << l_in);
  assert (T.size() == lut_size);
  uint32_t z;
  uint32_t u;
  std::vector<uint32_t> v(lut_size);

  if (party == sci::ALICE) {
    auto [m] = SilentOT_1_out_of_N_server(numThreads, chl, l_in);
    
    cp::sync_wait(chl.recv(u));
    z = rand() % lut_size;

    for (int i = 0; i < lut_size; i++) {
      v[i] = T[i ^ x] ^ m[i ^ u].get<uint32_t>()[0] ^ z;
    }
    cp::sync_wait(chl.send(v));

  } else { // party == BOB
    auto [ms, s] = SilentOT_1_out_of_N_client(numThreads, chl, l_in);

    u = x ^ s;
    // std::vector<uint32_t> wrapper{u};
    cp::sync_wait(chl.send(u));
    
    cp::sync_wait(chl.recv(v));
    z = v[x] ^ ms.get<uint32_t>()[0];
  }
  return z;
}