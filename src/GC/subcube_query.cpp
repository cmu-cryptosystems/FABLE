#include "subcube_query.h"

namespace sci {

SubcubeContext subcube_query_gen(IntegerArray& query) {
    size_t batch_size = query.size();
    size_t depth = std::log2(batch_size);
    size_t encoded_size = ipow(3, depth);
    size_t bitlength = query[0].size();
    std::map<size_t, std::vector<std::map<size_t, Bit>>> comp_bits;

    IntegerArray result(encoded_size);
    for (size_t i = 0; i < batch_size; i++) {
        auto index_res = switch_base(i, depth, 2, 3);
        // cout << fmt::format("result[{}] <- query[{}]={:b}", index_res, i, query[i].reveal<uint32_t>()) << endl;
        result[index_res] = query[i];
    }
    for (size_t dim = 0; dim < depth; dim++) {
        comp_bits[dim] = {};
        size_t right_length = depth - dim - 1;
        for (size_t i = 0; i < ipow(3, dim); i++) {
            auto index = get_subcube_indices(i, dim);
            index.resize(depth);
            for (size_t j = 0; j < (1 << right_length); j++) {
                // cout << fmt::format("dim {} i {} j {}", dim, i, j) << endl;
                auto j_index = get_subcube_indices(j, right_length, 2);
                std::copy(j_index.begin(), j_index.end(), index.begin() + dim + 1);
                // cout << "j_index" << endl;
                // for (auto idx : j_index) {
                //     cout << fmt::format("{} ", idx);
                // }
                // cout << endl;
                // cout << "index" << endl;
                // for (auto idx : index) {
                //     cout << fmt::format("{} ", idx);
                // }
                // cout << endl;
                auto [idx1, idx2, idx3] = expand(index, depth, dim);
                auto b1 = result[idx1][bitlength-dim-1], b2 = result[idx2][bitlength-dim-1];
                std::map<size_t, Bit> equality = {{0b00, (!b1) & (!b2)}, {0b10, b1 & (!b2)}, {0b11, b1 & b2}};
                // cout << fmt::format("{} {} {} {} {} {} {}", idx1, idx2, idx3, dim, depth, b1.reveal(), b2.reveal()) << endl;
                // print equality
                // for (auto eq : equality) {
                //     cout << fmt::format("{} ", eq.reveal());
                // }
                // cout << comp_bits[dim].size() << endl;
                swap(equality[0b10], result[idx1], result[idx2]);
                result[idx3] = If(b1, result[idx1], result[idx2]);
                comp_bits[dim].push_back(equality);
                // cout << fmt::format("result[{}] <- query[{}]={}", idx3, result[idx1][dim].reveal() ? idx1 : idx2, result[idx3].reveal<uint32_t>()) << endl;
            }
        }
    }
    SubcubeContext context{
        comp_bits, 
        batch_size, 
        depth, 
        encoded_size,
        bitlength
    };
    query = result;
    return context;
}

void subcube_response_collect(IntegerArray& response, const SubcubeContext& context) {
    auto [comp_bits, batch_size, depth, encoded_size, bitlength] = context;

    for (int dim = depth-1; dim >= 0; dim--) {
        auto records = comp_bits[dim];
        size_t right_length = depth - dim - 1;
        for (size_t i = 0; i < ipow(3, dim); i++) {
            auto index = get_subcube_indices(i, dim);
            index.resize(depth);
            for (size_t j = 0; j < (1 << right_length); j++) {
                // cout << fmt::format("dim {} i {} j {}", dim, i, j) << endl;
                auto j_index = get_subcube_indices(j, right_length, 2);
                std::copy(j_index.begin(), j_index.end(), index.begin() + dim + 1);
                auto [idx1, idx2, idx3] = expand(index, depth, dim);
                // cout << fmt::format("{} {} {} {} {}", idx1, idx2, idx3, dim, depth) << endl;
                auto equality = records[i*(1 << right_length) + j];
                //print equality
                // for (auto eq : equality) {
                //     cout << fmt::format("{} ", eq.reveal());
                // }
                // cout << i*(1 << right_length) + j << endl;
                auto& r1 = response[idx1];
                auto& r2 = response[idx2];
                auto& r3 = response[idx3];
                r2 = r2 ^ If(equality[0b00], r3, Integer(bitlength, 0));
                r1 = r1 ^ If(equality[0b11], r3, Integer(bitlength, 0));
                swap(equality[0b10], r1, r2);
                // cout << fmt::format("response[{}] <- {}", idx1, r1.reveal<uint32_t>()) << endl;
                // cout << fmt::format("response[{}] <- {}", idx2, r2.reveal<uint32_t>()) << endl;
            }
        }
    }
    IntegerArray result(batch_size);
    for (size_t i = 0; i < batch_size; i++) {
        auto subcube_index = get_subcube_indices(i, depth, 2);
        size_t index_res = get_original_indices(subcube_index, depth);
        result[i] = response[index_res];
    }
    response = result;
}

} // namespace sci
