/*
Original Work Copyright (c) 2018 Xiao Wang (wangxiao@gmail.com)
Modified Work Copyright (c) 2021 Microsoft Research

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Enquiries about further applications and development opportunities are welcome.

Modified by Deevashwer Rathee
*/

#ifndef EMP_DEDUPLICATE_H__
#define EMP_DEDUPLICATE_H__
#include "GC/bit.h"
#include "GC/integer.h"
#include "sort.h"
#include <fmt/core.h>
#include "custom_types.h"

namespace sci {


struct BatchLUTConfig {
  uint64_t batch_size, bucket_size, db_size, bitlength;
};
    
struct DedupContext {
  CompResultType sort_result;
  BitArray label;
  BatchLUTConfig config;
};

DedupContext deduplicate(IntegerArray& in, BatchLUTConfig config);

// Integer* remap_obselete(Integer* in, Integer* inDeduplicated, Integer* resp, int b, int B, int N, int bitlength, int* cuckoo_map, Integer* constantArray);

// cuckoo map: [0, b) -> [0, B), -1
void remap(IntegerArray& resp, DedupContext& context);

} // namespace sci
#endif
