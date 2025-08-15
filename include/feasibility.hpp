#pragma once

#include "fresub_aig.hpp"
#include "window.hpp"
#include <vector>
#include <cstdint>

namespace fresub {

// CPU implementation of gresub feasibility check (legacy single-word)
uint32_t solve_resub_overlap_cpu(int i, int j, int k, int l, 
                                const std::vector<uint64_t>& truth_tables,
                                uint64_t target_tt, int num_inputs);

// Multi-word implementation of gresub feasibility check - returns true if feasible
bool solve_resub_overlap_multiword(int i, int j, int k, int l,
                                  const std::vector<std::vector<uint64_t>>& truth_tables,
                                  const std::vector<uint64_t>& target_tt, int num_inputs);

// Find all feasible 4-input resubstitution combinations
std::vector<std::vector<int>> find_feasible_4resub(
    const std::vector<std::vector<uint64_t>>& divisor_truth_tables,
    const std::vector<uint64_t>& target_truth_table,
    int num_inputs);

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(uint64_t target_tt, 
                            const std::vector<uint64_t>& divisor_tts,
                            const std::vector<int>& selected_divisors,
                            int num_inputs,
                            const std::vector<int>& window_inputs,
                            const std::vector<int>& all_divisors,
                            std::vector<std::vector<bool>>& br,
                            std::vector<std::vector<bool>>& sim);

} // namespace fresub