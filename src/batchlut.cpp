#include "GC/emp-sh2pc.h"
#include "GC/deduplicate.h"
#include "GC/lowmc.h"
#include "GC/io_utils.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fmt/core.h>
#include <omp.h>
#include <seal/util/defines.h>
#include <set>
#include "batchpirserver.h"
#include "batchpirclient.h"

using namespace sci;
using std::cout, std::endl, std::vector;

int party, port = 8000, batch_size = 256, parallel = 1, type = 0, lut_type = 0;
int db_size = (1 << LUT_OUTPUT_SIZE);
int bitlength = LUT_OUTPUT_SIZE;
NetIO *io_gc;

enum LUTType {
	Random, 
	Gamma
};

const int client_id = 0;

void barrier() {
	bool prepared = false;
	if (party == BOB) {
		prepared = true;
		io_gc->send_data(&prepared, sizeof(prepared));
	} else {
		io_gc->recv_data(&prepared, sizeof(prepared));
		utils::check(prepared, "[BatchLUT] Synchronization failed. ");
	}
}

inline int ftoi(double x, double lo, double hi) {
	x = clamp(x, lo, hi);
	x = (x - lo) / (hi - lo);
	x = round(x * db_size);
	return x;
}

inline double itof(int x, double lo, double hi) {
	return lo + ((double)x / db_size) * (hi - lo);
}

inline vector<uint64_t> get_lut(LUTType lut_typ) {
	vector<uint64_t> lut(db_size);
	vector<double> abs_error(db_size, 0);
	vector<double> rel_error(db_size, 0);
	
	for (int i = 0; i < db_size; i ++) {
		if (lut_typ == Random)
			lut[i] = rand() % db_size;
		else if (lut_typ == Gamma) {
			double input = itof(i, 1, 4);
			double value = std::tgamma(input);
			lut[i] = ftoi(value, 0, 6);
			abs_error[i] = abs(itof(lut[i], 0, 6) - value);
			rel_error[i] = abs_error[i] / value;
		}
	}
	cout << fmt::format(
		"LUT built. \nMax Absolute error = {}\nMax Relative error = {}", 
		*std::max_element(abs_error.begin(), abs_error.end()),
		*std::max_element(rel_error.begin(), rel_error.end())
	) << endl;
	return lut;
}

