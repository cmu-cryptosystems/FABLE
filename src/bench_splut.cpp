#include "LUT_utils.h"

#include <fmt/format.h>
#include <libOTe/TwoChooseOne/ConfigureCode.h>
#include <libOTe/TwoChooseOne/TcoOtDefines.h>
#include <libOTe/Tools/Coproto.h>
#include <random>
#include "utils/io_utils.h"
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
int lut_type = 0;
int num_threads = 1;
string address = "127.0.0.1";
int port = 32000;
std::random_device rand_div;
std::mt19937 generator(rand_div());

int main(int argc, char **argv) {
  ArgMapping amap;

  amap.arg("r", party, "Role of party: ALICE/SERVER = 1; BOB/CLIENT = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("len", lut_bitlength, "Bit Length");
  amap.arg("s", batch_size , "Batch Size");
  amap.arg("l", lut_type , "0 = Random LUT; 1 = Gamma LUT");
  amap.arg("t", num_threads , "#Threads");
  amap.arg("ip", address, "IP Address of server (ALICE)");
  amap.parse(argc, argv);

  cout << fmt::format("Testing SPLUT with parameters bit_length={}, batch_size = {}, lut_type = {}, num_threads = {}", lut_bitlength, batch_size, lut_type, num_threads) << endl;

  auto ip = address+":"+std::to_string(port);
  auto chl = cp::asioConnect(ip, party == ALICE);

  auto lut_size = 1ULL << lut_bitlength;
	vector<uint64_t> lut = get_lut((LUTType)lut_type);
  
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
    cp::sync_wait(chl.send(input_client));
  } else {
    cp::sync_wait(chl.recv(input));
  }
  for (int i = 0; i < batch_size; i++) {
    assert (input[i] < lut_size);
  }

  auto ret = SPLUT(lut, input, lut_bitlength, lut_bitlength, party, chl, num_threads);
  cout << "Execution end. " << endl;

  // Verify
  if (party == ALICE) {
    std::vector<uint32_t> ret_client(batch_size);
    cp::sync_wait(chl.recv(ret_client));
    for (int i = 0; i < batch_size; i++) {
        auto recovered_result = ret[i] ^ ret_client[i];
        if (recovered_result != (lut[plain_queries[i]])) {
          cout << RED << "SPLUT+ test failed" << RESET << endl;
          std::cerr << "Mismatch at " << i << " " << recovered_result << " " << lut[plain_queries[i]] << std::endl;
          exit(1);
        }
    }
  } else {
    cp::sync_wait(chl.send(ret));
  }

  cout << GREEN << "SPLUT+ test passed" << RESET << endl;

}
