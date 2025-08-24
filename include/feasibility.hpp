#pragma once

#include <cstdint>
#include <vector>

#include "window.hpp"

namespace fresub {

  // Multi-word implementation of gresub feasibility check - returns true if feasible
  bool solve_resub_overlap_multiword(int i, int j, int k, int l, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);

  // Find all feasible 4-input resubstitution combinations
  std::vector<std::vector<int>> find_feasible_4resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);

  void feasibility_check_cpu(std::vector<Window>::iterator it, std::vector<Window>::iterator end);

  // CUDA feasibility check with vector iterator interface (original - finds first solution)
  void feasibility_check_cuda(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);

  // CUDA feasibility check that finds ALL feasible combinations per window
  void feasibility_check_cuda_all(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);

} // namespace fresub
