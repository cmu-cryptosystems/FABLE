#include <cryptoTools/Common/Defines.h>
#include <cstdint>
#include <fmt/format.h>
#include <libOTe/TwoChooseOne/ConfigureCode.h>
#include <libOTe/TwoChooseOne/TcoOtDefines.h>
#include "OT/silent_ot.h"
#include "utils/constants.h"

std::vector<uint32_t> SPLUT(const std::vector<uint32_t> &T, std::vector<uint32_t> x, int l_out, int l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads) {

  PRNG prng(sysRandomSeed());

  auto lut_size = 1ULL << l_in;
  int batch_size = x.size();

  std::vector<uint32_t> z(batch_size);
  std::vector<uint32_t> u(batch_size);
  std::vector<uint32_t> v(lut_size);
  
  for (int b = 0; b < batch_size; b++) {
    z[b] = prng.get<uint32_t>() % lut_size;
  }

  if (party == sci::ALICE) {
    auto [m] = SilentOT_1_out_of_N_server(batch_size, numThreads, chl, l_in);
    
    cp::sync_wait(chl.recv(u));
    
    for (int b = 0; b < batch_size; b++) {
      for (int i = 0; i < lut_size; i++) {
        v[i] = T[i ^ x[b]] ^ m[b][i ^ u[b]].get<uint32_t>()[0] ^ z[b];
      }
      cp::sync_wait(chl.send(v));
    }

  } else { // party == BOB
    auto [ms, s] = SilentOT_1_out_of_N_client(batch_size, numThreads, chl, l_in);

    for (int b = 0; b < batch_size; b++) {
      u[b] = x[b] ^ s[b];
    }
    cp::sync_wait(chl.send(u));
    
    for (int b = 0; b < batch_size; b++) {
      cp::sync_wait(chl.recv(v));
      z[b] = v[x[b]] ^ ms[b].get<uint32_t>()[0];
    }
  }
  return z;
}