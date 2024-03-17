#include "sort.h"
#include <fmt/core.h>

namespace sci {

void cmp_swap(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int i, int j, Bit acc, bool plain_acc, int party, CompResultType& result) {
  Bit to_swap;
  if (plain_key.empty()) {
    auto& key = data[0];
    to_swap = ((key[i] > key[j]) == acc);
  } else {
    bool plain_swap = ((plain_key[i] > plain_key[j]) == plain_acc);
    if (plain_swap)
      std::swap(plain_key[i], plain_key[j]);
    to_swap = Bit(plain_swap, party);
  }
  if (result.recording)
    result.emplace_back(std::make_tuple(to_swap, i, j));
  for (auto& datum: data)
    swap(to_swap, datum[i], datum[j]);
}

void bitonic_merge(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int lo, int n, Bit acc, bool plain_acc, int party, CompResultType& result) {
  if (n > 1) {
    int m = greatestPowerOfTwoLessThan(n);
    for (int i = lo; i < lo + n - m; i++)
      cmp_swap(data, plain_key, i, i + m, acc, plain_acc, party, result);
    bitonic_merge(data, plain_key, lo, m, acc, plain_acc, party, result);
    bitonic_merge(data, plain_key, lo + m, n - m, acc, plain_acc, party, result);
  }
}

void bitonic_sort(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int lo, int n, Bit acc, bool plain_acc, int party, CompResultType& result) {
  if (n > 1) {
    int m = n / 2;
    bitonic_sort(data, plain_key, lo, m, !acc, !plain_acc, party, result);
    bitonic_sort(data, plain_key, lo + m, n - m, acc, plain_acc, party, result);
    bitonic_merge(data, plain_key, lo, n, acc, plain_acc, party, result);
  }
}


// Sort data, key is the first columns
CompResultType sort(std::vector<IntegerArray>& data, int size, bool record, Bit acc) {
  CompResultType result;
  result.recording = record;
  std::vector<int> placeholder(0);
  bitonic_sort(data, placeholder, 0, size, acc, false, PUBLIC, result);
  return result;
}
CompResultType sort(IntegerArray& data, int size, bool record, Bit acc) {
  std::vector<IntegerArray> wrapper{data};
  auto result = sort(wrapper, size, record, acc);
  data = wrapper[0];
  return result;
}
CompResultType sort(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int size, int party, bool record, bool acc) {
  CompResultType result;
  result.recording = record;
  bitonic_sort(data, plain_key, 0, size, acc, acc, party, result);
  return result;
}
CompResultType sort(IntegerArray& data, std::vector<int>& plain_key, int size, int party, bool record, bool acc) {
  std::vector<IntegerArray> wrapper{data};
  auto result = sort(wrapper, plain_key, size, party, record, acc);
  data = wrapper[0];
  return result;
}
CompResultType sort(std::vector<int>& plain_key, int size, int party, bool record, bool acc) {
  std::vector<IntegerArray> wrapper;
  auto result = sort(wrapper, plain_key, size, party, record, acc);
  return result;
}

} // namespace sci