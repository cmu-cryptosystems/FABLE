#include "GC/emp-sh2pc.h"
#include "custom_types.h"
#include "sort.h"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <fmt/core.h>

using namespace sci;
using std::cout, std::endl;

int party, port = 8000, iters = 512, batch_size = 256;
int bitlength = 32;
NetIO *io_gc;

void test_sort() {
	std::vector<uint32_t> plain(batch_size);
	IntegerArray in(batch_size);

// First specify Alice's input
	for(int i = 0; i < batch_size; ++i) {
		plain[i] = rand()%(batch_size >> 1);
		in[i] = Integer(bitlength, plain[i], ALICE);
	}
	
    auto comm_start = io_gc->counter;
	auto time_start = clock_start();

	auto swap_map = sort(in, batch_size);

	auto time_span = time_from(time_start);
	cout << BLUE << "Sort" << RESET << endl;
    cout << "elapsed " << time_span / 1000 << " ms." << endl;
    cout << "sent " << (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)) << " MB" << endl;
	int max = -1;
	for(int i = 0; i < batch_size; ++i) {
		auto res = in[i].reveal<int32_t>();
		if (max > res)
			error(fmt::format("{}-th position incorrect!", i).c_str());
		max = res;
	}

	comm_start = io_gc->counter;
	time_start = high_resolution_clock::now();

	permute(swap_map, in, true);

	time_span = time_from(time_start);
	cout << BLUE << "Unsort" << RESET << endl;
    cout << "elapsed " << time_span / 1000 << " ms." << endl;
    cout << "sent " << (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)) << " MB" << endl;
	for(int i = 0; i < batch_size; ++i)
		if(plain[i] != in[i].reveal<int32_t>())
			error(fmt::format("{}-th position incorrect! {} != {}", i, plain[i], in[i].reveal<int32_t>()).c_str());

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
	test_sort();
}