void test_lut() {
	
	BatchPirParams params(batch_size, db_size, bitlength / 4, parallel, (BatchPirType)type);
    // params.print_params();

	BatchLUTConfig config{
		batch_size, 
		(int)params.get_bucket_size(), 
		db_size, 
		bitlength + 1
	};

	// preparing queries
    vector<uint64_t> plain_queries(batch_size);
    vector<Integer> secret_queries(batch_size);
	vector<uint64_t> lut = get_lut((LUTType)lut_type);

    for (int i = 0; i < batch_size; i++) {
        plain_queries[i] = rand() % db_size;
		secret_queries[i] = Integer(DatabaseConstants::InputLength, plain_queries[i], BOB);
	}

	auto generator = [lut](size_t i){return rawdatablock(lut.at(i)); };
	
	BatchPIRServer* batch_server; 
	BatchPIRClient* batch_client;

	// ALICE: server
	// BOB: client
	int w = params.get_num_hash_funcs();
	int num_bucket = params.get_num_buckets();
	int bucket_size = params.get_bucket_size();

	// synchronize
	barrier();

	io_gc->flush();
	cout << BLUE << "BatchLUT" << RESET << endl;
	start_record(io_gc, "BatchLUT");
	
    vector<keyblock> keys(w); 
    vector<prefixblock> prefixes(w); 
	if (party == ALICE) {
		for (size_t hash_idx = 0; hash_idx < w; hash_idx++) {
			keys[hash_idx] = random_bitset<utils::keysize>();
			prefixes[hash_idx] = random_bitset<utils::prefixsize>();
		}
	}

	// Deduplication
	start_record(io_gc, "Deduplicate");
	auto context = deduplicate(secret_queries, config);
	end_record(io_gc, "Deduplicate");

	// prepare batch
	start_record(io_gc, "Batch Preparation");
    vector<vector<string>> batch(batch_size, vector<string>(w));
	vector<sci::LowMC> ciphers_2PC;
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {
		ciphers_2PC.emplace_back(sci::LowMC(keys[hash_idx], ALICE, batch_size));
	}

	vector<secret_block> m(w);
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {	
		Integer secret_prefix(prefixsize, prefixes[hash_idx].to_ullong(), ALICE);
		for (int j = 0; j < sci::blocksize; j++) {
			m[hash_idx][j].bits.resize(batch_size);
			if (j >= bitlength+1) {
				m[hash_idx][j].bits.assign(batch_size, secret_prefix[j-bitlength-1]);
			} else {
				for (int i = 0; i < batch_size; i++) {
					m[hash_idx][j][i] = secret_queries[i][j];
				}
			}
		}
	}

	vector<secret_block> c(w);
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {
		c[hash_idx] = ciphers_2PC[hash_idx].encrypt(m[hash_idx]); // blocksize, batchsize
	}
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {
    	for (int i = 0; i < batch_size; i++) {
			block hash_out;
			for (int j = 0; j < sci::blocksize; j++) {
				hash_out[j] = c[hash_idx][j][i].reveal(BOB);
			}
			batch[i][hash_idx] = hash_out.to_string();
		}
	}
	end_record(io_gc, "Batch Preparation");

	vector<IntegerArray> A_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> A_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> index(w, IntegerArray(num_bucket));
	vector<IntegerArray> entry(w, IntegerArray(num_bucket));
	vector<int> sort_reference(num_bucket, 0);
	CompResultType sort_res;

	// PIR
	start_record(io_gc, "PIR");
	if (party == BOB) {
		
		batch_client = new BatchPIRClient(params);

		auto queries = batch_client->create_queries(batch);
		auto query_buffer = batch_client->serialize_query(queries);
		auto [glk_buffer, rlk_buffer] = batch_client->get_public_keys();

		// send key_buf and query_buf
		uint32_t glk_size = glk_buffer.size(), rlk_size = rlk_buffer.size();
		io_gc->send_data(&glk_size, sizeof(uint32_t));
		io_gc->send_data(&rlk_size, sizeof(uint32_t));
		io_gc->send_data(glk_buffer.data(), glk_size);
		io_gc->send_data(rlk_buffer.data(), rlk_size);

        for (int i = 0; i < params.query_size[0]; i++) {
            for (int j = 0; j < params.query_size[1]; j++) {
                for (int k = 0; k < params.query_size[2]; k++) {
					uint32_t buf_size = query_buffer[i][j][k].size();
					io_gc->send_data(&buf_size, sizeof(uint32_t));
                    io_gc->send_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }

		set<int> dummy_buckets;
		for(int bucket_idx = 0; bucket_idx < num_bucket; ++bucket_idx) {
			if (batch_client->cuckoo_map.count(bucket_idx) == 0)
				dummy_buckets.insert(bucket_idx);
		}
		vector<int> dummies(dummy_buckets.begin(), dummy_buckets.end());
		for (int i = 0; i < batch_size; i++) {
			sort_reference[i] = batch_client->inv_cuckoo_map[i];
		}
		for (int i = batch_size; i < num_bucket; i++) {
			sort_reference[i] = dummies[i - batch_size];
		}
		sort_res = sort(sort_reference, num_bucket, BOB);

		vector<vector<vector<seal_byte>>> response_buffer(params.response_size[0]);
		for (int i = 0; i < params.response_size[0]; i++) {
            response_buffer[i].resize(params.response_size[1]);
			for (int j = 0; j < params.response_size[1]; j++) {
				uint32_t buf_size;
				io_gc->recv_data(&buf_size, sizeof(uint32_t));
				response_buffer[i][j].resize(buf_size);
				io_gc->recv_data(response_buffer[i][j].data(), buf_size);
			}
		}
		auto responses = batch_client->deserialize_response(response_buffer);
		auto decode_responses = batch_client->decode_responses(responses);

		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				A_index[hash_idx][bucket_idx] = Integer(bitlength+1, 0, ALICE);
				A_entry[hash_idx][bucket_idx] = Integer(bitlength, 0, ALICE);
			}
		}

		int total_length = w * num_bucket * datablock_size;
		bool* b = new bool[total_length];
		vector<Bit> bits(total_length);
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				auto [index, entry] = utils::split<DatabaseConstants::InputLength>(decode_responses[bucket_idx][hash_idx]);
				for (int bit_idx = 0; bit_idx < datablock_size; bit_idx++) {
					if (bit_idx < DatabaseConstants::InputLength) {
						b[hash_idx * num_bucket * datablock_size + bucket_idx * datablock_size + bit_idx] = index[bit_idx];
					} else {
						b[hash_idx * num_bucket * datablock_size + bucket_idx * datablock_size + bit_idx] = entry[bit_idx - DatabaseConstants::InputLength];
					}
				}
			}
		}
		prot_exec->feed((block128 *)bits.data(), BOB, b, total_length); 
		delete[] b;

		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				auto start_it = bits.begin() + (hash_idx * num_bucket * datablock_size + bucket_idx * datablock_size);
				B_index[hash_idx][bucket_idx].bits = vector<Bit>(start_it, start_it + DatabaseConstants::InputLength);
				B_entry[hash_idx][bucket_idx].bits = vector<Bit>(start_it + DatabaseConstants::InputLength, start_it + datablock_size);
			}
		}
	} else {
		batch_server = new BatchPIRServer(params);
		batch_server->populate_raw_db(generator);
		batch_server->initialize(keys, prefixes);

		uint32_t glk_size, rlk_size;
		io_gc->recv_data(&glk_size, sizeof(uint32_t));
		io_gc->recv_data(&rlk_size, sizeof(uint32_t));
		vector<seal::seal_byte> glk_buffer(glk_size), rlk_buffer(rlk_size);
		io_gc->recv_data(glk_buffer.data(), glk_size);
		io_gc->recv_data(rlk_buffer.data(), rlk_size);
		batch_server->set_client_keys(client_id, {glk_buffer, rlk_buffer});
		vector<vector<vector<vector<seal_byte>>>> query_buffer(params.query_size[0]);
        for (int i = 0; i < params.query_size[0]; i++) {
            query_buffer[i].resize(params.query_size[1]);
            for (int j = 0; j < params.query_size[1]; j++) {
                query_buffer[i][j].resize(params.query_size[2]);
                for (int k = 0; k < params.query_size[2]; k++) {
					uint32_t buf_size;
					io_gc->recv_data(&buf_size, sizeof(uint32_t));
					query_buffer[i][j][k].resize(buf_size);
                    io_gc->recv_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }

		sort_res = sort(sort_reference, num_bucket, BOB);

		auto queries = batch_server->deserialize_query(query_buffer);
		vector<PIRResponseList> responses = batch_server->generate_response(client_id, queries);
		auto response_buffer = batch_server->serialize_response(responses);
		 for (int i = 0; i < params.response_size[0]; i++) {
            for (int j = 0; j < params.response_size[1]; j++) {
				uint32_t buf_size = response_buffer[i][j].size();
				io_gc->send_data(&buf_size, sizeof(uint32_t));
				io_gc->send_data(response_buffer[i][j].data(), buf_size);
            }
        }
		
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				A_index[hash_idx][bucket_idx] = Integer(bitlength+1, batch_server->index_masks[hash_idx][bucket_idx].to_ullong(), ALICE);
				A_entry[hash_idx][bucket_idx] = Integer(bitlength, batch_server->entry_masks[hash_idx][bucket_idx].to_ullong(), ALICE);
			}
		}

		int total_length = w * num_bucket * datablock_size;
		bool* b = new bool[total_length];
		std::fill(b, b + total_length, false);
		vector<Bit> bits(total_length);
		prot_exec->feed((block128 *)bits.data(), BOB, b, total_length); 
		delete[] b;

		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				auto start_it = bits.begin() + (hash_idx * num_bucket * datablock_size + bucket_idx * datablock_size);
				B_index[hash_idx][bucket_idx].bits = vector<Bit>(start_it, start_it + DatabaseConstants::InputLength);
				B_entry[hash_idx][bucket_idx].bits = vector<Bit>(start_it + DatabaseConstants::InputLength, start_it + datablock_size);
			}
		}
	}

	for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			index[hash_idx][bucket_idx] = A_index[hash_idx][bucket_idx] ^ B_index[hash_idx][bucket_idx];
			entry[hash_idx][bucket_idx] = A_entry[hash_idx][bucket_idx] ^ B_entry[hash_idx][bucket_idx];
		}
	}

	// Collect result
	auto zero_index = Integer(bitlength+1, 0);
	secret_queries.resize(num_bucket, zero_index);
	permute(sort_res, secret_queries);

	auto zero_entry = Integer(bitlength, 0);
	IntegerArray result(num_bucket, zero_entry);
	for(int bucket_idx = 0; bucket_idx < num_bucket; ++bucket_idx) {
		auto selected_query = secret_queries[bucket_idx];
		
		for (int hash_idx = 0; hash_idx < w; ++hash_idx) {
			result[bucket_idx] = result[bucket_idx] ^ If(selected_query == index[hash_idx][bucket_idx], entry[hash_idx][bucket_idx], zero_entry);
		}
	}
	end_record(io_gc, "PIR");

	// Remapping
	start_record(io_gc, "Remapping");
	permute(sort_res, result, true);
	remap(result, context);
	end_record(io_gc, "Remapping");

	end_record(io_gc, "BatchLUT");

	// Verify
	vector<uint64_t> plain_result(batch_size);
	for (int i = 0; i < batch_size; i++) {
		plain_result[i] = result[i].reveal<uint64_t>();
	}
	for(int batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
		utils::check(
			plain_result[batch_idx] == lut.at(plain_queries[batch_idx]), 
			fmt::format("[BatchLUT] Test failed. T[{}]={}, but we get {}. ", plain_queries[batch_idx], lut.at(plain_queries[batch_idx]), plain_result[batch_idx])
		);
	}

	cout << GREEN << "[BatchLUT] Test passed" << RESET << endl;

	if (party == ALICE) {
		delete batch_server;
	} else {
		delete batch_client;
	}

}

int main(int argc, char **argv) {

	cout << "Maximum #threads = " << omp_get_max_threads() << endl;
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("s", batch_size, "number of total elements");
	amap.arg("par", parallel, "parallel flag: 1 = parallel; 0 = sequential");
	amap.arg("t", type, "0 = PIRANA; 1 = UIUC");
	amap.arg("l", lut_type, "0 = Random LUT; 1 = Gamma LUT");
	amap.parse(argc-1, argv+1);
	io_gc = new NetIO(party == ALICE ? nullptr : argv[1],
						port + GC_PORT_OFFSET, true);

	auto time_start = clock_start(); 
	setup_semi_honest(io_gc, party);
	auto time_span = time_from(time_start);
	cout << "General setup: elapsed " << time_span / 1000 << " ms." << endl;
	utils::check(type == 0, "Only PIRANA is supported now. "); 
	test_lut();
	delete io_gc;
	return 0;
}
