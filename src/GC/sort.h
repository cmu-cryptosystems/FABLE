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

#ifndef BATCHLUT_NUMBER_H__
#define BATCHLUT_NUMBER_H__
#include "GC/bit.h"
#include "custom_types.h"
#include <tuple>
#include "io_utils.h"

using std::cout, std::endl;
namespace sci {

typedef std::tuple<Bit, int, int> CompResultItem;
class CompResultType: public std::vector<CompResultItem> {
public:
  std::vector<std::tuple<bool, int, int>> plain_buffer;
};

inline bool is_power_of_2(int x) {
    return (x & (x-1)) == 0;
}

inline int getLogOf(int x) {
  int log = 0;
  x >>= 1;
  while (x) {
      log++;
      x >>= 1;
  }
  return log;
}

inline constexpr size_t ipow(size_t base, size_t exp)
{
    size_t result = 1;
    while (true)
    {
        if (exp & 1)
          result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }
    return result;
}

inline void show(IntegerArray& x, string name) {
  cout << name << ": ";
  for (int i=0; i<x.size(); i++) {
    cout << x[i].reveal<uint32_t>() << " ";
  }
  cout << endl;
}

inline void show(BitArray& x, string name) {
  cout << name << ": ";
  for (int i=0; i<x.size(); i++) {
    cout << x[i].reveal() << " ";
  }
  cout << endl;
}

inline void show(std::vector<int>& x, string name) {
  cout << name << ": ";
  for (int i=0; i<x.size(); i++) {
    cout << x[i] << " ";
  }
  cout << endl;
}

inline void permute(const CompResultType result, IntegerArray& data, bool inverse = false) {
  if (!inverse) {
    for (auto &[s, i, j] : result) {
      swap(s, data[i], data[j]);
    }
  } else {
    Bit s;
    int i, j;
    for (auto it = result.rbegin(); it != result.rend(); ++it) {
      std::tie(s, i, j) = *it;
      swap(s, data[i], data[j]);
    }
  }
}

void cmp_swap(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int i, int j, Bit acc, bool plain_acc, int party, CompResultType& result);

void bitonic_merge(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int lo, int n, Bit acc, bool plain_acc, int party, CompResultType& result);
void bitonic_sort(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int lo, int n, Bit acc, bool plain_acc, int party, CompResultType& result);
// Sort data, key is the first columns
CompResultType sort(std::vector<IntegerArray>& data, int size, Bit acc = true);
CompResultType sort(IntegerArray& data, int size, Bit acc = true);
CompResultType sort(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int size, int party = PUBLIC, bool acc = true);
CompResultType sort(IntegerArray& data, std::vector<int>& plain_key, int size, int party = PUBLIC, bool acc = true);
CompResultType sort(std::vector<int>& plain_key, int size, int party = PUBLIC, bool acc = true);

} // namespace sci
#endif
