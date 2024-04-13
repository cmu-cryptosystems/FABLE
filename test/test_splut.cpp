#include <cstdint>
#include <memory>
#include <random>
// #include "OT/split-kkot.h"
#include "kkot.h"
#include "GC/io_utils.h"

using namespace sci;
using namespace std;

bool verbose = true;
std::unique_ptr<NetIO> io;
int party = 1;
int batch_size = 1;
int lut_bitlength = 8;
string address = "127.0.0.1";
int port = 32000;
std::random_device rand_div;
// std::mt19937 generator(rand_div());
std::mt19937 generator(0);

template <typename T>
void lookup_table(T **spec, T *x, T *y, int32_t size, int32_t bw_x, int32_t bw_y, SplitKKOT<NetIO>* kkot) {
  if (party == sci::ALICE) {
    assert(x == nullptr);
    assert(y == nullptr);
  } else { // party == sci::BOB
    assert(spec == nullptr);
  }
  // assert(bw_x <= 8 && bw_x >= 1);
  int32_t T_size = sizeof(T) * 8;
  assert(bw_y <= T_size);

  T mask_x = (bw_x == T_size ? -1 : ((1ULL << bw_x) - 1));
  T mask_y = (bw_y == T_size ? -1 : ((1ULL << bw_y) - 1));
  uint64_t N = 1 << bw_x;

  if (party == sci::ALICE) {
    // PRG128 prg;
    T **data = new T *[size];
    for (int i = 0; i < size; i++) {
      data[i] = new T[N];
      for (uint64_t j = 0; j < N; j++) {
        data[i][j] = spec[i][j];
      }
    }

    kkot->send(data, size, bw_y);

    for (int i = 0; i < size; i++)
      delete[] data[i];
    delete[] data;
  } else { // party == sci::BOB
    uint8_t *choice = new uint8_t[size];
    for (int i = 0; i < size; i++) {
      choice[i] = x[i] & mask_x;
    }
    kkot->recv(y, choice, size, bw_y);

    delete[] choice;
  }
}

auto SPLUT(const vector<uint64_t> &spec_vec, vector<uint64_t> input, int l_out, int l_in) {
  
  std::unique_ptr<SplitKKOT<NetIO>> kkot(new SplitKKOT<NetIO>(party, io.get(), 1 << l_in));
  cout << "kkot initialized" << endl;

  assert (input.size() == batch_size);

  uint64_t* ret = new uint64_t[batch_size];
  uint64_t x_mask = (1ULL << l_in) - 1;
  uint64_t ret_mask = (1ULL << l_out) - 1;
  if (party == ALICE) {
    uint64_t **spec;
    spec = new uint64_t *[batch_size];
    PRG128 prg;
    prg.random_data(ret, batch_size * sizeof(uint64_t));
    for (int i = 0; i < batch_size; i++) {
      spec[i] = new uint64_t[spec_vec.size()];
      ret[i] &= ret_mask;
      for (int j = 0; j < spec_vec.size(); j++) {
        int idx = (input[i] ^ j) & x_mask;
        spec[i][j] = (spec_vec[idx] ^ ret[i]) & ret_mask;
      }
    }
    lookup_table<uint64_t>(spec, nullptr, nullptr, batch_size, l_in, l_out, kkot.get());

    for (int i = 0; i < batch_size; i++)
      delete[] spec[i];
    delete[] spec;
  } else {
    lookup_table<uint64_t>(nullptr, input.data(), ret, batch_size, l_in, l_out, kkot.get());
  }
  return ret;
}


int main(int argc, char **argv) {
  ArgMapping amap;

  amap.arg("r", party, "Role of party: ALICE/SERVER = 1; BOB/CLIENT = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("ip", address, "IP Address of server (ALICE)");
  amap.parse(argc, argv);

  io = std::unique_ptr<NetIO>(new NetIO(party == ALICE ? nullptr : address.c_str(), port + GC_PORT_OFFSET, true));

  auto lut_size = 1ULL << lut_bitlength;
	vector<uint64_t> lut(lut_size);
  for (int i = 0; i < lut.size(); i++) {
    lut[i] = rand() % lut_size;
  }
  
  vector<uint64_t> plain_queries(batch_size);
  vector<uint64_t> input(batch_size);
  uint64_t mask;
  for (int i = 0; i < batch_size; i++) {
    if (party == ALICE) {
      plain_queries[i] = i; //rand() % lut_size;
      mask = rand() % lut_size;
      input[i] = plain_queries[i] ^ mask;
      io->send_data(&mask, sizeof(mask));
    } else {
      io->recv_data(&mask, sizeof(mask));
      input[i] = mask;
    }
  }

  start_record(io.get(), "SPLUT");
  auto ret = SPLUT(lut, input, lut_bitlength, lut_bitlength);
  end_record(io.get(), "SPLUT");

  // Verify
  for (int i = 0; i < batch_size; i++) {
    if (party == ALICE) {
      io->recv_data(&mask, sizeof(mask));
      auto recovered_result = ret[i] ^ mask;
      if (recovered_result != (lut[plain_queries[i]])) {
        std::cerr << "Mismatch at " << i << " " << recovered_result << " " << lut[plain_queries[i]] << std::endl;
      }
    } else {
      io->send_data(&ret[i], sizeof(ret[i]));
    }
  }

  cout << GREEN << "SPLUT test passed" << RESET << endl;

}
