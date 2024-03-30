#include "sort.h"
#include <fmt/core.h>

namespace sci {

void cmp_swap(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int i, int j, Bit acc, bool plain_acc, int party, CompResultType& result) {
  if (plain_key.empty()) {
    auto& key = data[0];
    Bit to_swap = ((key[i] > key[j]) == acc);
    result.push_back({to_swap, i, j});
    for (auto& datum: data)
      swap(to_swap, datum[i], datum[j]);
  } else {
    bool plain_swap = ((plain_key[i] > plain_key[j]) == plain_acc);
    if (plain_swap)
      std::swap(plain_key[i], plain_key[j]);
    result.plain_buffer.push_back({plain_swap, i, j});
  }
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
CompResultType sort(std::vector<IntegerArray>& data, int size, Bit acc) {
  CompResultType result;
  std::vector<int> placeholder(0);
  bitonic_sort(data, placeholder, 0, size, acc, false, PUBLIC, result);
  return result;
}
CompResultType sort(IntegerArray& data, int size, Bit acc) {
  std::vector<IntegerArray> wrapper{data};
  auto result = sort(wrapper, size, acc);
  data = wrapper[0];
  return result;
}
CompResultType sort(std::vector<IntegerArray>& data, std::vector<int>& plain_key, int size, int party, bool acc) {
  assert (plain_key.size() > 0);
  CompResultType result;
  bitonic_sort(data, plain_key, 0, size, acc, acc, party, result);

  // post processing result
  size_t num_swap = result.plain_buffer.size();
  bool* b = new bool[num_swap];
  for (int swap_idx = 0; swap_idx < num_swap; swap_idx++) {
    b[swap_idx] = std::get<0>(result.plain_buffer[swap_idx]);
  }
  vector<block128> blocks(num_swap);
  prot_exec->feed(blocks.data(), BOB, b, num_swap); 
  delete[] b;
  for (int swap_idx = 0; swap_idx < num_swap; swap_idx++) {
    auto &[_, i, j] = result.plain_buffer[swap_idx];
    result.push_back({Bit(blocks[swap_idx]), i, j});
  }
  result.plain_buffer.clear();

  for (auto& datum: data)
    permute(result, datum);
  return result;
}
CompResultType sort(IntegerArray& data, std::vector<int>& plain_key, int size, int party, bool acc) {
  std::vector<IntegerArray> wrapper{data};
  auto result = sort(wrapper, plain_key, size, party, acc);
  data = wrapper[0];
  return result;
}
CompResultType sort(std::vector<int>& plain_key, int size, int party, bool acc) {
  std::vector<IntegerArray> wrapper;
  auto result = sort(wrapper, plain_key, size, party, acc);
  return result;
}

} // namespace sci