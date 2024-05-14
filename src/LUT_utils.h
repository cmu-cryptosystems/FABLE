#include <algorithm>
#include "utils/net_io_channel.h"
#include <fmt/core.h>
#include "utils.h"

enum LUTType {
	Random, 
	Gamma, 
	Cauchy_dis, 
    Filled, 
    NumLUTTypes
};

// string representation of lut type
inline std::string lut_type_to_string(LUTType lut_typ) {
    if (lut_typ == Random)
        return "Random";
    else if (lut_typ == Gamma)
        return "Gamma";
    else if (lut_typ == Cauchy_dis)
        return "Cauchy_dis";
    else if (lut_typ == Filled)
        return "Filled";
    else
        return "Invalid";
}

const int client_id = 0;
const uint64_t db_size = (1 << LUT_INPUT_SIZE);

inline void barrier(int party, sci::NetIO* io_gc) {
	bool prepared = false;
	if (party == sci::BOB) {
		prepared = true;
		io_gc->send_data(&prepared, sizeof(prepared));
	} else {
		io_gc->recv_data(&prepared, sizeof(prepared));
		utils::check(prepared, "[BatchLUT] Synchronization failed. ");
	}
}

// Convert double to fixed point representation (scale x from [0, input_range) to [0, fixedpoint_range), then truncate the fractional part)
inline uint64_t ftoi(double x, long double fixedpoint_range = db_size, double input_range = 10) {
	return std::round(std::clamp(x / input_range, -1., 1.) * fixedpoint_range);
}

// Convert fixed point representation to double
inline double itof(uint64_t x, long double fixedpoint_range = db_size, double input_range = 10) {
	return x * input_range / fixedpoint_range;
}

inline std::vector<uint64_t> get_lut(LUTType lut_typ, int input_bits = LUT_INPUT_SIZE, int output_bits = LUT_OUTPUT_SIZE) {
    uint64_t lut_size = 1 << input_bits;
    long double range = pow(2.0L, output_bits);
	std::vector<uint64_t> lut(lut_size);
	std::vector<double> abs_error(lut_size, 0);
	std::vector<double> rel_error(lut_size, 0);
	
	for (uint64_t i = 0; i < lut_size; i ++) {
		if (lut_typ == Random) {
			lut[i] = rand() % (uint64_t)range;
        } else if (lut_typ == Gamma) {
			double input = (double)i/lut_size * 3 + 1; // from 1 to 4
			double value = std::tgamma(input);
            assert (value < 10);
			lut[i] = ftoi(value, range);
			abs_error[i] = abs(itof(lut[i], range) - value);
			rel_error[i] = abs_error[i] / value;
		} else if (lut_typ == Filled) {
            lut[i] = range - 1;
        } else if (lut_typ == Cauchy_dis) {
			double input = (double)i/lut_size * 80 - 40; // from -40 to 40
			double value = 1.0 / (M_PI * (1 + input * input));
			lut[i] = ftoi(value, range, 0.35);
			abs_error[i] = abs(itof(lut[i], range, 0.35) - value);
			rel_error[i] = abs_error[i] / value;
		} else {
			assert(false);
		}
	}
	std::cout << fmt::format(
		"LUT built. \nMax Absolute error = {}\nMax Relative error = {}", 
		*std::max_element(abs_error.begin(), abs_error.end()),
		*std::max_element(rel_error.begin(), rel_error.end())
	) << std::endl;
	return lut;
}