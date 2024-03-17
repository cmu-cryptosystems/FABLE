
#include "deduplicate.h"
using namespace std;

namespace sci {


DedupContext deduplicate(IntegerArray& in, BatchLUTConfig config) {

  auto sort_result = sort(in, config.batch_size);

  IntegerArray dummies(config.batch_size);
  for (int i = 0; i < config.batch_size; i++) {
    in[i].resize(config.bitlength+1);
    dummies[i] = Integer(config.bitlength+1, config.db_size+i);
    // cout << dummies[i].reveal<uint>() << config.db_size+i << endl;
    // assert(dummies[i].reveal<uint>() == config.db_size+i);
  }

  BitArray label(config.batch_size);
  for (int i=1; i<config.batch_size; i++)
    label[i] = (in[i] == in[i-1]);
  for (int i=1; i<config.batch_size; i++)
    in[i] = If(label[i], dummies[i], in[i]);

  return DedupContext{
    sort_result,
    label,
    config
  };
}

void remap(IntegerArray& resp, DedupContext& context) {

  auto config = context.config;

  for (int i = 1; i < config.batch_size; i++) {
    resp[i] = If(context.label[i], resp[i-1], resp[i]);
  }

  permute(context.sort_result, resp, true);
  resp.resize(config.batch_size);
}

// Integer* remap(Integer* in, Integer* inDeduplicated, Integer* resp, int b, int B, int N, int bitlength, int* cuckoo_map, Integer* constantArray) {
  
//   auto zero = Bit(false);
//   auto one = Bit(true);
  
//   bool* cuckoo_label_plain = new bool[B]();
//   for (int i=0; i<b; i++) {
//     cuckoo_label_plain[cuckoo_map[i]] = 1;
//   }
//   auto cuckoo_label = new Bit[B];
//   for (int j=0; j<B; j++) {
//     cuckoo_label[j] = cuckoo_label_plain[j] ? one : zero;
//   }

//   compact(cuckoo_label, std::vector{resp}, B, bitlength, constantArray);

//   auto cuckoo_idx = new Integer[b];
//   for (int i=0; i<b; i++) {
//     cuckoo_idx[i] = constantArray[cuckoo_map[i]];
//   }

//   sort(cuckoo_idx, b, std::vector{resp});

//   auto zero_integer = Integer(2, 0);
//   auto one_integer = Integer(2, 1);

//   // Step 1. Extend input
//   auto idx = new Integer[2*b];
//   auto extended_input = new Integer[2*b];
//   auto extended_response = new Integer[2*b];
//   auto tag = new Integer[2*b];
//   auto label = new Bit[2*b];

//   for (int i=0; i<b; i++) {
//     idx[i] = constantArray[0];
//     idx[i+b] = constantArray[i];
//     extended_input[i] = inDeduplicated[i];
//     extended_input[i+b] = in[i];
//     extended_response[i] = resp[i];
//     extended_response[i+b] = constantArray[0];
//     tag[i] = zero_integer;
//     tag[i+b] = one_integer;
//   }
//   for (int i=0; i<2*b; i++) {
//     idx[i].resize(getLogOf(b)+1);
//   }

//   // Step 2. Sort by input
//   sort(extended_input, 2*b, std::vector{idx, extended_response}, tag);
//   for (int i=0; i<2*b; i++) {
//     label[i] = tag[i][0];
//   }

//   // Step 3. Propagation
//   for (int i=1; i<2*b; i++) {
//     extended_response[i] = If(label[i], extended_response[i-1], extended_response[i]);
//   }
    
//   // Step 4. Oblivious Compaction
//   compact(label, std::vector{idx, extended_response}, 2*b, bitlength, constantArray);

//   // Step 5. Oblivious sort w.r.t. idx
//   sort(idx, b, std::vector{extended_response});

//   delete [] idx;
//   delete [] extended_input;
//   delete [] tag;
//   delete [] label;
//   delete [] cuckoo_label_plain;
//   delete [] cuckoo_label;
//   delete [] cuckoo_idx;

//   return extended_response;
// }

} // namespace sci