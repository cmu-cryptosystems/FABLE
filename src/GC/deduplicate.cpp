
#include "deduplicate.h"
using namespace std;

namespace sci {


DedupContext deduplicate(IntegerArray& in, BatchLUTConfig config) {

  auto sort_result = sort(in, config.batch_size);

  IntegerArray dummies(config.batch_size);
  for (int i = 0; i < config.batch_size; i++) {
    in[i].resize(config.bitlength+1);
    dummies[i] = Integer(config.bitlength+1, config.db_size+i);
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

} // namespace sci