#ifndef FABLE_LOOKUP_H__
#define FABLE_LOOKUP_H__

#include "GC/integer.h"
#include "sort.h"
#include "deduplicate.h"
#include "lowmc.h"
#include "aes.h"
#include <fmt/core.h>
#include "utils/io_utils.h"
#include "custom_types.h"
#include "batchpirserver.h"
#include "batchpirclient.h"
#include <set>

namespace sci {

const int client_id = 0;

struct BatchLUTParams{
    int party;
    int hash_type;
    int batch_size;
    BatchLUTConfig* config;
    osuCrypto::PRNG* prng; 
    BatchPirParams* params; 
    BatchPIRServer* batch_server; 
    BatchPIRClient* batch_client;
    NetIO *io_gc; 
}; 

inline void barrier(int party, sci::NetIO* io_gc) {
	bool prepared = false;
	if (party == sci::BOB) {
		prepared = true;
		io_gc->send_data(&prepared, sizeof(prepared));
	} else {
		io_gc->recv_data(&prepared, sizeof(prepared));
		utils::check(prepared, "[BatchLUT] Synchronization failed. ");
	}
}

template<size_t size>
Integer share_bitset(std::bitset<size> bits, int party) {
	Integer res(size, 0);
	for (int i = 0; i < size; i++) {
		res[i] = Bit(bits[i], party);
	}
	return res;
}

BatchLUTParams fable_prepare(map<uint64_t, uint64_t>& lut, int party, int batch_size, bool parallel, int num_threads, int type, int hash_type, NetIO *io_gc); 

BatchLUTParams fable_prepare(map<uint64_t, rawdatablock>& lut, int party, int batch_size, int db_size, bool parallel, int num_threads, BatchPirType type, HashType hash_type, NetIO *io_gc); 

IntegerArray fable_lookup(IntegerArray secret_queries, BatchLUTParams& lut_params, bool verbose = false); 

IntegerArray fable_lookup_fuse(IntegerArray secret_queries, BatchLUTParams& lut_params, bool verbose = false); 

} // namespace sci
#endif
