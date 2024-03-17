
#include "orcompact.h"
#include <tuple>
using namespace std;

namespace sci {

void OROffCompact(BitArray label, std::vector<IntegerArray>& data, IntegerArray prefixSum, Integer& z, int n, int bitlen, int offset, IntegerArray constant, CompResultType& result) {
  assert(is_power_of_2(n));
  if (n == 2) {
    for (auto& datum: data) {
      Bit s = ((!label[offset]) & label[offset+1]) ^ z[0];
      swap(s, datum[offset], datum[offset+1]);
      result.emplace_back(std::make_tuple(s, offset, offset+1));
    }
  } else if (n > 2) {
    int half = n >> 1;

    // Some intermediate variables
    auto m = prefixSum[offset+half] - prefixSum[offset];
    auto logHalf = getLogOf(half);
    auto zModHalf = mod(z, logHalf, constant[0][0]); // z mod n/2
    auto zModHalfPlusM = zModHalf + m; // (z mod n/2) + m
    auto zPlusMModHalf = mod(zModHalfPlusM, logHalf, constant[0][0]); // (z + m) mod n/2

    OROffCompact(label, data, prefixSum, zModHalf, half, bitlen, offset, constant, result);
    OROffCompact(label, data, prefixSum, zPlusMModHalf, half, bitlen, offset+half, constant, result);
    auto s = (zModHalfPlusM >= constant[half]) ^ (z >= constant[half]);
    for (int i = 0; i < half; i++) {
      auto sel = s^(constant[i] >= zPlusMModHalf);
      for (auto& datum: data) {
        swap(sel, datum[offset+i], datum[offset+i+half]);
        result.emplace_back(std::make_tuple(sel, offset+i, offset+i+half));
      }
    }
  }
}

void ORCompact(BitArray label, std::vector<IntegerArray>& data, int n, int bitlen, IntegerArray constant, IntegerArray prefixSum, CompResultType& result) {
  if (n < 2) return;
  int n1 = greatestPowerOfTwoLessThan(n);

  if (n == n1) {
    OROffCompact(label, data, prefixSum, constant[0], n1, bitlen, 0, constant, result);
  } else {
    int n2 = n - n1;
    auto m = prefixSum[n2] - prefixSum[0];
    auto z = mod(constant[n2] + m, getLogOf(n1), constant[0][0]);

    ORCompact(label, data, n2, bitlen, constant, prefixSum, result);
    OROffCompact(label, data, prefixSum, z, n1, bitlen, n2, constant, result);
    
    for (int i = 0; i < n2; i++) {
      auto sel = constant[i] >= m;
      for (auto& datum: data) {
        swap(sel, datum[i], datum[i + n1]);
        result.emplace_back(std::make_tuple(sel, i, i+n1));
      }
    }
  }
}

CompResultType compact(BitArray label, std::vector<IntegerArray>& data, int n, int bitlen, IntegerArray constant) {
  CompResultType result;
  if (n < 2) return result;
  // Compute prefix sum
  IntegerArray prefixSum(n);
  prefixSum[0] = constant[0];
  Integer temp(bitlen, 0);
  for (int i = 1; i < n; i++) {
      temp[0] = label[i-1];
      prefixSum[i] = prefixSum[i-1] + temp;
  }
  ORCompact(label, data, n, bitlen, constant, prefixSum, result);

  return result;
}

CompResultType compact(BitArray label, IntegerArray& data, int n, int bitlen, IntegerArray constant) {
  std::vector<IntegerArray> wrapper{data};
  auto result = compact(label, wrapper, n, bitlen, constant);
  data = wrapper[0];
  return result;
}

} // namespace sci