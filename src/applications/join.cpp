#include "GC/sort.h"
#include "LUT_utils.h"

#include "GC/emp-sh2pc.h"
#include "GC/lookup.h"
#include "utils/io_utils.h"
#include <cstdint>

using namespace sci;


int party, port = 8000, parallel = 1, num_threads = 32, seed = 12345, baseline = 0;
NetIO *io_gc;

typedef vector<uint32_t> PlainField;
typedef vector<PlainField> PlainTable; // The first column is PK
typedef vector<IntegerArray> Table; // The first column is PK

// FABLE is a drop-in replacement for PK-PK join and PK-FK join. 
// Suppose we have three tables:
// customer: [c_ssn (PK), c_orderkey], 4096 rows, held by BOB
// balance: [b_ssn (PK), b_acctbal], 4096 rows, held by ALICE
// orders: [o_orderkey (PK), o_totalprice], 1M rows, held by ALICE
// We want to make the following query: 
// select COUNT(*) from customer, balance, orders where c_ssn == b_ssn and c_orderkey == o_orderkey and b_acctbal < o_totalprice

const size_t customer_size = 4096; 
const size_t balance_size = 4096; 
const size_t orders_size = (1 << 20); 
const size_t ssn_range = 10000; 
const size_t acctbal_range = 1000; 
const size_t totalprice_range = 1000; 

Table share(PlainTable& pt, int src_party) {
	auto num_fields = pt.size();
	auto num_rows = pt[0].size();

	auto buffer = new bool[num_fields * num_rows * 32];
	if (src_party == party) {
		for (int field_idx = 0; field_idx < num_fields; field_idx++) {
			for (int row_idx = 0; row_idx < num_rows; row_idx++) {
				for (int i = 0; i < 32; i++) {
					int_to_bool(buffer + (field_idx * num_rows + row_idx) * 32, pt[field_idx][row_idx], 32);
				}
			}
		}
	} else {
		std::fill(buffer, buffer + num_fields * num_rows * 32, false);
	}
	vector<Bit> bits(num_fields * num_rows * 32);
	// A trick to reduce rounds
	prot_exec->feed((block128 *)bits.data(), src_party, buffer, num_fields * num_rows * 32); 
	delete[] buffer;
	
	Table t(num_fields, IntegerArray(num_rows)); 
	for (int field_idx = 0; field_idx < num_fields; field_idx++) {
		for (int row_idx = 0; row_idx < num_rows; row_idx++) {
			t[field_idx][row_idx].bits = vector<Bit>(bits.begin() + (field_idx * num_rows + row_idx) * 32, bits.begin() + (field_idx * num_rows + row_idx + 1) * 32);
		}
	}

	return t;
}

PlainTable reveal(Table& t, int party) {
	PlainTable pt; 
	for (int field_idx = 0; field_idx < t.size(); field_idx++) {
		PlainField field;
		field.reserve(t[field_idx].size());
		for (auto item : t[field_idx])
			field.push_back(item.reveal<uint32_t>(party));
		pt.push_back(field);
	}
	return pt;
}

void print(PlainTable pt, string name) {
	cout << fmt::format("Table: {}", name) << endl;
	for (int field_idx = 0; field_idx < pt.size(); field_idx++) {
		cout << fmt::format("field {}: ", field_idx);
		for (auto item : pt[field_idx])
			cout << item << " ";
		cout << endl;
	}
}

