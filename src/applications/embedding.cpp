#include "GC/emp-sh2pc.h"
#include "GC/lookup.h"
#include "utils/io_utils.h"

using namespace sci;


int party, port = 8000, parallel = 1, num_threads = 32, seed = 12345;
NetIO *io_gc;

const size_t input_bits = 20; 
const size_t bits_per_element = 16, num_dimensions = 32, vocab_size = 519820; 
const size_t output_bits = bits_per_element * num_dimensions; 
const size_t words_per_sample = 16, samples_per_batch = 32;
const size_t words_per_batch = words_per_sample * samples_per_batch;

typedef vector<uint16_t> PlainArray;

inline rawdatablock pack(PlainArray& embedding) {
	string buf = "";
	for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++)
		buf = std::bitset<bits_per_element>(embedding[dim_idx]).to_string() + buf;
	return rawdatablock(buf);
}

inline IntegerArray unpack(Integer& packed_embedding) {
	IntegerArray result(num_dimensions);
	for (int dim_idx = 0; dim_idx < num_dimensions; dim_idx++) {
		result[dim_idx].bits.resize(bits_per_element);
		std::copy(
			packed_embedding.bits.begin() + dim_idx*bits_per_element, 
			packed_embedding.bits.begin() + (dim_idx + 1)*bits_per_element, 
			result[dim_idx].bits.begin()
		); 
	}
	return result;
}

void bench_embedding() {
	
    // FABLE can also be applied to word embedding lookup
	// In Crypten, 

	utils::check(LUT_INPUT_SIZE == input_bits, fmt::format("Please set LUT_INPUT_SIZE={}. ", input_bits)); 
	utils::check(LUT_OUTPUT_SIZE == output_bits, fmt::format("Please set LUT_OUTPUT_SIZE={}. ", output_bits)); 

	vector<PlainArray> word_embedding(vocab_size, PlainArray(num_dimensions)); 
	vector<PlainArray> input_sentences(samples_per_batch, PlainArray(words_per_sample)); 
	vector<IntegerArray> input_sentences_secret(samples_per_batch, IntegerArray(words_per_sample)); 
	
	if (party == ALICE) {
		for (uint64_t word_idx = 0; word_idx < vocab_size; word_idx ++) {
			for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++) {
				word_embedding[word_idx][dim_idx] = rand() % ((1 << bits_per_element) / words_per_sample); 
			}
		}
	}
	
	cout << "Embedding matrix specified." << endl;

	srand(seed);
	for (uint64_t sample_idx = 0; sample_idx < samples_per_batch; sample_idx ++) {
		for (uint64_t word_idx = 0; word_idx < words_per_sample; word_idx ++) {
			input_sentences[sample_idx][word_idx] = rand() % vocab_size; 
			input_sentences_secret[sample_idx][word_idx] = Integer(input_bits + 1, input_sentences[sample_idx][word_idx], PUBLIC); 
		}
	}
	cout << "Input specified." << endl;

	// The above code specifies the embedding matrix and input sentences

	start_record(io_gc, "Protocol Preparation");
	
	std::map<uint64_t, rawdatablock> lut; 
	if (party == ALICE) {
		for (uint64_t word_idx = 0; word_idx < vocab_size; word_idx ++) {
			lut[word_idx] = pack(word_embedding[word_idx]); 
		}
	}
	
	auto lut_params = fable_prepare(
		lut, 
		party, 
		words_per_batch, 
		vocab_size, 
		parallel, 
		num_threads, 
		BatchPirType::PIRANA, 
		HashType::LowMC, 
		io_gc
	); 
	
	end_record(io_gc, "Protocol Preparation");

	// synchronize
	barrier(party, io_gc);
	io_gc->flush();

	start_record(io_gc, "Embedding Lookup");

	IntegerArray flattened_input(words_per_batch);
	for (uint64_t sample_idx = 0; sample_idx < samples_per_batch; sample_idx ++) {
		for (uint64_t word_idx = 0; word_idx < words_per_sample; word_idx ++) {
			flattened_input[sample_idx * words_per_sample + word_idx] = input_sentences_secret[sample_idx][word_idx];
		}
	}

	auto flattened_result = fable_lookup(flattened_input, lut_params, false);

	vector<IntegerArray> result(samples_per_batch, IntegerArray(num_dimensions));

	for (uint64_t sample_idx = 0; sample_idx < samples_per_batch; sample_idx ++) {
		for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++) {
			result[sample_idx][dim_idx] = Integer(bits_per_element, 0);
		}
		for (uint64_t word_idx = 0; word_idx < words_per_sample; word_idx ++) {
			auto unpacked_result = unpack(flattened_result[sample_idx * words_per_sample + word_idx]);
			for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++) {
				result[sample_idx][dim_idx] = result[sample_idx][dim_idx] + unpacked_result[dim_idx];
			}
		}
	}

	end_record(io_gc, "Embedding Lookup");

	// Verify
	start_record(io_gc, "Verification");
	std::array<std::array<uint16_t, num_dimensions>, samples_per_batch> plain_result;
	for (uint64_t sample_idx = 0; sample_idx < samples_per_batch; sample_idx ++) {
		for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++) {
			plain_result[sample_idx][dim_idx] = result[sample_idx][dim_idx].reveal<uint32_t>(ALICE);
		}
	}
	if (party == ALICE) {
		for (uint64_t sample_idx = 0; sample_idx < samples_per_batch; sample_idx ++) {
			for (uint64_t dim_idx = 0; dim_idx < num_dimensions; dim_idx ++) {
				uint16_t gt_result = 0; 
				for (uint64_t word_idx = 0; word_idx < words_per_sample; word_idx ++) {
					gt_result += word_embedding[input_sentences[sample_idx][word_idx]][dim_idx];
				}
				check(
					plain_result[sample_idx][dim_idx] == gt_result, 
					fmt::format("[Application - Embedding] Test failed. Emb[{}][{}]={}, but we get {}. ", sample_idx, dim_idx, gt_result, plain_result[sample_idx][dim_idx])
				);
			}
		}
	}
	end_record(io_gc, "Verification");

	if (party == ALICE) {
		cout << GREEN << "[Application - Embedding] Test passed" << RESET << endl;
	} else {
		cout << "[Application - Embedding] Validation is carried out on ALICE side. " << endl;
	}
}

int main(int argc, char **argv) {
	
	signal(SIGSEGV, handler);
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("seed", seed, "random seed");
	amap.arg("par", parallel, "parallel flag: 1 = parallel; 0 = sequential");
	amap.arg("thr", num_threads, "number of threads");
	amap.parse(argc-1, argv+1);
	io_gc = new NetIO(party == ALICE ? nullptr : argv[1],
						port + GC_PORT_OFFSET, true);

	auto time_start = clock_start(); 
	setup_semi_honest(io_gc, party);
	auto time_span = time_from(time_start);
	cout << "General setup: elapsed " << time_span / 1000 << " ms." << endl;
	bench_embedding();
	delete io_gc;
	return 0;
}
