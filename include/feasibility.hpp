#pragma once

#include "fresub_aig.hpp"
#include "window.hpp"
#include <vector>
#include <cstdint>

namespace fresub {

// Result of feasibility check
struct FeasibilityResult {
    bool found;
    std::vector<int> divisor_indices;  // Which divisors in the window
    std::vector<int> divisor_nodes;    // Actual node IDs
    uint32_t mask;
};

// Compute truth table for a node within a window context
uint64_t compute_truth_table_for_node(const AIG& aig, int node, 
                                      const std::vector<int>& window_inputs,
                                      const std::vector<int>& window_nodes);

// CPU implementation of gresub feasibility check
uint32_t solve_resub_overlap_cpu(int i, int j, int k, int l, 
                                const std::vector<uint64_t>& truth_tables,
                                uint64_t target_tt, int num_inputs);

// Find feasible 4-input resubstitution
FeasibilityResult find_feasible_4resub(const AIG& aig, const Window& window);

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