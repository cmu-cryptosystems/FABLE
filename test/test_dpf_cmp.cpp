#include "GC/emp-sh2pc.h"
#include "GC/custom_types.h"
#include "utils/io_utils.h"
#include <cstdint>
#include <fmt/core.h>

using namespace sci;

int party, port = 8000, size = 512, batch_size = 256;
int bitlength = 10;
string address = "127.0.0.1";
NetIO *io_gc;

inline IntegerArray get_pool(int size) {
	IntegerArray pool(size);
	for (int i = 0; i < size; i++) {
		pool[i] = Integer(bitlength, i);
	}
	return pool;
}

Integer one_hot(Integer idx, IntegerArray pool, int size) {
	Integer ret(size, 0);
	for (int i = 0; i < size; i++) {
		ret[i] = (idx == pool[i]);
	}
	return ret;
}

void test_dpf_cmp() {
	std::vector<uint32_t> plain(batch_size);
	IntegerArray in(batch_size);
	IntegerArray out;

// First specify Alice's input
	for(int i = 0; i < batch_size; ++i) {
		plain[i] = rand()%(1 << bitlength);
		in[i] = Integer(bitlength, plain[i], ALICE);
	}
	
	io_gc->flush();
	start_record(io_gc, "pool");
	auto pool = get_pool(1ULL << bitlength);
	end_record(io_gc, "pool");

	start_record(io_gc, "dpf");
	for (int i = 0; i < batch_size; i++) {
		out.push_back(one_hot(in[i], pool, 1ULL << bitlength));
	}
	end_record(io_gc, "dpf");

	for(int i = 0; i < batch_size; ++i) {
		for (int j = 0; j < 1ULL << bitlength; j++) {
			auto res = out[i][j].reveal();
			if (j == plain[i] && !res) {
				error(fmt::format("{}-th position incorrect!", i).c_str());
			}
			if (j != plain[i] && res) {
				error(fmt::format("{}-th position incorrect!", i).c_str());
			}
		}
	}

	cout << "Test passed" << endl;

}

int main(int argc, char **argv) {
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("s", batch_size, "number of total elements");
	amap.arg("l", bitlength, "bitlength of inputs");
  	amap.arg("ip", address, "IP Address of server (ALICE)");
	amap.parse(argc, argv);
	io_gc = new NetIO(party == ALICE ? nullptr : address.c_str(),
						port + GC_PORT_OFFSET, true);

	setup_semi_honest(io_gc, party);
	test_dpf_cmp();
}
