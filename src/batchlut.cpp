#include "GC/emp-sh2pc.h"
#include "GC/deduplicate.h"
#include "GC/lowmc.h"
#include "GC/io_utils.h"
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

int party, port = 8000, batch_size = 256, parallel = 1, type = 0;
int db_size = (1 << LUT_OUTPUT_SIZE);
int bitlength = LUT_OUTPUT_SIZE;
NetIO *io_gc;

const int client_id = 0;

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
	vector<uint64_t> lut(db_size);
    for (int i = 0; i < batch_size; i++) {
        plain_queries[i] = rand() % db_size;
		secret_queries[i] = Integer(bitlength + 1, plain_queries[i], BOB);
	}
	for (int i = 0; i < db_size; i ++) {
		lut[i] = rand() % db_size;
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
	bool prepared = false;
	if (party == BOB) {
		prepared = true;
		io_gc->send_data(&prepared, sizeof(prepared));
	} else {
		io_gc->recv_data(&prepared, sizeof(prepared));
		utils::check(prepared, "[BatchLUT] Synchronization failed. ");
	}

	cout << BLUE << "BatchLUT" << RESET << endl;
	start_record(io_gc, "BatchLUT");

	// Initialize server and client
	start_record(io_gc, "Initialization");
	if (party == ALICE) {
		batch_server = new BatchPIRServer(params);
		batch_server->populate_raw_db(generator);
		batch_server->initialize();
	} else {
		batch_client = new BatchPIRClient(params);
	}
	end_record(io_gc, "Initialization");

	// Deduplication
	start_record(io_gc, "Deduplicate");
	auto context = deduplicate(secret_queries, config);
	end_record(io_gc, "Deduplicate");

	// prepare batch
	start_record(io_gc, "Batch Preparation");
    vector<vector<string>> batch(batch_size, vector<string>(w));
	vector<sci::LowMC> ciphers_2PC;
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {
		keyblock key = party == ALICE ? batch_server->ciphers[hash_idx].get_key() : 0;
		ciphers_2PC.emplace_back(sci::LowMC(key, ALICE, batch_size));
	}

	vector<secret_block> m(w);
	for (int hash_idx = 0; hash_idx < w; hash_idx++) {	
		for (int j = 0; j < sci::blocksize; j++) {
			m[hash_idx][j] = Integer(batch_size, 0, PUBLIC);
			for (int i = 0; i < batch_size; i++) {
				if (j >= bitlength+1) {
					bool value = (party == ALICE) ? batch_server->ciphers[hash_idx].prefix[j-bitlength-1] : 0;
					m[hash_idx][j][i] = Bit(value, ALICE);
				} else {
					m[hash_idx][j][i] = secret_queries[i][j];
				}
			}
		}
	}

	for (int hash_idx = 0; hash_idx < w; hash_idx++) {
		auto c = ciphers_2PC[hash_idx].encrypt(m[hash_idx]); // blocksize, batchsize
    	for (int i = 0; i < batch_size; i++) {
			block hash_out;
			for (int j = 0; j < sci::blocksize; j++) {
				hash_out[j] = c[j][i].reveal(BOB);
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

	// PIR
	start_record(io_gc, "PIR");
	if (party == BOB) {
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
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				auto [index, entry] = utils::split<DatabaseConstants::InputLength>(decode_responses[bucket_idx][hash_idx]);
				B_index[hash_idx][bucket_idx] = Integer(bitlength+1, index.to_ullong(), BOB);
				B_entry[hash_idx][bucket_idx] = Integer(bitlength, entry.to_ullong(), BOB);
			}
		}
	} else {
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
		
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				B_index[hash_idx][bucket_idx] = Integer(bitlength+1, 0, BOB);
				B_entry[hash_idx][bucket_idx] = Integer(bitlength, 0, BOB);
			}
		}
	}
	end_record(io_gc, "PIR");

	start_record(io_gc, "Share recovery");
	for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			index[hash_idx][bucket_idx] = A_index[hash_idx][bucket_idx] ^ B_index[hash_idx][bucket_idx];
			entry[hash_idx][bucket_idx] = A_entry[hash_idx][bucket_idx] ^ B_entry[hash_idx][bucket_idx];
		}
	}
	end_record(io_gc, "Share recovery");

	// Collect result
	start_record(io_gc, "Result Collection");
	auto zero_index = Integer(bitlength+1, 0);
	secret_queries.resize(num_bucket, zero_index);
	vector<int> sort_reference(num_bucket, 0);
	if (party == BOB) {
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
	}

	auto sort_res = sort(sort_reference, num_bucket, BOB);
	permute(sort_res, secret_queries);

	auto zero_entry = Integer(bitlength, 0);
	IntegerArray result(num_bucket, zero_entry);
	for(int bucket_idx = 0; bucket_idx < num_bucket; ++bucket_idx) {
		auto selected_query = secret_queries[bucket_idx];
		
		for (int hash_idx = 0; hash_idx < w; ++hash_idx) {
			result[bucket_idx] = result[bucket_idx] ^ If(selected_query == index[hash_idx][bucket_idx], entry[hash_idx][bucket_idx], zero_entry);
		}
	}
	end_record(io_gc, "Result Collection");

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
	amap.parse(argc, argv);
	io_gc = new NetIO(party == ALICE ? nullptr : "127.0.0.1",
						port + GC_PORT_OFFSET, true);

	auto time_start = clock_start(); 
	setup_semi_honest(io_gc, party);
	auto time_span = time_from(time_start);
	cout << "General setup: elapsed " << time_span / 1000 << " ms." << endl;
	utils::check(type == 0, "Only PIRANA is supported now. "); 
	test_lut();
	io_gc->flush();
	delete io_gc;
}
