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

#ifndef EMP_COMPACT_H__
#define EMP_COMPACT_H__
#include "GC/bit.h"
#include "GC/integer.h"
#include <fmt/core.h>
#include <vector>

#include "sort.h"
#include "custom_types.h"

namespace sci {

inline Integer mod(Integer x, int len, Bit& zero) {
  for (int i=len; i < x.size()-1; i++) {
    x[i] = zero;
  }
  return x;
}

inline std::vector<Integer> getConstantArray(int n, int bitlen) {
  std::vector<Integer> constantArray(n);
  for (int i = 0; i < n; i++) {
    constantArray[i] = Integer(bitlen, i);
  }
  return constantArray;
}

void OROffCompact(BitArray label, std::vector<IntegerArray>& data, IntegerArray prefixSum, Integer& z, int n, int bitlen, int offset, IntegerArray constant, CompResultType& result);

void ORCompact(BitArray label, std::vector<IntegerArray>& data, int n, int bitlen, IntegerArray constant, IntegerArray prefixSum, CompResultType& result);

CompResultType compact(BitArray label, std::vector<IntegerArray>& data, int n, int bitlen, IntegerArray constant);

CompResultType compact(BitArray label, IntegerArray& data, int n, int bitlen, IntegerArray constant);

} // namespace sci
#endif
