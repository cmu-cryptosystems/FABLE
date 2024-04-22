#include "splut.h"
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

Setup SPLUT_setup(uint32_t batch_size, int l_out, int l_in, int party, string ip, uint64_t numThreads) {

  Setup results;
  auto chl = cp::asioConnect(ip, party == sci::ALICE);

  if (party == sci::ALICE) {
    results.server = SilentOT_1_out_of_N_server_Compressed(batch_size, numThreads, chl, l_in);
  } else { // party == BOB
    results.client = SilentOT_1_out_of_N_client(batch_size, numThreads, chl, l_in);
  }
  results.bytes_transferred = chl.bytesSent() + chl.bytesReceived();

  return results;
}

std::vector<uint32_t> SPLUT(Setup& setup, const std::vector<uint32_t> &T, std::vector<uint32_t> x, int l_out, int l_in, int party, sci::NetIO* io, string ip, uint64_t numThreads) {

  start_record(io, "Initialization");
  PRNG prng(sysRandomSeed());

  auto lut_size = 1ULL << l_in;
  uint32_t in_mask = (1ULL << l_in) - 1;
  uint32_t out_mask = (1ULL << l_out) - 1;
  int batch_size = x.size();

  std::vector<uint32_t> z(batch_size);
  std::vector<uint32_t> u(batch_size);
  uint64_t chunk_size = std::min<uint64_t>(batch_size, highestPowerOfTwoIn(total_system_memory * 8 * 0.95 / (lut_size * l_out))); // allow v_serialize to use at most half of the memory
  cout << "Num Chunks = " << batch_size / chunk_size << endl;
  BitVector v_serialized(chunk_size * lut_size * (uint64_t)l_out); 
  
  for (int b = 0; b < batch_size; b++) {
    z[b] = prng.get<uint32_t>() & out_mask;
  }
  end_record(io, "Initialization");

  if (party == sci::ALICE) {
    auto result = setup.server;
    
    start_record(io, "Online Phase");
    io->recv_data(u.data(), u.size() * sizeof(uint32_t));
    
    for (int chunk_idx = 0; chunk_idx < batch_size / chunk_size; chunk_idx++) {
      // int c_range = std::min(batch_size - chunk_idx * chunk_size, chunk_size);
      # pragma omp parallel for if (numThreads > 1) collapse(2)
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
      auto to_sent = v_serialized.getSpan<uint64_t>();
      io->send_data(to_sent.data(), to_sent.size() * sizeof(uint64_t));
    }
    end_record(io, "Online Phase");

  } else { // party == BOB
    auto [ms, s] = setup.client;

    start_record(io, "Online Phase");
    // The vector u doesn't need that much space to store. For simplicity we just use vector<u32> for it, as it is not the bottleneck. 
    for (int b = 0; b < batch_size; b++) {
      u[b] = x[b] ^ s[b];
    }
    io->send_data(u.data(), u.size() * sizeof(uint32_t));
    
    for (int chunk_idx = 0; chunk_idx < batch_size / chunk_size; chunk_idx++) {
      auto to_recv = v_serialized.getSpan<uint64_t>();
      io->recv_data(to_recv.data(), to_recv.size() * sizeof(uint64_t));
      
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
    end_record(io, "Online Phase");
  }

  return z;
}