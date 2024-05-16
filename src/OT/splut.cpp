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
#include <unistd.h>

uint64_t total_system_memory = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);

inline uint32_t highestPowerOfTwoIn(uint32_t x) {
  return 0x80000000 >> __builtin_clz(x);
}

std::vector<uint32_t> SPLUT(const std::vector<uint32_t> &T, std::vector<uint32_t> x, uint64_t l_out, uint64_t l_in, int party, coproto::AsioSocket& chl, uint64_t numThreads) {

  start_record(chl, "Initialization");
  PRNG prng(sysRandomSeed());

  auto lut_size = 1ULL << l_in;
  uint32_t in_mask = (1ULL << l_in) - 1;
  uint32_t out_mask = (1ULL << l_out) - 1;
  int batch_size = x.size();

  std::vector<uint32_t> z(batch_size);
  std::vector<uint32_t> u(batch_size);
  uint64_t chunk_size = std::min<uint64_t>(batch_size, highestPowerOfTwoIn(std::min<uint64_t>(std::numeric_limits<u32>::max(), total_system_memory * 4) / (lut_size * l_out))); // allow v_serialize to use at most half of the memory
  BitVector v_serialized(chunk_size * lut_size * l_out);
  
  for (int b = 0; b < batch_size; b++) {
    z[b] = prng.get<uint32_t>() & out_mask;
  }
  end_record(chl, "Initialization");

  if (party == sci::ALICE) {
    start_record(chl, "Setup Phase");
    auto result = SilentOT_1_out_of_N_server_Compressed(batch_size, numThreads, chl, l_in);
    end_record(chl, "Setup Phase");
    
    start_record(chl, "Online Phase");
    cp::sync_wait(chl.recv(u));
    
    for (int chunk_idx = 0; chunk_idx < batch_size / chunk_size; chunk_idx++) {
      # pragma omp parallel for collapse(2) num_threads(numThreads)
      for (int c = 0; c < chunk_size; c++) {
        for (int i = 0; i < lut_size; i++) {
          int b = chunk_idx * chunk_size + c;
          auto m = result.get_mask(b, i ^ u[b]);
          uint32_t v = (T[i ^ x[b]] ^ m ^ z[b]) & out_mask;
          std::bitset<POWER_MAX> blk(v);
          for (int j = 0; j < l_out; j++) {
            v_serialized[c * (lut_size * l_out) + i * l_out + j] = blk[j];
          }
        }
      }
      cp::sync_wait(chl.send(v_serialized));
    }
    end_record(chl, "Online Phase");

  } else { // party == BOB
    start_record(chl, "Setup Phase");
    auto [ms, s] = SilentOT_1_out_of_N_client(batch_size, numThreads, chl, l_in);
    end_record(chl, "Setup Phase");

    start_record(chl, "Online Phase");
    for (int b = 0; b < batch_size; b++) {
      u[b] = x[b] ^ s[b];
    }
    cp::sync_wait(chl.send(u));
    
    for (int chunk_idx = 0; chunk_idx < batch_size / chunk_size; chunk_idx++) {
      cp::sync_wait(chl.recv(v_serialized));
      for (int c = 0; c < chunk_size; c++) {
        int b = chunk_idx * chunk_size + c;
        std::bitset<POWER_MAX> blk;
        for (int j = 0; j < l_out; j++) {
          blk[j] = v_serialized[c * (lut_size * l_out) + x[b] * l_out + j];
        }
        uint32_t v = blk.to_ulong();
        z[b] = (v ^ ms[b].get<uint32_t>()[0]) & out_mask;
      }
    }
    end_record(chl, "Online Phase");
  }
  
  cp::sync_wait(chl.flush());
  return z;
}