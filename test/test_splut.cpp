#include <cstdint>
#include <fmt/format.h>
#include <libOTe/TwoChooseOne/ConfigureCode.h>
#include <libOTe/TwoChooseOne/TcoOtDefines.h>
#include <libOTe/Tools/Coproto.h>
#include <random>
#include "utils/io_utils.h"
#include "GC/semihonest.h"
#include "OT/splut.h"
#include "utils/ArgMapping/ArgMapping.h"
#include "utils/ubuntu_terminal_colors.h"
#include <libOTe/Tools/Tools.h>

using namespace sci;
using namespace std;
using namespace oc;

bool verbose = true;
int party = 1;
int batch_size = 256;
int lut_bitlength = 16;
int num_threads = 1;
string address = "127.0.0.1";
int port = 32000;
std::random_device rand_div;
std::mt19937 generator(rand_div());
sci::NetIO* io;

int main(int argc, char **argv) {
  ArgMapping amap;

  amap.arg("r", party, "Role of party: ALICE/SERVER = 1; BOB/CLIENT = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("l", lut_bitlength, "Bit Length");
  amap.arg("s", batch_size , "Batch Size");
  amap.arg("t", num_threads , "#Threads");
  amap.arg("ip", address, "IP Address of server (ALICE)");
  amap.parse(argc, argv);

  cout << fmt::format("Testing SPLUT with parameters bit_length={}, batch_size={}, num_threads={}", lut_bitlength, batch_size, num_threads) << endl;

  auto ip = address+":"+std::to_string(port + 1);
  // test(ip);
  start_timing("Setup");
  auto setup = SPLUT_setup(batch_size, lut_bitlength, lut_bitlength, party, ip, num_threads);
  auto duration_ms = end_timing("Setup", true);
  cout << "Setup complete. Byte transferred = " << setup.bytes_transferred << " Bytes. " << endl;
  
	io = new NetIO(party == ALICE ? nullptr : address.c_str(), port, true);
	setup_semi_honest(io, party);

  auto lut_size = 1ULL << lut_bitlength;
	vector<uint32_t> lut(lut_size);
  for (int i = 0; i < lut.size(); i++) {
    lut[i] = rand() % lut_size;
  }
  
  vector<uint32_t> plain_queries(batch_size);
  vector<uint32_t> input(batch_size);
  uint32_t mask;
  if (party == ALICE) {
    vector<uint32_t> input_client(batch_size);
    for (int i = 0; i < batch_size; i++) {
      plain_queries[i] = rand() % lut_size;
      input_client[i] = rand() % lut_size;
      input[i] = plain_queries[i] ^ input_client[i];
    }
    io->send_data(input_client.data(), input_client.size() * sizeof(uint32_t));
  } else {
    io->recv_data(input.data(), input.size() * sizeof(uint32_t));
  }
  io->flush();
  for (int i = 0; i < batch_size; i++) {
    assert (input[i] < lut_size);
  }

  start_record(io, "SPLUT+");
  auto ret = SPLUT(setup, lut, input, lut_bitlength, lut_bitlength, party, io, ip, num_threads);
  end_record(io, "SPLUT+");

  // Verify
  if (party == ALICE) {
    std::vector<uint32_t> ret_client(batch_size);
    io->recv_data(ret_client.data(), ret_client.size() * sizeof(uint32_t));
    for (int i = 0; i < batch_size; i++) {
        auto recovered_result = ret[i] ^ ret_client[i];
        if (recovered_result != (lut[plain_queries[i]])) {
          std::cerr << fmt::format("Mismatch at {} : {} ^ {} != {}", i, ret[i], ret_client[i], recovered_result, lut[plain_queries[i]]) << std::endl;
          cout << RED << "SPLUT+ test failed" << RESET << endl;
          exit(1);
        }
    }
  } else {
    io->send_data(ret.data(), ret.size() * sizeof(uint32_t));
  }

  cout << GREEN << "SPLUT+ test passed" << RESET << endl;

}
