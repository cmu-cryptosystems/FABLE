#include "GC/emp-sh2pc.h"
#include "utils/io_utils.h"
#include "GC/aes.h"
#include <cstdint>
#include <emmintrin.h>
#include <iostream>
#include <chrono>
#include <fmt/core.h>
#include <cryptoTools/Crypto/AES.h>

using namespace sci;
using std::cout, std::endl;

#define BENCH_EACH 1

int party, port = 8000, size = 1024;
NetIO *io_gc;

void tprint(__m128i ciphertext) {
	std::vector<string> buf;
	for (int j=1; j>=0; j--) {
		auto tmp = std::bitset<64>(ciphertext[j]).to_string();
		for (int k=0; k<8; k++) {
			buf.push_back(tmp.substr(k*8, 8));
		}
	}
	for (int i=0; i<4; i++) {
		for (int j=0; j<4; j++) {
			cout << buf[i + 4*j] << " ";
		}
		cout << endl;
	}
}

void test_aes() {
	uint64_t kh = rand(), kl = rand();
	uint64_t xh = 0xFFD5, xl = 0x4321;

	auto key = AES::create_block(kh, kl, ALICE);
	auto m = AES::create_block(xh, xl, BOB);
	std::vector<Integer> ms(size, m);
	
	auto num_ands = circ_exec->num_and();
	#if BENCH_EACH
	start_record(io_gc, "construction");
	#else
	start_record(io_gc, "total");
	#endif

	AES cipher(key);
	#if BENCH_EACH
	end_record(io_gc, "construction");
  	std::cout << fmt::format("#AND Gates = {}", (circ_exec->num_and() - num_ands) / 2) << std::endl;
	num_ands = circ_exec->num_and();
	start_record(io_gc, "encrypt");
	#endif
	auto res = cipher.EncryptECB(ms);
	#if BENCH_EACH
	end_record(io_gc, "encrypt");
	#else
	end_record(io_gc, "total");
	#endif
  	std::cout << fmt::format("#AND Gates = {}", (circ_exec->num_and() - num_ands) / 2) << std::endl;

	oc::block plaintext(xh, xl);
	oc::block ockey(kh, kl);
	oc::AES aes(ockey);

	// for (int r = 0; r <= aes.rounds; r++) {
	// 	string str_gt="\n", str_res="\n";
	// 	int counter = 0;
	// 	for (auto& byte : aes.mRoundKey[r].get<uint8_t>()) {
	// 		counter ++;
	// 		str_gt = ((counter % 4 == 0) ? "\n" : " ") + std::bitset<8>(byte).to_string() + str_gt;
	// 	}
	// 	counter = 0;
	// 	for (auto& byte : cipher.roundKeys[r]) {
	// 		counter ++;
	// 		str_res += std::bitset<8>(byte.reveal<uint32_t>()).to_string() + ((counter % 4 == 0) ? "\n" : " ");
	// 	}
	// 	if (str_gt != str_res) {
	// 		error(fmt::format("Round {} not align! \n{}\n!=\n{}", r, str_res, str_gt).c_str());
	// 	}
	// }
	// cout << GREEN << "key matched! " << RESET << endl;
	
	oc::block gt_block = aes.ecbEncBlock(plaintext);
	auto gt_result = gt_block.get<uint64_t>();

	std::bitset<128> ground_truth(std::bitset<64>(gt_result[1]).to_string() + std::bitset<64>(gt_result[0]).to_string());
	for (int i=0; i<128; i++) {
		if (res[0][i].reveal() != ground_truth[i]) {
			error(fmt::format("{}-th position not align! ", i).c_str());
		}
	}

    cout << GREEN << "Test Passed!" << RESET << endl;
}

int main(int argc, char **argv) {
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("s", size, "number of total elements");
	amap.parse(argc, argv);

	io_gc = new NetIO(party == ALICE ? nullptr : "127.0.0.1",
						port + GC_PORT_OFFSET, true);

	auto time_start = high_resolution_clock::now();
	setup_semi_honest(io_gc, party);
	auto time_end = high_resolution_clock::now();
	auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time_end - time_start).count();
	cout << "General setup: elapsed " << time_span * 1000 << " ms." << endl;
	test_aes();
}
