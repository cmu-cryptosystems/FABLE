#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Common/Defines.h>
#include <cstdint>
#include <bitset>
#include <fmt/format.h>
#include <libOTe/TwoChooseOne/ConfigureCode.h>
#include <libOTe/TwoChooseOne/TcoOtDefines.h>
#include "OT/silent_ot.h"
#include "utils/constants.h"
#include "utils/io_utils.h"

std::vector<uint32_t> SPLUT(const std::vector<uint32_t> &T, std::vector<uint32_t> x, int l_out, int l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads) {

  PRNG prng(sysRandomSeed());

  auto lut_size = 1ULL << l_in;
  uint32_t in_mask = (1ULL << l_in) - 1;
  uint32_t out_mask = (1ULL << l_out) - 1;
  int batch_size = x.size();

  std::vector<uint32_t> z(batch_size);
  std::vector<uint32_t> u(batch_size);
  BitVector v_serialized(batch_size * lut_size * l_out);
  
  for (int b = 0; b < batch_size; b++) {
    z[b] = prng.get<uint32_t>() & out_mask;
  }

  if (party == sci::ALICE) {
    auto [m] = SilentOT_1_out_of_N_server(batch_size, numThreads, chl, l_in);
    
    start_record(chl, "U Communication");
    cp::sync_wait(chl.recv(u));
    end_record(chl, "U Communication");
    
    start_record(chl, "Compute V");
    # pragma omp parallel for if (numThreads > 1)
    for (int b = 0; b < batch_size; b++) {
      for (int i = 0; i < lut_size; i++) {
        uint32_t v = (T[i ^ x[b]] ^ m[b][i ^ u[b]].get<uint32_t>()[0] ^ z[b]) & out_mask;
        std::bitset<POWER_MAX> blk(v);
        for (int j = 0; j < l_out; j++) {
          v_serialized[b * (lut_size * l_out) + i * l_out + j] = blk[j];
        }
      }
    }
    end_record(chl, "Compute V");

    start_record(chl, "V Communication");
    cp::sync_wait(chl.send(v_serialized));
    end_record(chl, "V Communication");

  } else { // party == BOB
    auto [ms, s] = SilentOT_1_out_of_N_client(batch_size, numThreads, chl, l_in);

    start_record(chl, "Compute u");
    for (int b = 0; b < batch_size; b++) {
      u[b] = x[b] ^ s[b];
    }
    end_record(chl, "Compute u");
    
    start_record(chl, "Communication");
    cp::sync_wait(chl.send(u));
    
    cp::sync_wait(chl.recv(v_serialized));
    end_record(chl, "Communication");

    start_record(chl, "Compute v");
    # pragma omp parallel for if (numThreads > 1)
    for (int b = 0; b < batch_size; b++) {
      std::bitset<POWER_MAX> blk;
      for (int j = 0; j < l_out; j++) {
        blk[j] = v_serialized[b * (lut_size * l_out) + x[b] * l_out + j];
      }
      uint32_t v = blk.to_ulong();
      z[b] = (v ^ ms[b].get<uint32_t>()[0]) & out_mask;
    }
    end_record(chl, "Compute v");
  }
  
  cp::sync_wait(chl.flush());
  return z;
}