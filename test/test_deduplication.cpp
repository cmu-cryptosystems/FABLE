#include "GC/emp-sh2pc.h"
#include "custom_types.h"
#include "deduplicate.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <set>
#include <fmt/core.h>

using namespace sci;
using std::cout, std::endl;

int party, port = 8000, batch_size = 256;
int bucket_size = 3 * batch_size / 2;
int bitlength = 16;
NetIO *io_gc;

void test_deduplication() {

	BatchLUTConfig config{
		batch_size, 
		bucket_size, 
		(1 << bitlength), 
		bitlength
	};

	std::vector<int> in(batch_size);
	IntegerArray shrin(batch_size);
	std::map<int, int> lut;
	
	for(int i = 0; i < batch_size; ++i) {
		in[i] = rand()%(batch_size >> 1);
		shrin[i] = Integer(bitlength, in[i], BOB);
		if (!lut.count(in[i])) {
			lut[in[i]] = rand() % (1 << bitlength);
		}
	}
	
	cout << BLUE << "Deduplication" << RESET << endl;
    auto comm_start = io_gc->counter;
	auto round_start = io_gc->num_rounds;
	auto time_start = clock_start();
	
	auto context = deduplicate(shrin, config);

	auto time_span = time_from(time_start);
    cout << "elapsed " << time_span / 1000 << " ms." << endl;
    cout << "sent " << (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)) << " MB" << endl;
    cout << "took " << (io_gc->num_rounds - round_start) << " rounds. " << endl;

	// Verify
	std::set<int32_t> s;
	for(int i = 0; i < batch_size; ++i) {
		auto result = shrin[i].reveal<int32_t>();
		if (s.count(result)) {
			error(fmt::format("{} repeated!", result).c_str());
		}
		s.insert(result);
	}
	for(int i = 0; i < batch_size; ++i) {
		if (!s.count(in[i])) {
			error(fmt::format("{} does not exist!", in[i]).c_str());
		}
	}
	cout << GREEN << "[Deduplication] Test passed" << RESET << endl;

	// Remapping

	std::vector<Integer> resp(bucket_size, Integer(bitlength, 0, ALICE));
	std::vector<int> cuckoo_map(bucket_size, batch_size+1);

	for (int j = 0, m = batch_size; j < bucket_size; ++j) {
		if (rand() % (bucket_size - j) < m) {
			cuckoo_map[j] = batch_size - m;
			m--;
		}
	}

    for (size_t j = 0; j < bucket_size; j++) {
		if (cuckoo_map[j] != batch_size + 1)
			resp[j] = Integer(bitlength, lut[shrin[cuckoo_map[j]].reveal<uint32_t>()], ALICE);
	}

	cout << BLUE << "Remapping" << RESET << endl;
	comm_start = io_gc->counter;
	round_start = io_gc->num_rounds;
	time_start = clock_start();
	
	sort(resp, cuckoo_map, resp.size());
	remap(resp, context);

	time_span = time_from(time_start);
    cout << "elapsed " << time_span / 1000 << " ms." << endl;
    cout << "sent " << (io_gc->counter - comm_start) / (1.0 * (1ULL << 20)) << " MB" << endl;
    cout << "took " << (io_gc->num_rounds - round_start) << " rounds. " << endl;

	for(int i = 0; i < batch_size; ++i) {
		auto result = resp[i].reveal<int32_t>();
		if (lut[in[i]] != result) {
			error(fmt::format("{}-th element: {} != {}!", i, lut[in[i]], result).c_str());
		}
	}
	cout << GREEN << "[Remapping] Test passed" << RESET << endl;
}

int main(int argc, char **argv) {
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("s", batch_size, "number of total elements");
	amap.arg("b", bucket_size, "number of buckets");
	amap.arg("l", bitlength, "bitlength of inputs");
	amap.parse(argc, argv);
	io_gc = new NetIO(party == ALICE ? nullptr : "127.0.0.1",
						port + GC_PORT_OFFSET, true);

	auto time_start = high_resolution_clock::now();
	setup_semi_honest(io_gc, party);
	auto time_end = high_resolution_clock::now();
	auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time_end - time_start).count();
	cout << "General setup: elapsed " << time_span * 1000 << " ms." << endl;
	test_deduplication();
	io_gc->flush();
}
