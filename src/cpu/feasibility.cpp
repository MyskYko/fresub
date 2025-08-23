#include "feasibility.hpp"

#include <iostream>

namespace fresub {

  // Multi-word implementation of gresub feasibility check - returns true if feasible
  bool solve_resub_overlap_multiword(int i, int j, int k, int l, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;
    // Process all words using bitwise operations
    uint64_t qs[32] = {0};
    for (int word_idx = 0; word_idx < num_words; word_idx++) {
      uint64_t t_on = truth_tables.back()[word_idx];
      uint64_t t_off = ~t_on;
      uint64_t t_i = truth_tables[i][word_idx];
      uint64_t t_j = truth_tables[j][word_idx];
      uint64_t t_k = truth_tables[k][word_idx];
      uint64_t t_l = truth_tables[l][word_idx];
      qs[0]  |= t_off &  t_i &  t_j &  t_k &  t_l;
      qs[1]  |= t_on  &  t_i &  t_j &  t_k &  t_l;
      qs[2]  |= t_off & ~t_i &  t_j &  t_k &  t_l;
      qs[3]  |= t_on  & ~t_i &  t_j &  t_k &  t_l;
      qs[4]  |= t_off &  t_i & ~t_j &  t_k &  t_l;
      qs[5]  |= t_on  &  t_i & ~t_j &  t_k &  t_l;
      qs[6]  |= t_off & ~t_i & ~t_j &  t_k &  t_l;
      qs[7]  |= t_on  & ~t_i & ~t_j &  t_k &  t_l;
      qs[8]  |= t_off &  t_i &  t_j & ~t_k &  t_l;
      qs[9]  |= t_on  &  t_i &  t_j & ~t_k &  t_l;
      qs[10] |= t_off & ~t_i &  t_j & ~t_k &  t_l;
      qs[11] |= t_on  & ~t_i &  t_j & ~t_k &  t_l;
      qs[12] |= t_off &  t_i & ~t_j & ~t_k &  t_l;
      qs[13] |= t_on  &  t_i & ~t_j & ~t_k &  t_l;
      qs[14] |= t_off & ~t_i & ~t_j & ~t_k &  t_l;
      qs[15] |= t_on  & ~t_i & ~t_j & ~t_k &  t_l;
      qs[16] |= t_off &  t_i &  t_j &  t_k & ~t_l;
      qs[17] |= t_on  &  t_i &  t_j &  t_k & ~t_l;
      qs[18] |= t_off & ~t_i &  t_j &  t_k & ~t_l;
      qs[19] |= t_on  & ~t_i &  t_j &  t_k & ~t_l;
      qs[20] |= t_off &  t_i & ~t_j &  t_k & ~t_l;
      qs[21] |= t_on  &  t_i & ~t_j &  t_k & ~t_l;
      qs[22] |= t_off & ~t_i & ~t_j &  t_k & ~t_l;
      qs[23] |= t_on  & ~t_i & ~t_j &  t_k & ~t_l;
      qs[24] |= t_off &  t_i &  t_j & ~t_k & ~t_l;
      qs[25] |= t_on  &  t_i &  t_j & ~t_k & ~t_l;
      qs[26] |= t_off & ~t_i &  t_j & ~t_k & ~t_l;
      qs[27] |= t_on  & ~t_i &  t_j & ~t_k & ~t_l;
      qs[28] |= t_off &  t_i & ~t_j & ~t_k & ~t_l;
      qs[29] |= t_on  &  t_i & ~t_j & ~t_k & ~t_l;
      qs[30] |= t_off & ~t_i & ~t_j & ~t_k & ~t_l;
      qs[31] |= t_on  & ~t_i & ~t_j & ~t_k & ~t_l;
    }
    // Check feasibility using same logic as original
    bool r = true;
    for (int h = 0; h < 16; h++) {
      if ((qs[2*h] != 0) && (qs[2*h+1] != 0)) {
	r = false;
      }
    }
    return r;
  }

  // Find all feasible 4-input resubstitution combinations
  std::vector<std::vector<int>> find_feasible_4resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    std::vector<std::vector<int>> feasible_combinations;
    if (truth_tables.size() < 5) {
      return feasible_combinations; // Need at least 4 divisors
    }
    int n_divisors = truth_tables.size() - 1;
    // Enumerate all combinations of 4 divisors and collect all feasible ones
    for (int i = 0; i < n_divisors; i++) {
      for (int j = i + 1; j < n_divisors; j++) {
	for (int k = j + 1; k < n_divisors; k++) {
	  for (int l = k + 1; l < n_divisors; l++) {
	    bool is_feasible = solve_resub_overlap_multiword(i, j, k, l, truth_tables, num_inputs);
	    if (is_feasible) {
	      feasible_combinations.push_back({static_cast<int>(i), static_cast<int>(j), static_cast<int>(k), static_cast<int>(l)});
	    }
	  }
	}
      }
    }
    return feasible_combinations;
  }

  void feasibility_check_cpu(std::vector<Window>::iterator it, std::vector<Window>::iterator end) {
    while(it != end) {
      it->feasible_combinations = find_feasible_4resub(it->truth_tables, it->inputs.size());
      it++;
    }
  }

} // namespace fresub
