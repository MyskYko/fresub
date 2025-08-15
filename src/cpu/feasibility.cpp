#include "feasibility.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <iostream>

namespace fresub {


// CPU implementation of gresub feasibility check
uint32_t solve_resub_overlap_cpu(int i, int j, int k, int l, 
                                const std::vector<uint64_t>& truth_tables,
                                uint64_t target_tt, int num_inputs) {
    
    if (i >= truth_tables.size() || j >= truth_tables.size() || 
        k >= truth_tables.size() || l >= truth_tables.size()) {
        return 0;
    }
    
    uint32_t res = ((1U << (i % 30)) | (1U << (j % 30)) | (1U << (k % 30)) | (1U << (l % 30)));
    uint32_t qs[32] = {0};
    
    int num_patterns = 1 << num_inputs;
    if (num_patterns > 64) return 0;
    
    uint64_t t_i = truth_tables[i];
    uint64_t t_j = truth_tables[j];
    uint64_t t_k = truth_tables[k];
    uint64_t t_l = truth_tables[l];
    
    uint64_t mask = (1ULL << num_patterns) - 1;
    uint64_t t_on = target_tt & mask;
    uint64_t t_off = (~target_tt) & mask;
    
    for (int p = 0; p < num_patterns; p++) {
        uint32_t bit_i = (t_i >> p) & 1;
        uint32_t bit_j = (t_j >> p) & 1;
        uint32_t bit_k = (t_k >> p) & 1;
        uint32_t bit_l = (t_l >> p) & 1;
        uint32_t bit_off = (t_off >> p) & 1;
        uint32_t bit_on = (t_on >> p) & 1;
        
        qs[0]  |= (bit_off &  bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[1]  |= (bit_on  &  bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[2]  |= (bit_off & ~bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[3]  |= (bit_on  & ~bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[4]  |= (bit_off &  bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[5]  |= (bit_on  &  bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[6]  |= (bit_off & ~bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[7]  |= (bit_on  & ~bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[8]  |= (bit_off &  bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[9]  |= (bit_on  &  bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[10] |= (bit_off & ~bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[11] |= (bit_on  & ~bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[12] |= (bit_off &  bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[13] |= (bit_on  &  bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[14] |= (bit_off & ~bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[15] |= (bit_on  & ~bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[16] |= (bit_off &  bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[17] |= (bit_on  &  bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[18] |= (bit_off & ~bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[19] |= (bit_on  & ~bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[20] |= (bit_off &  bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[21] |= (bit_on  &  bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[22] |= (bit_off & ~bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[23] |= (bit_on  & ~bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[24] |= (bit_off &  bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[25] |= (bit_on  &  bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[26] |= (bit_off & ~bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[27] |= (bit_on  & ~bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[28] |= (bit_off &  bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[29] |= (bit_on  &  bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[30] |= (bit_off & ~bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[31] |= (bit_on  & ~bit_i & ~bit_j) & (~bit_k & ~bit_l);
    }
    
    for (int h = 0; h < 16; h++) {
        int fail = ((qs[2*h] != 0) && (qs[2*h+1] != 0));
        res = fail ? 0 : res;
    }
    
    return res;
}

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

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(uint64_t target_tt, 
                            const std::vector<uint64_t>& divisor_tts,
                            const std::vector<int>& selected_divisors,
                            int num_inputs,
                            const std::vector<int>& window_inputs,
                            const std::vector<int>& all_divisors,
                            std::vector<std::vector<bool>>& br,
                            std::vector<std::vector<bool>>& sim) {
    
    int num_patterns = 1 << num_inputs;
    
    // Filter out divisors that are window inputs
    std::set<int> window_input_set(window_inputs.begin(), window_inputs.end());
    std::vector<int> non_input_divisor_indices;
    
    for (int idx : selected_divisors) {
        if (idx >= 0 && idx < all_divisors.size()) {
            int divisor_node = all_divisors[idx];
            if (window_input_set.find(divisor_node) == window_input_set.end()) {
                non_input_divisor_indices.push_back(idx);
            }
        }
    }
    
    // Initialize br[input_pattern][output_pattern]
    br.clear();
    br.resize(num_patterns, std::vector<bool>(2, false));
    
    // Initialize sim[input_pattern][non_input_divisor]
    sim.clear();
    sim.resize(num_patterns, std::vector<bool>(non_input_divisor_indices.size(), false));
    
    for (int p = 0; p < num_patterns; p++) {
        // Set target output value for this input pattern
        bool target_value = (target_tt >> p) & 1;
        br[p][target_value ? 1 : 0] = true;
        
        // Set non-input divisor values for this input pattern
        for (size_t d = 0; d < non_input_divisor_indices.size(); d++) {
            int divisor_idx = non_input_divisor_indices[d];
            bool divisor_value = (divisor_tts[divisor_idx] >> p) & 1;
            sim[p][d] = divisor_value;
        }
    }
}

} // namespace fresub