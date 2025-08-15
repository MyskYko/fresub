#pragma once

#include "window.hpp"
#include <vector>
#include <cstdint>

namespace fresub {


// Multi-word implementation of gresub feasibility check - returns true if feasible
bool solve_resub_overlap_multiword(int i, int j, int k, int l,
                                  const std::vector<std::vector<uint64_t>>& truth_tables,
                                  const std::vector<uint64_t>& target_tt, int num_inputs);

// Find all feasible 4-input resubstitution combinations
std::vector<std::vector<int>> find_feasible_4resub(
    const std::vector<std::vector<uint64_t>>& divisor_truth_tables,
    const std::vector<uint64_t>& target_truth_table,
    int num_inputs);

} // namespace fresub