#include "LUT_utils.h"
#include "GC/emp-sh2pc.h"
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

int lut_type = 2, input_bits = 16, output_bits = 16;

string gen_header() {
    string res = fmt::format("#STATISTICS\n#ORIGINAL LUTS: {}\n\n#GROUPED LUTS: 1\n#DEPTH: 1\n#XOR: 0\n#NOT: 0\n#XNOR: 0\n#ASSIGN: 0\n\n", input_bits);

    res += fmt::format("#INPUTS {}\n", input_bits);
    for (int i = 0; i < input_bits; i++) {
        res += fmt::format(" addr[{}]", i);
    }
    res += "\n";

    res += fmt::format("#OUTPUTS {}\n", output_bits);
    for (int i = 0; i < output_bits; i++) {
        res += fmt::format(" dout[{}]", i);
    }
    res += "\n";
    res += "#constant zero\n0\n#constant one\n1\n#LUTs\n";
    return res;
}

string dump_lut(vector<uint64_t> lut) {
    assert (lut.size() == (1 << input_bits));
    string body = "";
    int c = 0;
    for (int i = 0; i < lut.size(); i++) {
        string input = utils::block(i).to_string().substr(utils::blocksize - input_bits);
        assert (utils::block(i).to_string().find('1') >= utils::blocksize - input_bits);
        assert (lut[i] < (1 << output_bits));
        assert (utils::block(lut[i]).to_string().find('1') >= utils::blocksize - output_bits);
        string output = utils::block(lut[i]).to_string().substr(utils::blocksize - output_bits);

        int input_ones = count(input.begin(), input.end(), '1');
        
        for (int j = 0; j < output_bits; j++) {
            if (output[j] == '1') {
                c++;
                body += fmt::format(" {} {} {:#x} dout[{}]", input_ones, input, 1, j);
            }
        }
    }
    body += "\n";
    string header = gen_header();
    header += fmt::format("LUT {} {}", input_bits, c);
    for (int i = 0; i < input_bits; i++) {
        header += fmt::format(" addr[{}]", i);
    }
    return header + body;
}

int main(int argc, char** argv) {

    ArgMapping amap;
    amap.arg("t", lut_type, "LUTType: Random = 0; Gamma = 1, Filled = 2 (Default: 2)");
    amap.arg("i", input_bits, "Number of input bits (Default: 16)");
    amap.arg("o", output_bits, "Number of output bits (Default: 16)");
    amap.parse(argc, argv);

    utils::check(lut_type < NumLUTTypes && lut_type >= 0, "Invalid LUT type");
    utils::check(input_bits > 0, "Invalid input bits");
    utils::check(output_bits > 0, "Invalid output bits");

    vector<uint64_t> lut = get_lut((LUTType)lut_type, input_bits, output_bits);

    string filename = fmt::format("flute_luts/LUT_{}_{}_{}.lut", input_bits, output_bits, lut_type_to_string((LUTType)lut_type));
    std::ofstream out(filename);

    out << dump_lut(lut);

    return 0;
}
    