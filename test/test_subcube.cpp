#include "GC/emp-sh2pc.h"
#include "GC/subcube_query.h"
#include "utils/io_utils.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <fmt/core.h>
#include <sys/types.h>
#include <tuple>

using namespace sci;
using std::cout, std::endl;

int party, port = 8000, batch_size = 256;
int bitlength = 16;
string address = "127.0.0.1";
NetIO *io_gc;

auto split_db(const std::vector<uint32_t>& db) {
	assert(db.size() % 2 == 0);
	size_t length = db.size() / 2;
	std::vector<uint32_t> r1(length), r2(length), r3(length);
	std::copy(db.begin(), db.begin() + length, r1.begin());
	std::copy(db.begin() + length, db.end(), r2.begin());
	for (size_t i = 0; i < length; i++) {
		r3[i] = r1[i] ^ r2[i];
	}
	return std::make_tuple(r1, r2, r3);
}

std::vector<std::vector<uint32_t>> construct_db(int depth, std::map<uint32_t, uint32_t>& lut) {
	std::vector<uint32_t> db(1 << bitlength);
	for (size_t i = 0; i < (1 << bitlength); i++) {
		db[i] = lut[i];
	}
	std::vector<std::vector<uint32_t>> buffer{db};
	for (int i = 0; i < depth; i++) {
		std::vector<std::vector<uint32_t>> new_buf;
		auto size = buffer.size();
		new_buf.resize(size * 3);
		#pragma omp parallel for
		for (int j = 0; j < size; j++) {
		 	std::tie(new_buf[j*3], new_buf[j*3+1], new_buf[j*3+2]) = split_db(buffer[j]);
		}
		buffer = new_buf;
	}
	assert(buffer.size() == ipow(3, depth));
	return buffer;
}

void test_subcube() {

	std::vector<uint32_t> in(batch_size);
	IntegerArray query(batch_size);

	std::map<uint32_t, uint32_t> lut;
	for(int i = 0; i < batch_size; ++i) {
		in[i] = rand() % (1 << bitlength);
		query[i] = Integer(bitlength, in[i], BOB);
	}
	for (uint32_t i = 0; i < (1 << bitlength); i++) {
		lut[i] = rand() % (1 << (bitlength - 1));
	}

	size_t depth = std::log2(batch_size);

	start_record(io_gc, "Database encoding");

	auto db = construct_db(depth, lut);

	end_record(io_gc, "Database encoding");
	int size = 0;
	for (auto& b: db) {
		size += b.capacity();
	}
	cout << fmt::format("database size: {} MB. ", size * sizeof(uint32_t) / (1 << 20)) << endl;
	
	cout << BLUE << "Subcube query" << RESET << endl;
	start_record(io_gc, "Subcube query");
	auto context = subcube_query_gen(query);
	end_record(io_gc, "Subcube query");

	start_record(io_gc, "Response Generation");
	IntegerArray resp(context.encoded_size);
    for (size_t j = 0; j < context.encoded_size; j++) {
		resp[j] = Integer(
			bitlength, 
			db[j][query[j].reveal<uint32_t>() % (1 << (bitlength - depth))], 
			ALICE
		);
	}
	end_record(io_gc, "Response Generation");

	cout << BLUE << "Response Collection" << RESET << endl;
	start_record(io_gc, "Response Collection");
	
	subcube_response_collect(resp, context);

	end_record(io_gc, "Response Collection");

	for(int i = 0; i < batch_size; ++i) {
		auto result = resp[i].reveal<int32_t>();
		if (lut[in[i]] != result) {
			error(fmt::format("{}-th element: {} != {}!", i, lut[in[i]], result).c_str());
		}
	}
	cout << GREEN << "[Response Collection] Test passed" << RESET << endl;
}

int main(int argc, char **argv) {
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("s", batch_size, "bitlength of inputs");
	amap.arg("l", bitlength, "bitlength of inputs");
  	amap.arg("ip", address, "IP Address of server (ALICE)");
	amap.parse(argc, argv);
	io_gc = new NetIO(party == ALICE ? nullptr : address.c_str(),
						port + GC_PORT_OFFSET, true);

	auto time_start = high_resolution_clock::now();
	setup_semi_honest(io_gc, party);
	auto time_end = high_resolution_clock::now();
	auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time_end - time_start).count();
	cout << "General setup: elapsed " << time_span * 1000 << " ms." << endl;
	test_subcube();
	io_gc->flush();
}
