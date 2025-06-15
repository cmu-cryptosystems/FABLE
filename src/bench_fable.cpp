#include "LUT_utils.h"

#include "GC/emp-sh2pc.h"
#include "GC/lookup.h"
#include "database_constants.h"
#include "utils/io_utils.h"
#include <cstdint>

#include <signal.h>

using namespace sci;
using std::cout, std::endl, std::vector;

int party, port = 8000, batch_size = 4096, db_size = (1 << LUT_INPUT_SIZE), parallel = 1, num_threads = 16, type = 0, lut_type = 0, hash_type = 0, fuse = 0, seed = 12345;
NetIO *io_gc;



void bench_lut() {
	
	auto lut = get_lut_vec((LUTType)lut_type, db_size, seed);

	start_record(io_gc, "Protocol Preparation");
	
	auto lut_params = fable_prepare(
		lut, 
		party, 
		batch_size, 
		db_size, 
		parallel, 
		num_threads, 
		type, 
		hash_type, 
		io_gc
	); 
	
	end_record(io_gc, "Protocol Preparation");

	start_record(io_gc, "Input Preparation");
	// preparing queries
    vector<uint64_t> plain_queries(batch_size);
    vector<Integer> secret_queries;
    for (int i = 0; i < batch_size; i++) {
		if (i < (batch_size + 1) / 2) {
			plain_queries[i] = rand() % lut.size(); 
		} else {
			plain_queries[i] = plain_queries[rand() % ((batch_size + 1) / 2)]; // Force duplicates. 
		}
		secret_queries.emplace_back(DatabaseConstants::InputLength + 1, plain_queries[i], BOB);
	}
	end_record(io_gc, "Input Preparation");

	// synchronize
	barrier(party, io_gc);
	io_gc->flush();

	cout << BLUE << "FABLE Execution" << RESET << endl;
	start_record(io_gc, "FABLE Execution");

	auto result = fuse ? fable_lookup_fuse(secret_queries, lut_params, true) : fable_lookup(secret_queries, lut_params, true);

	end_record(io_gc, "FABLE Execution");

	// Verify
	start_record(io_gc, "Verification");
	vector<uint64_t> plain_result(batch_size);
	for (int i = 0; i < batch_size; i++) {
		plain_result[i] = result[i].reveal<uint64_t>();
	}
	for(int batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
		check(
			plain_result[batch_idx] == lut.at(plain_queries[batch_idx]), 
			fmt::format("[FABLE] Test failed. T[{}]={}, but we get {}. ", plain_queries[batch_idx], lut.at(plain_queries[batch_idx]), plain_result[batch_idx])
		);
	}
	end_record(io_gc, "Verification");

	cout << GREEN << "[FABLE] Test passed" << RESET << endl;

}

int main(int argc, char **argv) {

	signal(SIGSEGV, handler);
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("bs", batch_size, "batch size");
	amap.arg("db", db_size, "database size");
	amap.arg("seed", seed, "random seed");
	amap.arg("par", parallel, "parallel flag: 1 = parallel; 0 = sequential");
	amap.arg("thr", num_threads, "number of threads");
	amap.arg("t", type, "0 = PIRANA; 1 = UIUC");
	amap.arg("l", lut_type, "0 = Random LUT; 1 = Gamma LUT");
	amap.arg("h", hash_type, "0 = LowMC; 1 = AES");
	amap.arg("f", fuse, "0 = not fuse; 1 = fuse");
	amap.parse(argc-1, argv+1);
	io_gc = new NetIO(party == ALICE ? nullptr : argv[1],
						port + GC_PORT_OFFSET, true);

	auto time_start = clock_start(); 
	setup_semi_honest(io_gc, party);
	auto time_span = time_from(time_start);
	cout << "General setup: elapsed " << time_span / 1000 << " ms." << endl;
	cout << fmt::format("Running FABLE with batch size = {}, parallel = {}, num_threads = {}, type = {}, lut_type = {}, hash_type = {}, input_size = {}, output_size = {}", batch_size, parallel, num_threads, type, lut_type, hash_type, LUT_INPUT_SIZE, LUT_OUTPUT_SIZE) << endl;
	// utils::check(type == 0, "Only PIRANA is supported now. "); 
	bench_lut();
	delete io_gc;
	return 0;
}
