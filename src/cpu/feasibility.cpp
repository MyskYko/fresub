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

  // --- Skeletons for 0..3-input overlap feasibility (to be implemented) ---
  bool solve_resub_overlap_multiword_0(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    if (truth_tables.empty()) return false;
    const auto& target = truth_tables.back();
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;

    bool all_zero = true;
    bool all_one = true;
    for (int word_idx = 0; word_idx < num_words; word_idx++) {
      uint64_t t = target[word_idx];
      if (t) all_zero = false;     // some 1s present -> not all zero
      if (~t) all_one = false;     // some 0s present -> not all ones
      if (!all_zero && !all_one) break; // early exit
    }
    return all_zero || all_one;
  }

  bool solve_resub_overlap_multiword_1(int i, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    // Accumulate conflicts per divisor pattern using the same style as 4-input version
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;
    uint64_t qs[4] = {0};
    for (int word_idx = 0; word_idx < num_words; word_idx++) {
      uint64_t t_on = truth_tables.back()[word_idx];
      uint64_t t_off = ~t_on;
      uint64_t t_i = truth_tables[i][word_idx];
      qs[0] |= t_off &  t_i; // pattern: i=1, target=0
      qs[1] |= t_on  &  t_i; // pattern: i=1, target=1
      qs[2] |= t_off & ~t_i; // pattern: i=0, target=0
      qs[3] |= t_on  & ~t_i; // pattern: i=0, target=1
    }
    bool r = true;
    for (int h = 0; h < 2; h++) {
      if ((qs[2*h] != 0) && (qs[2*h+1] != 0)) {
        r = false;
      }
    }
    return r;
  }

  bool solve_resub_overlap_multiword_2(int i, int j, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    // Mirror 4-input style with 2 divisors => 4 patterns (00,01,10,11), each with onset/offset
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;
    uint64_t qs[8] = {0};
    for (int word_idx = 0; word_idx < num_words; word_idx++) {
      uint64_t t_on = truth_tables.back()[word_idx];
      uint64_t t_off = ~t_on;
      uint64_t t_i = truth_tables[i][word_idx];
      uint64_t t_j = truth_tables[j][word_idx];
      // pattern 11
      qs[0] |= t_off &  t_i &  t_j;
      qs[1] |= t_on  &  t_i &  t_j;
      // pattern 01
      qs[2] |= t_off & ~t_i &  t_j;
      qs[3] |= t_on  & ~t_i &  t_j;
      // pattern 10
      qs[4] |= t_off &  t_i & ~t_j;
      qs[5] |= t_on  &  t_i & ~t_j;
      // pattern 00
      qs[6] |= t_off & ~t_i & ~t_j;
      qs[7] |= t_on  & ~t_i & ~t_j;
    }
    bool r = true;
    for (int h = 0; h < 4; h++) {
      if ((qs[2*h] != 0) && (qs[2*h+1] != 0)) {
        r = false;
      }
    }
    return r;
  }

  bool solve_resub_overlap_multiword_3(int i, int j, int k, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    // Mirror 4-input style with 3 divisors => 8 patterns, each with onset/offset
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;
    uint64_t qs[16] = {0};
    for (int word_idx = 0; word_idx < num_words; word_idx++) {
      uint64_t t_on = truth_tables.back()[word_idx];
      uint64_t t_off = ~t_on;
      uint64_t t_i = truth_tables[i][word_idx];
      uint64_t t_j = truth_tables[j][word_idx];
      uint64_t t_k = truth_tables[k][word_idx];
      // 111
      qs[0]  |= t_off &  t_i &  t_j &  t_k;
      qs[1]  |= t_on  &  t_i &  t_j &  t_k;
      // 011
      qs[2]  |= t_off & ~t_i &  t_j &  t_k;
      qs[3]  |= t_on  & ~t_i &  t_j &  t_k;
      // 101
      qs[4]  |= t_off &  t_i & ~t_j &  t_k;
      qs[5]  |= t_on  &  t_i & ~t_j &  t_k;
      // 001
      qs[6]  |= t_off & ~t_i & ~t_j &  t_k;
      qs[7]  |= t_on  & ~t_i & ~t_j &  t_k;
      // 110
      qs[8]  |= t_off &  t_i &  t_j & ~t_k;
      qs[9]  |= t_on  &  t_i &  t_j & ~t_k;
      // 010
      qs[10] |= t_off & ~t_i &  t_j & ~t_k;
      qs[11] |= t_on  & ~t_i &  t_j & ~t_k;
      // 100
      qs[12] |= t_off &  t_i & ~t_j & ~t_k;
      qs[13] |= t_on  &  t_i & ~t_j & ~t_k;
      // 000
      qs[14] |= t_off & ~t_i & ~t_j & ~t_k;
      qs[15] |= t_on  & ~t_i & ~t_j & ~t_k;
    }
    bool r = true;
    for (int h = 0; h < 8; h++) {
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

  // --- Skeletons for k=0..3 enumerators (to be implemented) ---
  std::vector<std::vector<int>> find_feasible_0resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    std::vector<std::vector<int>> combos;
    if (solve_resub_overlap_multiword_0(truth_tables, num_inputs)) {
      combos.push_back({}); // empty selection represents constant solution
    }
    return combos;
  }

  std::vector<std::vector<int>> find_feasible_1resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    std::vector<std::vector<int>> combos;
    int n_divisors = static_cast<int>(truth_tables.size()) - 1;
    for (int i = 0; i < n_divisors; i++) {
      if (solve_resub_overlap_multiword_1(i, truth_tables, num_inputs)) {
        combos.push_back({i});
      }
    }
    return combos;
  }

  std::vector<std::vector<int>> find_feasible_2resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    std::vector<std::vector<int>> combos;
    int n_divisors = static_cast<int>(truth_tables.size()) - 1;
    for (int i = 0; i < n_divisors; i++) {
      for (int j = i + 1; j < n_divisors; j++) {
        if (solve_resub_overlap_multiword_2(i, j, truth_tables, num_inputs)) {
          combos.push_back({i, j});
        }
      }
    }
    return combos;
  }

  std::vector<std::vector<int>> find_feasible_3resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs) {
    std::vector<std::vector<int>> combos;
    int n_divisors = static_cast<int>(truth_tables.size()) - 1;
    for (int i = 0; i < n_divisors; i++) {
      for (int j = i + 1; j < n_divisors; j++) {
        for (int k = j + 1; k < n_divisors; k++) {
          if (solve_resub_overlap_multiword_3(i, j, k, truth_tables, num_inputs)) {
            combos.push_back({i, j, k});
          }
        }
      }
    }
    return combos;
  }

  void feasibility_check_cpu(std::vector<Window>::iterator it, std::vector<Window>::iterator end) {
    while(it != end) {
      it->feasible_combinations = find_feasible_4resub(it->truth_tables, it->inputs.size());
      it++;
    }
  }

  // --- Skeletons for CPU feasibility modes (to be implemented) ---
  void feasibility_check_cpu_all(std::vector<Window>::iterator it, std::vector<Window>::iterator end) {
    while (it != end) {
      const auto& tts = it->truth_tables;
      int num_inputs = static_cast<int>(it->inputs.size());
      int n_div = static_cast<int>(tts.size()) - 1;
      int k = std::min(4, n_div);
      std::vector<std::vector<int>> comb;
      if (k == 0)        comb = find_feasible_0resub(tts, num_inputs);
      else if (k == 1)   comb = find_feasible_1resub(tts, num_inputs);
      else if (k == 2)   comb = find_feasible_2resub(tts, num_inputs);
      else if (k == 3)   comb = find_feasible_3resub(tts, num_inputs);
      else /* k == 4 */  comb = find_feasible_4resub(tts, num_inputs);
      it->feasible_combinations = std::move(comb);
      ++it;
    }
  }

  void feasibility_check_cpu_min(std::vector<Window>::iterator it, std::vector<Window>::iterator end) {
    while (it != end) {
      const auto& tts = it->truth_tables;
      int num_inputs = static_cast<int>(it->inputs.size());
      int n_div = static_cast<int>(tts.size()) - 1;

      auto comb = find_feasible_0resub(tts, num_inputs);
      if (comb.empty() && n_div >= 1) comb = find_feasible_1resub(tts, num_inputs);
      if (comb.empty() && n_div >= 2) comb = find_feasible_2resub(tts, num_inputs);
      if (comb.empty() && n_div >= 3) comb = find_feasible_3resub(tts, num_inputs);
      if (comb.empty() && n_div >= 4) comb = find_feasible_4resub(tts, num_inputs);

      it->feasible_combinations = std::move(comb);
      ++it;
    }
  }

} // namespace fresub
