#include "feasibility.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <iostream>

namespace fresub {



// Multi-word implementation of gresub feasibility check - returns true if feasible
bool solve_resub_overlap_multiword(int i, int j, int k, int l,
                                  const std::vector<std::vector<uint64_t>>& truth_tables,
                                  const std::vector<uint64_t>& target_tt, int num_inputs) {
    
    if (i >= truth_tables.size() || j >= truth_tables.size() || 
        k >= truth_tables.size() || l >= truth_tables.size()) {
        return false;
    }
    
    if (truth_tables[i].empty() || truth_tables[j].empty() || 
        truth_tables[k].empty() || truth_tables[l].empty() || target_tt.empty()) {
        return false;
    }
    
    uint64_t qs[32] = {0};
    
    int num_patterns = 1 << num_inputs;
    size_t num_words = (num_patterns + 63) / 64;  // Ceiling division
    
    // Process all words using bitwise operations
    for (size_t word_idx = 0; word_idx < num_words; word_idx++) {
        if (word_idx >= truth_tables[i].size() || word_idx >= truth_tables[j].size() ||
            word_idx >= truth_tables[k].size() || word_idx >= truth_tables[l].size() ||
            word_idx >= target_tt.size()) {
            continue;
        }
        
        uint64_t t_i = truth_tables[i][word_idx];
        uint64_t t_j = truth_tables[j][word_idx];
        uint64_t t_k = truth_tables[k][word_idx];
        uint64_t t_l = truth_tables[l][word_idx];
        uint64_t target_word = target_tt[word_idx];
        
        // Apply mask for partial words (last word might have fewer than 64 bits)
        uint64_t mask = ~0ULL;
        if (word_idx == num_words - 1 && (num_patterns % 64) != 0) {
            mask = (1ULL << (num_patterns % 64)) - 1;
        }
        
        uint64_t t_on = target_word & mask;
        uint64_t t_off = (~target_word) & mask;
        
        // Compute all combinations using bitwise operations (64 bits at once!)
        qs[0]  |= t_off &  t_i &  t_j &  t_k &  t_l;    // off, i, j, k, l
        qs[1]  |= t_on  &  t_i &  t_j &  t_k &  t_l;    // on,  i, j, k, l
        qs[2]  |= t_off & ~t_i &  t_j &  t_k &  t_l;    // off, ~i, j, k, l
        qs[3]  |= t_on  & ~t_i &  t_j &  t_k &  t_l;    // on,  ~i, j, k, l
        qs[4]  |= t_off &  t_i & ~t_j &  t_k &  t_l;    // off, i, ~j, k, l
        qs[5]  |= t_on  &  t_i & ~t_j &  t_k &  t_l;    // on,  i, ~j, k, l
        qs[6]  |= t_off & ~t_i & ~t_j &  t_k &  t_l;    // off, ~i, ~j, k, l
        qs[7]  |= t_on  & ~t_i & ~t_j &  t_k &  t_l;    // on,  ~i, ~j, k, l
        qs[8]  |= t_off &  t_i &  t_j & ~t_k &  t_l;    // off, i, j, ~k, l
        qs[9]  |= t_on  &  t_i &  t_j & ~t_k &  t_l;    // on,  i, j, ~k, l
        qs[10] |= t_off & ~t_i &  t_j & ~t_k &  t_l;    // off, ~i, j, ~k, l
        qs[11] |= t_on  & ~t_i &  t_j & ~t_k &  t_l;    // on,  ~i, j, ~k, l
        qs[12] |= t_off &  t_i & ~t_j & ~t_k &  t_l;    // off, i, ~j, ~k, l
        qs[13] |= t_on  &  t_i & ~t_j & ~t_k &  t_l;    // on,  i, ~j, ~k, l
        qs[14] |= t_off & ~t_i & ~t_j & ~t_k &  t_l;    // off, ~i, ~j, ~k, l
        qs[15] |= t_on  & ~t_i & ~t_j & ~t_k &  t_l;    // on,  ~i, ~j, ~k, l
        qs[16] |= t_off &  t_i &  t_j &  t_k & ~t_l;    // off, i, j, k, ~l
        qs[17] |= t_on  &  t_i &  t_j &  t_k & ~t_l;    // on,  i, j, k, ~l
        qs[18] |= t_off & ~t_i &  t_j &  t_k & ~t_l;    // off, ~i, j, k, ~l
        qs[19] |= t_on  & ~t_i &  t_j &  t_k & ~t_l;    // on,  ~i, j, k, ~l
        qs[20] |= t_off &  t_i & ~t_j &  t_k & ~t_l;    // off, i, ~j, k, ~l
        qs[21] |= t_on  &  t_i & ~t_j &  t_k & ~t_l;    // on,  i, ~j, k, ~l
        qs[22] |= t_off & ~t_i & ~t_j &  t_k & ~t_l;    // off, ~i, ~j, k, ~l
        qs[23] |= t_on  & ~t_i & ~t_j &  t_k & ~t_l;    // on,  ~i, ~j, k, ~l
        qs[24] |= t_off &  t_i &  t_j & ~t_k & ~t_l;    // off, i, j, ~k, ~l
        qs[25] |= t_on  &  t_i &  t_j & ~t_k & ~t_l;    // on,  i, j, ~k, ~l
        qs[26] |= t_off & ~t_i &  t_j & ~t_k & ~t_l;    // off, ~i, j, ~k, ~l
        qs[27] |= t_on  & ~t_i &  t_j & ~t_k & ~t_l;    // on,  ~i, j, ~k, ~l
        qs[28] |= t_off &  t_i & ~t_j & ~t_k & ~t_l;    // off, i, ~j, ~k, ~l
        qs[29] |= t_on  &  t_i & ~t_j & ~t_k & ~t_l;    // on,  i, ~j, ~k, ~l
        qs[30] |= t_off & ~t_i & ~t_j & ~t_k & ~t_l;    // off, ~i, ~j, ~k, ~l
        qs[31] |= t_on  & ~t_i & ~t_j & ~t_k & ~t_l;    // on,  ~i, ~j, ~k, ~l
    }
    
    // Check feasibility using same logic as original
    for (int h = 0; h < 16; h++) {
        if ((qs[2*h] != 0) && (qs[2*h+1] != 0)) {
            return false; // Not feasible
        }
    }
    
    return true; // Feasible
}

// Find all feasible 4-input resubstitution combinations
std::vector<std::vector<int>> find_feasible_4resub(
    const std::vector<std::vector<uint64_t>>& divisor_truth_tables,
    const std::vector<uint64_t>& target_truth_table,
    int num_inputs) {
    
    std::vector<std::vector<int>> feasible_combinations;
    
    if (divisor_truth_tables.size() < 4) {
        return feasible_combinations; // Need at least 4 divisors
    }
    
    // Enumerate all combinations of 4 divisors and collect all feasible ones
    for (size_t i = 0; i < divisor_truth_tables.size(); i++) {
        for (size_t j = i + 1; j < divisor_truth_tables.size(); j++) {
            for (size_t k = j + 1; k < divisor_truth_tables.size(); k++) {
                for (size_t l = k + 1; l < divisor_truth_tables.size(); l++) {
                    bool is_feasible = solve_resub_overlap_multiword(i, j, k, l, divisor_truth_tables, 
                                                                     target_truth_table, num_inputs);
                    if (is_feasible) {
                        feasible_combinations.push_back({static_cast<int>(i), static_cast<int>(j), 
                                                        static_cast<int>(k), static_cast<int>(l)});
                    }
                }
            }
        }
    }
    
    return feasible_combinations;
}


} // namespace fresub