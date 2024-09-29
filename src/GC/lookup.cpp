#include "lookup.h"

namespace sci {

BatchLUTParams fable_prepare(map<uint64_t, uint64_t>& lut, int party, int batch_size, bool parallel, int num_threads, int type, int hash_type, NetIO *io_gc) {

	auto params = new BatchPirParams(batch_size, lut.size(), parallel, num_threads, (BatchPirType)type, (HashType)hash_type);

	auto config = new BatchLUTConfig{
		params->get_batch_size(), 
		params->get_bucket_size(), 
		(1 << LUT_INPUT_SIZE), 
		LUT_INPUT_SIZE
	};

	BatchPIRServer* batch_server; 
	BatchPIRClient* batch_client;
	
    osuCrypto::PRNG* prng = new osuCrypto::PRNG(osuCrypto::sysRandomSeed());

	if (party == BOB) {
		batch_client = new BatchPIRClient(*params);
		auto [glk_buffer, rlk_buffer] = batch_client->get_public_keys();

		// send key_buf and query_buf
		uint32_t glk_size = glk_buffer.size(), rlk_size = rlk_buffer.size();
		io_gc->send_data(&glk_size, sizeof(uint32_t));
		io_gc->send_data(&rlk_size, sizeof(uint32_t));
		io_gc->send_data(glk_buffer.data(), glk_size);
		io_gc->send_data(rlk_buffer.data(), rlk_size);
	} else {
		batch_server = new BatchPIRServer(*params, *prng);
		batch_server->populate_raw_db(lut);
		uint32_t glk_size, rlk_size;
		io_gc->recv_data(&glk_size, sizeof(uint32_t));
		io_gc->recv_data(&rlk_size, sizeof(uint32_t));
		vector<seal::seal_byte> glk_buffer(glk_size), rlk_buffer(rlk_size);
		io_gc->recv_data(glk_buffer.data(), glk_size);
		io_gc->recv_data(rlk_buffer.data(), rlk_size);
		batch_server->set_client_keys(client_id, {glk_buffer, rlk_buffer});
	}
	
	auto lut_params = BatchLUTParams{
		party, 
		hash_type, 
		batch_size, 
		config, 
		prng, 
		params,  
		batch_server, 
		batch_client, 
		io_gc
	};

	return lut_params; 
}

BatchLUTParams fable_prepare(map<uint64_t, rawdatablock>& lut, int party, int batch_size, int db_size, bool parallel, int num_threads, BatchPirType type, HashType hash_type, NetIO *io_gc) {

	auto params = new BatchPirParams(batch_size, db_size, parallel, num_threads, type, hash_type);

	auto config = new BatchLUTConfig{
		params->get_batch_size(), 
		params->get_bucket_size(), 
		(1 << LUT_INPUT_SIZE), 
		LUT_INPUT_SIZE
	};

	BatchPIRServer* batch_server; 
	BatchPIRClient* batch_client;
	
    osuCrypto::PRNG* prng = new osuCrypto::PRNG(osuCrypto::sysRandomSeed());

	if (party == BOB) {
		batch_client = new BatchPIRClient(*params);
		auto [glk_buffer, rlk_buffer] = batch_client->get_public_keys();

		// send key_buf and query_buf
		uint32_t glk_size = glk_buffer.size(), rlk_size = rlk_buffer.size();
		io_gc->send_data(&glk_size, sizeof(uint32_t));
		io_gc->send_data(&rlk_size, sizeof(uint32_t));
		io_gc->send_data(glk_buffer.data(), glk_size);
		io_gc->send_data(rlk_buffer.data(), rlk_size);
	} else {
		batch_server = new BatchPIRServer(*params, *prng);
		batch_server->populate_raw_db(lut);
		uint32_t glk_size, rlk_size;
		io_gc->recv_data(&glk_size, sizeof(uint32_t));
		io_gc->recv_data(&rlk_size, sizeof(uint32_t));
		vector<seal::seal_byte> glk_buffer(glk_size), rlk_buffer(rlk_size);
		io_gc->recv_data(glk_buffer.data(), glk_size);
		io_gc->recv_data(rlk_buffer.data(), rlk_size);
		batch_server->set_client_keys(client_id, {glk_buffer, rlk_buffer});
	}
	
	auto lut_params = BatchLUTParams{
		party, 
		hash_type, 
		batch_size, 
		config, 
		prng, 
		params,  
		batch_server, 
		batch_client, 
		io_gc
	};

	return lut_params; 
}

IntegerArray fable_lookup(IntegerArray secret_queries, BatchLUTParams& lut_params, bool verbose) {

	auto& [party, hash_type, batch_size, config, prng, params, batch_server, batch_client, io_gc] = lut_params;

	int num_bucket = params->get_num_buckets();
	const int w = DatabaseConstants::NumHashFunctions;

    // Deduplication
	start_record(io_gc, "Deduplicate");
	auto context = deduplicate(secret_queries, *config);
	end_record(io_gc, "Deduplicate", verbose);

	// prepare batch
	start_record(io_gc, "OPRF Evaluation");
	
    keyblock lowmc_key; 
    prefixblock lowmc_prefix; 
    oc::block aes_key;
    std::bitset<128-DatabaseConstants::InputLength> aes_prefix;
	if (party == ALICE) {
		if (hash_type == HashType::LowMC) {
			lowmc_key = random_bitset<utils::keysize>(prng);
			lowmc_prefix = 0; // random_bitset<utils::prefixsize>(&prng);
		} else {
			aes_key = prng->get<oc::block>();
			aes_prefix = 0;
		}
	}

    vector<string> batch(batch_size);
	sci::LowMC* lowmc_ciphers_2PC;
	sci::AES* aes_ciphers_2PC;

	if (hash_type == HashType::LowMC) {
		lowmc_ciphers_2PC = new sci::LowMC(lowmc_key, ALICE, batch_size);

		secret_block m;
		// Integer secret_prefix = share_bitset(lowmc_prefix, ALICE);
		for (int j = 0; j < sci::blocksize; j++) {
			m[j].bits.resize(batch_size);
			if (j >= LUT_INPUT_SIZE+1) {
				m[j].bits.assign(batch_size, Bit(0));
				// m[j].bits.assign(batch_size, secret_prefix[j-bitlength-1]);
			} else {
				for (int i = 0; i < batch_size; i++) {
					m[j][i] = secret_queries[i][j];
				}
			}
		}

		secret_block c;
		c = lowmc_ciphers_2PC->encrypt(m); // blocksize, batchsize
		for (int i = 0; i < batch_size; i++) {
			block hash_out;
			for (int j = 0; j < sci::blocksize; j++) {
				hash_out[j] = c[j][i].reveal(BOB);
			}
			batch[i] = hash_out.to_string();
		}
	} else {
		vector<Integer> m(batch_size);
		vector<Integer> c;
		Integer key(128, 0);
		auto data = aes_key.get<uint64_t>();
		auto key_bitset = concatenate(std::bitset<64>(data[1]), std::bitset<64>(data[0]));
		for (int i = 0; i < 128; i++) {
			key[i] = Bit(key_bitset[i], ALICE);
		}
		aes_ciphers_2PC = new sci::AES(key);
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {	
			Integer secret_prefix = share_bitset(aes_prefix, ALICE);
			for (int j = 0; j < batch_size; j++) {
				m[j] = secret_queries[j];
				m[j].bits.insert(m[j].bits.end(), secret_prefix.bits.begin(), secret_prefix.bits.end());
			}
			c = aes_ciphers_2PC->EncryptECB(m);
			for (int i = 0; i < batch_size; i++) {
				std::bitset<128> hash_out;
				for (int j = 0; j < 128; j++) {
					hash_out[j] = c[i][j].reveal(BOB);
				}
				batch[i] = hash_out.to_string();
			}
		}
	}
	end_record(io_gc, "OPRF Evaluation", verbose);

	// PIR
	start_record(io_gc, "Share Retrieval");

	vector<IntegerArray> A_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> A_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> index(w, IntegerArray(num_bucket));
	vector<IntegerArray> entry(w, IntegerArray(num_bucket));

	if (party == BOB) {
		start_record(io_gc, "Query Computation");
		auto queries = batch_client->create_queries(batch);
		auto query_buffer = batch_client->serialize_query(queries);
		end_record(io_gc, "Query Computation", verbose);

		start_record(io_gc, "Query Communication");
        for (int i = 0; i < params->query_size[0]; i++) {
            for (int j = 0; j < params->query_size[1]; j++) {
                for (int k = 0; k < params->query_size[2]; k++) {
					uint32_t buf_size = query_buffer[i][j][k].size();
					io_gc->send_data(&buf_size, sizeof(uint32_t));
                    io_gc->send_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }
		end_record(io_gc, "Query Communication", verbose);


		start_record(io_gc, "Answer Communication");
		vector<vector<vector<seal_byte>>> response_buffer(params->response_size[0]);
		for (int i = 0; i < params->response_size[0]; i++) {
            response_buffer[i].resize(params->response_size[1]);
			for (int j = 0; j < params->response_size[1]; j++) {
				uint32_t buf_size;
				io_gc->recv_data(&buf_size, sizeof(uint32_t));
				response_buffer[i][j].resize(buf_size);
				io_gc->recv_data(response_buffer[i][j].data(), buf_size);
			}
		}
		end_record(io_gc, "Answer Communication", verbose);

		start_record(io_gc, "Extraction");
		auto responses = batch_client->deserialize_response(response_buffer);
		auto decode_responses = batch_client->decode_responses(responses);
		end_record(io_gc, "Extraction", verbose);

		start_record(io_gc, "Share Conversion");
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				A_index[hash_idx][bucket_idx] = Integer(LUT_INPUT_SIZE+1, 0, ALICE);
				A_entry[hash_idx][bucket_idx] = Integer(LUT_OUTPUT_SIZE, 0, ALICE);
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
		start_record(io_gc, "Server Setup");
		if (params->get_hash_type() == HashType::LowMC) {
			batch_server->lowmc_prepare(lowmc_key, lowmc_prefix);
		} else {
			batch_server->aes_prepare(aes_key, aes_prefix);
		}
		batch_server->initialize();
		end_record(io_gc, "Server Setup", verbose);

		start_record(io_gc, "Query Communication");
		vector<vector<vector<vector<seal_byte>>>> query_buffer(params->query_size[0]);
        for (int i = 0; i < params->query_size[0]; i++) {
            query_buffer[i].resize(params->query_size[1]);
            for (int j = 0; j < params->query_size[1]; j++) {
                query_buffer[i][j].resize(params->query_size[2]);
                for (int k = 0; k < params->query_size[2]; k++) {
					uint32_t buf_size;
					io_gc->recv_data(&buf_size, sizeof(uint32_t));
					query_buffer[i][j][k].resize(buf_size);
                    io_gc->recv_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }
		end_record(io_gc, "Query Communication", verbose);

		start_record(io_gc, "Answer Computation");
		auto queries = batch_server->deserialize_query(query_buffer);
		vector<PIRResponseList> responses = batch_server->generate_response(client_id, queries);
		auto response_buffer = batch_server->serialize_response(responses);
		end_record(io_gc, "Answer Computation", verbose);

		start_record(io_gc, "Answer Communication");
		for (int i = 0; i < params->response_size[0]; i++) {
            for (int j = 0; j < params->response_size[1]; j++) {
				uint32_t buf_size = response_buffer[i][j].size();
				io_gc->send_data(&buf_size, sizeof(uint32_t));
				io_gc->send_data(response_buffer[i][j].data(), buf_size);
            }
        }
		end_record(io_gc, "Answer Communication", verbose);
		
		start_record(io_gc, "Share Conversion");
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				bool* index_mask_buffer = new bool[LUT_INPUT_SIZE+1];
				for (int i = 0; i < LUT_INPUT_SIZE+1; i++) 
					index_mask_buffer[i] = batch_server->index_masks[hash_idx][bucket_idx][i];
				A_index[hash_idx][bucket_idx].bits.resize(LUT_INPUT_SIZE+1);
				prot_exec->feed((block128 *)A_index[hash_idx][bucket_idx].bits.data(), ALICE, index_mask_buffer, LUT_INPUT_SIZE+1); 
				delete[] index_mask_buffer;

				bool* entry_mask_buffer = new bool[LUT_OUTPUT_SIZE];
				for (int i = 0; i < LUT_OUTPUT_SIZE; i++) 
					entry_mask_buffer[i] = batch_server->entry_masks[hash_idx][bucket_idx][i];
				A_entry[hash_idx][bucket_idx].bits.resize(LUT_OUTPUT_SIZE);
				prot_exec->feed((block128 *)A_entry[hash_idx][bucket_idx].bits.data(), ALICE, entry_mask_buffer, LUT_OUTPUT_SIZE); 
				delete[] entry_mask_buffer;
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
	end_record(io_gc, "Share Conversion", verbose);

	end_record(io_gc, "Share Retrieval", verbose);

	start_record(io_gc, "Decode");
	
	// Gen Context
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
	CompResultType sort_res = sort(sort_reference, num_bucket, BOB);

	// Collect result
	auto zero_index = Integer(LUT_INPUT_SIZE+1, 0);
	secret_queries.resize(num_bucket, zero_index);
	permute(sort_res, secret_queries);

	auto zero_entry = Integer(LUT_OUTPUT_SIZE, 0);
	IntegerArray result(num_bucket, zero_entry);
	for(int bucket_idx = 0; bucket_idx < num_bucket; ++bucket_idx) {
		auto selected_query = secret_queries[bucket_idx];
		
		for (int hash_idx = 0; hash_idx < w; ++hash_idx) {
			result[bucket_idx] = result[bucket_idx] ^ If(selected_query == index[hash_idx][bucket_idx], entry[hash_idx][bucket_idx], zero_entry);
		}
	}

	permute(sort_res, result, true);
	end_record(io_gc, "Decode", verbose);
	
	// Remapping
	start_record(io_gc, "Mapping");
	remap(result, context);
	end_record(io_gc, "Mapping", verbose);
	
	if (party == ALICE) {
		delete batch_server;
	} else {
		delete batch_client;
	}
	delete prng;
	delete params;
	delete config;
	if (hash_type == HashType::LowMC) {
		delete lowmc_ciphers_2PC;
	} else {
		delete aes_ciphers_2PC;
	}
    
    return result;
}

IntegerArray fable_lookup_fuse(IntegerArray secret_queries, BatchLUTParams& lut_params, bool verbose) {

	auto& [party, hash_type, batch_size, config, prng, params, batch_server, batch_client, io_gc] = lut_params;

	int num_bucket = params->get_num_buckets();
	const int w = DatabaseConstants::NumHashFunctions;

    // Deduplication
	start_record(io_gc, "Deduplicate");
	auto context = deduplicate(secret_queries, *config);
	end_record(io_gc, "Deduplicate", verbose);

	// prepare batch
	start_record(io_gc, "OPRF Evaluation");
	
    keyblock lowmc_key; 
    prefixblock lowmc_prefix; 
    oc::block aes_key;
    std::bitset<128-DatabaseConstants::InputLength> aes_prefix;
	if (party == ALICE) {
		if (hash_type == HashType::LowMC) {
			lowmc_key = random_bitset<utils::keysize>(prng);
			lowmc_prefix = 0; // random_bitset<utils::prefixsize>(&prng);
		} else {
			aes_key = prng->get<oc::block>();
			aes_prefix = 0;
		}
	}

    vector<string> batch(batch_size);
	sci::LowMC* lowmc_ciphers_2PC;
	sci::AES* aes_ciphers_2PC;

	if (hash_type == HashType::LowMC) {
		lowmc_ciphers_2PC = new sci::LowMC(lowmc_key, ALICE, batch_size);

		secret_block m;
		// Integer secret_prefix = share_bitset(lowmc_prefix, ALICE);
		for (int j = 0; j < sci::blocksize; j++) {
			m[j].bits.resize(batch_size);
			if (j >= LUT_INPUT_SIZE+1) {
				m[j].bits.assign(batch_size, Bit(0));
				// m[j].bits.assign(batch_size, secret_prefix[j-bitlength-1]);
			} else {
				for (int i = 0; i < batch_size; i++) {
					m[j][i] = secret_queries[i][j];
				}
			}
		}

		secret_block c;
		c = lowmc_ciphers_2PC->encrypt(m); // blocksize, batchsize
		for (int i = 0; i < batch_size; i++) {
			block hash_out;
			for (int j = 0; j < sci::blocksize; j++) {
				hash_out[j] = c[j][i].reveal(BOB);
			}
			batch[i] = hash_out.to_string();
		}
	} else {
		vector<Integer> m(batch_size);
		vector<Integer> c;
		Integer key(128, 0);
		auto data = aes_key.get<uint64_t>();
		auto key_bitset = concatenate(std::bitset<64>(data[1]), std::bitset<64>(data[0]));
		for (int i = 0; i < 128; i++) {
			key[i] = Bit(key_bitset[i], ALICE);
		}
		aes_ciphers_2PC = new sci::AES(key);
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {	
			Integer secret_prefix = share_bitset(aes_prefix, ALICE);
			for (int j = 0; j < batch_size; j++) {
				m[j] = secret_queries[j];
				m[j].bits.insert(m[j].bits.end(), secret_prefix.bits.begin(), secret_prefix.bits.end());
			}
			c = aes_ciphers_2PC->EncryptECB(m);
			for (int i = 0; i < batch_size; i++) {
				std::bitset<128> hash_out;
				for (int j = 0; j < 128; j++) {
					hash_out[j] = c[i][j].reveal(BOB);
				}
				batch[i] = hash_out.to_string();
			}
		}
	}
	end_record(io_gc, "OPRF Evaluation", verbose);

	// PIR
	start_record(io_gc, "Share Retrieval + Decode");

	vector<IntegerArray> A_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> A_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_index(w, IntegerArray(num_bucket));
	vector<IntegerArray> B_entry(w, IntegerArray(num_bucket));
	vector<IntegerArray> index(w, IntegerArray(num_bucket));
	vector<IntegerArray> entry(w, IntegerArray(num_bucket));
	vector<int> sort_reference(num_bucket, 0);
	CompResultType sort_res;

	if (party == BOB) {
		start_record(io_gc, "Query Computation");
		auto queries = batch_client->create_queries(batch);
		auto query_buffer = batch_client->serialize_query(queries);
		end_record(io_gc, "Query Computation", verbose);

		start_record(io_gc, "Query Communication");
        for (int i = 0; i < params->query_size[0]; i++) {
            for (int j = 0; j < params->query_size[1]; j++) {
                for (int k = 0; k < params->query_size[2]; k++) {
					uint32_t buf_size = query_buffer[i][j][k].size();
					io_gc->send_data(&buf_size, sizeof(uint32_t));
                    io_gc->send_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }
		end_record(io_gc, "Query Communication", verbose);

		start_record(io_gc, "Context Generation");
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
		end_record(io_gc, "Context Generation");

		start_record(io_gc, "Answer Communication");
		vector<vector<vector<seal_byte>>> response_buffer(params->response_size[0]);
		for (int i = 0; i < params->response_size[0]; i++) {
            response_buffer[i].resize(params->response_size[1]);
			for (int j = 0; j < params->response_size[1]; j++) {
				uint32_t buf_size;
				io_gc->recv_data(&buf_size, sizeof(uint32_t));
				response_buffer[i][j].resize(buf_size);
				io_gc->recv_data(response_buffer[i][j].data(), buf_size);
			}
		}
		end_record(io_gc, "Answer Communication", verbose);

		start_record(io_gc, "Extraction");
		auto responses = batch_client->deserialize_response(response_buffer);
		auto decode_responses = batch_client->decode_responses(responses);
		end_record(io_gc, "Extraction", verbose);

		start_record(io_gc, "Share Conversion");
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				A_index[hash_idx][bucket_idx] = Integer(LUT_INPUT_SIZE+1, 0, ALICE);
				A_entry[hash_idx][bucket_idx] = Integer(LUT_OUTPUT_SIZE, 0, ALICE);
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
		start_record(io_gc, "Server Setup");
		if (params->get_hash_type() == HashType::LowMC) {
			batch_server->lowmc_prepare(lowmc_key, lowmc_prefix);
		} else {
			batch_server->aes_prepare(aes_key, aes_prefix);
		}
		batch_server->initialize();
		end_record(io_gc, "Server Setup", verbose);

		start_record(io_gc, "Query Communication");
		vector<vector<vector<vector<seal_byte>>>> query_buffer(params->query_size[0]);
        for (int i = 0; i < params->query_size[0]; i++) {
            query_buffer[i].resize(params->query_size[1]);
            for (int j = 0; j < params->query_size[1]; j++) {
                query_buffer[i][j].resize(params->query_size[2]);
                for (int k = 0; k < params->query_size[2]; k++) {
					uint32_t buf_size;
					io_gc->recv_data(&buf_size, sizeof(uint32_t));
					query_buffer[i][j][k].resize(buf_size);
                    io_gc->recv_data(query_buffer[i][j][k].data(), buf_size);
                }
            }
        }
		end_record(io_gc, "Query Communication", verbose);
		
		start_record(io_gc, "Context Generation");
		sort_res = sort(sort_reference, num_bucket, BOB);
		end_record(io_gc, "Context Generation");

		start_record(io_gc, "Answer Computation");
		auto queries = batch_server->deserialize_query(query_buffer);
		vector<PIRResponseList> responses = batch_server->generate_response(client_id, queries);
		auto response_buffer = batch_server->serialize_response(responses);
		end_record(io_gc, "Answer Computation", verbose);

		start_record(io_gc, "Answer Communication");
		for (int i = 0; i < params->response_size[0]; i++) {
            for (int j = 0; j < params->response_size[1]; j++) {
				uint32_t buf_size = response_buffer[i][j].size();
				io_gc->send_data(&buf_size, sizeof(uint32_t));
				io_gc->send_data(response_buffer[i][j].data(), buf_size);
            }
        }
		end_record(io_gc, "Answer Communication", verbose);
		
		start_record(io_gc, "Share Conversion");
		for (int hash_idx = 0; hash_idx < w; hash_idx++) {
			for (int bucket_idx = 0; bucket_idx < num_bucket; bucket_idx++) {
				bool* index_mask_buffer = new bool[LUT_INPUT_SIZE+1];
				for (int i = 0; i < LUT_INPUT_SIZE+1; i++) 
					index_mask_buffer[i] = batch_server->index_masks[hash_idx][bucket_idx][i];
				A_index[hash_idx][bucket_idx].bits.resize(LUT_INPUT_SIZE+1);
				prot_exec->feed((block128 *)A_index[hash_idx][bucket_idx].bits.data(), ALICE, index_mask_buffer, LUT_INPUT_SIZE+1); 
				delete[] index_mask_buffer;

				bool* entry_mask_buffer = new bool[LUT_OUTPUT_SIZE];
				for (int i = 0; i < LUT_OUTPUT_SIZE; i++) 
					entry_mask_buffer[i] = batch_server->entry_masks[hash_idx][bucket_idx][i];
				A_entry[hash_idx][bucket_idx].bits.resize(LUT_OUTPUT_SIZE);
				prot_exec->feed((block128 *)A_entry[hash_idx][bucket_idx].bits.data(), ALICE, entry_mask_buffer, LUT_OUTPUT_SIZE); 
				delete[] entry_mask_buffer;
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
	end_record(io_gc, "Share Conversion", verbose);
	
	// Gen Context is fused to above

	// Collect result
	start_record(io_gc, "Result Collection");
	auto zero_index = Integer(LUT_INPUT_SIZE+1, 0);
	secret_queries.resize(num_bucket, zero_index);
	permute(sort_res, secret_queries);

	auto zero_entry = Integer(LUT_OUTPUT_SIZE, 0);
	IntegerArray result(num_bucket, zero_entry);
	for(int bucket_idx = 0; bucket_idx < num_bucket; ++bucket_idx) {
		auto selected_query = secret_queries[bucket_idx];
		
		for (int hash_idx = 0; hash_idx < w; ++hash_idx) {
			result[bucket_idx] = result[bucket_idx] ^ If(selected_query == index[hash_idx][bucket_idx], entry[hash_idx][bucket_idx], zero_entry);
		}
	}

	permute(sort_res, result, true);
	end_record(io_gc, "Result Collection");
	end_record(io_gc, "Share Retrieval + Decode", verbose);
	
	// Remapping
	start_record(io_gc, "Mapping");
	remap(result, context);
	end_record(io_gc, "Mapping", verbose);
	
	if (party == ALICE) {
		delete batch_server;
	} else {
		delete batch_client;
	}
	delete prng;
	delete params;
	delete config;
	if (hash_type == HashType::LowMC) {
		delete lowmc_ciphers_2PC;
	} else {
		delete aes_ciphers_2PC;
	}
    
    return result;
}

} // namespace sci