// Join two tables on the first column, assuming no common fields except the field to be join. 
auto join_scs(Table& table1, Table& table2, int n_common_fields = 1) {
	// primary keys should be sorted in ascending order beforehand
	// 0 is invalid

	// Concat two tables
	Table data(table1.size() + table2.size() - n_common_fields); 
	for (int i = 0; i < n_common_fields; i++) {
		data[i] = table2[i];
		data[i].insert(data[i].begin(), table1[i].rbegin(), table1[i].rend()); 
	}
	for (int i = n_common_fields; i < table1.size(); i++) {
		data[i] = IntegerArray(table2[0].size(), Integer(table1[i][0].size(), 0));
		data[i].insert(data[i].begin(), table1[i].rbegin(), table1[i].rend()); 
	}
	for (int i = n_common_fields; i < table2.size(); i++) {
		data[i - n_common_fields + table1.size()] = table2[i];
		auto placeholder = IntegerArray(table1[0].size(), Integer(table2[i][0].size(), 0));
		data[i - n_common_fields + table1.size()].insert(data[i - n_common_fields + table1.size()].begin(), placeholder.begin(), placeholder.end()); 
	}

	CompResultType result;
	std::vector<int> pl;
	bitonic_merge(data, pl, 0, data[0].size(), Bit(true), false, result); 

	Table join_result(data.size());

	for (int i = 1; i < data[0].size(); i += 2) {
		Bit cond = (data[0][i-1] == data[0][i]);
		Bit cond2;
		if (i < data[0].size() - 1) {
			cond2 = (data[0][i] == data[0][i+1]); 
		}
		for (int field_idx = 0; field_idx < data.size(); field_idx++) {
			Integer joined_record;
			if (i < data[0].size() - 1) {
				joined_record = 
					If(cond | cond2, data[field_idx][i], Integer(data[field_idx][i].size(), 0));
				if (field_idx >= n_common_fields) {
					joined_record = joined_record ^ If(cond, data[field_idx][i-1], Integer(data[field_idx][i-1].size(), 0)) ^ 
					If(cond2, data[field_idx][i+1], Integer(data[field_idx][i+1].size(), 0));
				}
			} else {
				joined_record = 
					If(cond, data[field_idx][i], Integer(data[field_idx][i].size(), 0));
				if (field_idx >= n_common_fields) {
					joined_record = joined_record ^ If(cond, data[field_idx][i-1], Integer(data[field_idx][i-1].size(), 0));
				}
			}
			join_result[field_idx].push_back(joined_record);
		}
	}

	return join_result;
}

PlainTable join_cleartext(PlainTable& table1, PlainTable& table2, int n_common_fields = 1) {
	std::map<size_t, size_t> table2_value2index; 
	for (int row_idx = 0; row_idx < table2[0].size(); row_idx++) {
		check(!table2_value2index.count(table2[0][row_idx]), fmt::format("{} occurs twice! ", table2[0][row_idx]));
		table2_value2index[table2[0][row_idx]] = row_idx;
	}

	PlainTable result(table1.size() + table2.size() - n_common_fields);
	for (int row_idx = 0; row_idx < table1[0].size(); row_idx++) {
		if (table2_value2index.count(table1[0][row_idx])) {
			auto row_idx2 = table2_value2index[table1[0][row_idx]]; 
			for (int field_idx = 0; field_idx < result.size(); field_idx++) {
				if (field_idx < table1.size()) {
					result[field_idx].push_back(table1[field_idx][row_idx]);
				} else {
					result[field_idx].push_back(table2[field_idx - table1.size() + n_common_fields][row_idx2]);
				}
			}
		}
	}

	return result;
}

void check_eq(PlainTable& table1, PlainTable& table2) {
	check(table1.size() == table2.size(), fmt::format("Num Columns mismatch! {} != {}", table1.size(), table2.size()));
	for(int field_idx = 0; field_idx < table1.size(); ++field_idx) {
		check(table1[field_idx].size() == table2[field_idx].size(), fmt::format("Field {} size mismatch! {} != {}", field_idx, table1[field_idx].size(), table2[field_idx].size()));
		for (int row_idx = 0; row_idx < table1[field_idx].size(); row_idx ++) {
			check(table1[field_idx][row_idx] == table2[field_idx][row_idx], fmt::format("Field {} row {} mismatch! {} != {}", field_idx, row_idx, table1[field_idx][row_idx], table2[field_idx][row_idx])); 
		}
	}
}

