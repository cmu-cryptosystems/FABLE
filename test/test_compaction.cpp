#include "GC/emp-sh2pc.h"
#include "custom_types.h"
#include "orcompact.h"
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <set>
#include <fmt/core.h>

using namespace sci;
using std::cout, std::endl;

int party, port = 8000, batch_size = 512;
int bitlength = 32;
NetIO *io_gc;

void test_compaction() {
	BitArray label(batch_size);
	IntegerArray B(batch_size);

	std::vector<int> in(batch_size);
	std::vector<bool> labelvec(batch_size);
	for (int i = 0, m = batch_size >> 1; i < batch_size; ++i) {
		if (rand() % (batch_size - i) < m) {
			labelvec[i] = 1;
			m--;
		} else {
			labelvec[i] = 0;
		}
	}

// First specify Alice's input
	for(int i = 0; i < batch_size; ++i) {
		label[i] = Bit(labelvec[i], ALICE);
	}

// Now specify Bob's input
	for(int i = 0; i < batch_size; ++i) {
		in[i] = rand()%(1ULL << 20);
		B[i] = Integer(32, in[i], BOB);
	}
	
    auto comm_start = io_gc->counter;
    auto round_start = io_gc->num_rounds;
	auto time_start = clock_start();
	
	auto constantArray = getConstantArray(batch_size, bitlength);
	auto compact_result = compact(label, B, batch_size, bitlength, constantArray);

	auto time_span = time_from(time_start);
	cout << BLUE << "Compaction" << RESET << endl;
	cout << fmt::format("elapsed {} ms. ", time_span / 1000) << endl;
	cout << fmt::format("sent {} MB with {} rounds. ", (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)), (io_gc->num_rounds - round_start)) << endl;

	// Verify
	std::multiset<int32_t> s;
	for(int i = 0; i < batch_size / 2; ++i) {
		s.insert(B[i].reveal<int32_t>());
	}
	for(int i = 0; i < batch_size; ++i) {
		if (labelvec[i]) {
			if (!s.count(in[i])) {
				error(fmt::format("{} does not exist!", in[i]).c_str());
			}
			s.extract(in[i]);
		}
	}

	comm_start = io_gc->counter;
    round_start = io_gc->num_rounds;
	time_start = clock_start();
	
	permute(compact_result, B, true);

	time_span = time_from(time_start);
	cout << BLUE << "Inverse Compaction" << RESET << endl;
	cout << fmt::format("elapsed {} ms. ", time_span / 1000) << endl;
	cout << fmt::format("sent {} MB with {} rounds. ", (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)), (io_gc->num_rounds - round_start)) << endl;

	for(int i = 0; i < batch_size; ++i)
		if(in[i] != B[i].reveal<int32_t>())
			error(fmt::format("{}-th position incorrect! {} != {}", i, in[i], B[i].reveal<int32_t>()).c_str());

	cout << "Test passed" << endl;

}

int main(int argc, char **argv) {
	
  ArgMapping amap;
  amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("s", batch_size, "number of total elements");
  amap.arg("l", bitlength, "bitlength of inputs");
  amap.parse(argc, argv);
  io_gc = new NetIO(party == ALICE ? nullptr : "127.0.0.1",
                    port + GC_PORT_OFFSET, true);

  setup_semi_honest(io_gc, party);
  test_compaction();
  io_gc->flush();
	cout << "# AND gates: " << circ_exec->num_and() << endl;
}
