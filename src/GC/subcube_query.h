#ifndef EMP_SUBCUBE_H__
#define EMP_SUBCUBE_H__

#include "GC/integer.h"
#include "sort.h"
#include <fmt/format.h>
#include <iostream>
#include <array>

using std::cout, std::endl;

namespace sci {

struct SubcubeContext {
    std::map<size_t, std::vector<std::map<size_t, Bit>>> comp_bits;
    size_t batch_size, depth, encoded_size, bitlength;
};

inline std::vector<size_t> get_subcube_indices(size_t index, size_t depth, size_t base=3) {
    std::vector<size_t> result(depth);
    switch (base) {
        case 2:
            for (size_t i = 0; i < depth && index; i++, index >>= 1) {
                result[depth-i-1] = index & 1;
            }
            break;
        default:
            for (size_t i = 0; i < depth && index; i++, index /= base) {
                result[depth-i-1] = index % base;
            }
            break;
    }
    return result;
}

inline size_t get_original_indices(const std::vector<size_t>& subcube_index, size_t depth, size_t base=3) {
    size_t result = 0, factor = 1;
    switch (base) {
        case 2:
            for (size_t i = 0; i < depth; i++) {
                result ^= (subcube_index[depth-i-1] << i);
            }
            break;
        default:
            for (size_t i = 0; i < depth; i++, factor *= base) {
                result += subcube_index[depth-i-1] * factor;
            }
            break;
    }
    return result;
}

inline std::array<size_t, 3> expand(std::vector<size_t> subcube_index, size_t depth, size_t dim, size_t base=3) {
    std::array<size_t, 3> result;
    for (subcube_index[dim] = 0; subcube_index[dim] < 3; subcube_index[dim]++) {
        result[subcube_index[dim]] = get_original_indices(subcube_index, depth, base);
    }
    return result;
}

inline size_t switch_base(size_t i, size_t depth, size_t base_from, size_t base_to) {
    auto subcube_index = get_subcube_indices(i, depth, base_from);
    return get_original_indices(subcube_index, depth, base_to);
}

SubcubeContext subcube_query_gen(IntegerArray& query);
void subcube_response_collect(IntegerArray& response, const SubcubeContext& context);

} // namespace sci

#endif // EMP_SUBCUBE_H__