void bench_join() {
	srand(seed);
	
	// Create tables
	PlainTable orders(2, PlainField(orders_size));
	std::iota(orders[0].begin(), orders[0].end(), 1);
	for (int i = 0; i < orders_size; i++) {
		orders[1][i] = rand() % totalprice_range + 1; 
	}

	PlainTable customer(2, PlainField(customer_size));
	std::iota(customer[0].begin(), customer[0].end(), 1);
	for (int i = 0; i < customer_size; i++) {
		do {
			customer[1][i] = rand() % orders_size + 1; 
		} while (std::find(customer[1].begin(), customer[1].end(), customer[1][i]) - customer[1].begin() != i);
	}

	PlainTable balance(2, PlainField(balance_size));
	std::iota(balance[0].begin(), balance[0].end(), 1);
	for (int i = 0; i < balance_size; i++) {
		balance[1][i] = rand() % acctbal_range + 1; 
	}

	// synchronize
	barrier(party, io_gc);
	io_gc->flush();
	
	start_record(io_gc, "Join");
	start_record(io_gc, "Share customer and balance");
	auto secret_customer = share(customer, BOB);
	auto secret_balance = share(balance, ALICE);
	end_record(io_gc, "Share customer and balance");
	start_record(io_gc, "First join");
	auto customer_with_balance = join_scs(secret_customer, secret_balance); // order_key, acctbal
	customer_with_balance.erase(customer_with_balance.begin());
	end_record(io_gc, "First join");

	start_record(io_gc, "Second join");
	Table final_aggregated_result;
	if (baseline) {
		auto secret_orders = share(orders, ALICE);
		sort(customer_with_balance, customer_with_balance[0].size()); 
		final_aggregated_result = join_scs(customer_with_balance, secret_orders); 
		final_aggregated_result.erase(final_aggregated_result.begin());
	} else {
		std::map<uint64_t, uint64_t> lut; 
		for (uint64_t order_idx = 0; order_idx < orders_size; order_idx ++) {
			lut[orders[0][order_idx]] = orders[1][order_idx]; 
		}
		auto lut_params = fable_prepare(
			lut, 
			party, 
			customer_with_balance[0].size(), 
			orders_size, 
			parallel, 
			num_threads, 
			BatchPirType::PIRANA, 
			HashType::LowMC, 
			io_gc
		); 
		auto prices = fable_lookup(customer_with_balance[0], lut_params, false);
		final_aggregated_result = Table{customer_with_balance[1], prices};
	}
	end_record(io_gc, "Second join");
	
	start_record(io_gc, "Count");
	Integer balance_insufficient(32, 0);
	Integer zero(32, 0);
	Integer one(32, 1);
	for (int row_idx = 0; row_idx < final_aggregated_result[0].size(); row_idx++) {
		balance_insufficient = balance_insufficient + If(final_aggregated_result[0][row_idx] < final_aggregated_result[1][row_idx], one, zero); 
	}
	end_record(io_gc, "Count");
	end_record(io_gc, "Join");

	// Verify
	start_record(io_gc, "Verification");
	auto plain_result_customer_balance = reveal(customer_with_balance, BOB);
	auto gt_customer_balance = join_cleartext(customer, balance);
	gt_customer_balance.erase(gt_customer_balance.begin());
	// if (party == BOB)
	// 	check_eq(plain_result_customer_balance, gt_customer_balance);
	
	auto plain_result = reveal(final_aggregated_result, BOB);
	for (auto& col : plain_result)
		col.erase(std::remove(col.begin(), col.end(), 0), col.end());
	auto gt = join_cleartext(gt_customer_balance, orders);
	gt.erase(gt.begin());
	// print(plain_result, "result");
	// print(gt, "gt");
	// if (party == BOB)
	// 	check_eq(plain_result, gt);
	
	uint32_t gt_violated = 0;
	for (int row_idx = 0; row_idx < gt[0].size(); row_idx++) {
		gt_violated += (gt[0][row_idx] < gt[1][row_idx]); 
	}
	auto plain_violated = balance_insufficient.reveal<uint32_t>(BOB);
	if (party == BOB)
		check(gt_violated == plain_violated, fmt::format("{} != {}", plain_violated, gt_violated));

	end_record(io_gc, "Verification");

	if (party == BOB) {
		cout << GREEN << "[Application - Embedding] Test passed" << RESET << endl;
	} else {
		cout << "[Application - Embedding] Validation is carried out on ALICE side. " << endl;
	}

}

int main(int argc, char **argv) {
	
	ArgMapping amap;
	amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
	amap.arg("p", port, "Port Number");
	amap.arg("seed", seed, "random seed");
	amap.arg("par", parallel, "parallel flag: 1 = parallel; 0 = sequential");
	amap.arg("thr", num_threads, "number of threads");
	amap.arg("baseline", baseline, "whether use baseline");
	amap.parse(argc-1, argv+1);
	io_gc = new NetIO(party == ALICE ? nullptr : argv[1],
						port + GC_PORT_OFFSET, true);

	auto time_start = clock_start(); 
	setup_semi_honest(io_gc, party);
	auto time_span = time_from(time_start);
	cout << "General setup: elapsed " << time_span / 1000 << " ms." << endl;
	// utils::check(type == 0, "Only PIRANA is supported now. "); 
	bench_join();
	delete io_gc;
	return 0;
}
