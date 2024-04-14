#include "GC/emp-sh2pc.h"
#include "GC/custom_types.h"
#include "GC/sort.h"
#include "utils/io_utils.h"
#include <cstdint>
#include <fmt/core.h>

using namespace sci;

int party, port = 8000, iters = 512, batch_size = 256;
int bitlength = 32;
NetIO *io_gc;

void test_sort() {
	std::vector<uint32_t> plain(batch_size);
	std::vector<int> plain_test(batch_size);
	IntegerArray in(batch_size);

// First specify Alice's input
	for(int i = 0; i < batch_size; ++i) {
		plain[i] = rand()%(batch_size >> 1);
		plain_test[i] = plain[i];
		in[i] = Integer(bitlength, plain[i], ALICE);
	}
	
	io_gc->flush();
	start_record(io_gc, "sort");
	auto swap_map = sort(in, batch_size);
	end_record(io_gc, "sort");

	int max = -1;
	for(int i = 0; i < batch_size; ++i) {
		auto res = in[i].reveal<int32_t>();
		if (max > res)
			error(fmt::format("{}-th position incorrect!", i).c_str());
		max = res;
	}

	IntegerArray in_copy(in);

	start_record(io_gc, "plain_sort");
	auto plain_swap_map = sort(plain_test, batch_size, BOB);
	end_record(io_gc, "plain_sort");

	max = -1;
	for(int i = 0; i < batch_size; ++i) {
		auto res = plain_test[i];
		if (max > res)
			error(fmt::format("{}-th position incorrect!", i).c_str());
		max = res;
	}

	start_record(io_gc, "unsort");
	permute(swap_map, in, true);
	end_record(io_gc, "unsort");
	
	start_record(io_gc, "unsort_plain");
	permute(plain_swap_map, in_copy, true);
	end_record(io_gc, "unsort_plain");

	for(int i = 0; i < batch_size; ++i)
		if(plain[i] != in[i].reveal<int32_t>())
			error(fmt::format("{}-th position incorrect! {} != {}", i, plain[i], in[i].reveal<int32_t>()).c_str());
		
	for(int i = 0; i < batch_size; ++i)
		if(plain[i] != in_copy[i].reveal<int32_t>())
			error(fmt::format("{}-th position incorrect! {} != {}", i, plain[i], in_copy[i].reveal<int32_t>()).c_str());

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
