#pragma once

#include <cstdint>
#include <vector>

#include "window.hpp"

namespace fresub {

  // Exposed for tests: CPU overlap-based feasibility for 4-divisor case
  bool solve_resub_overlap_multiword(int i, int j, int k, int l, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  // Exposed for tests: CPU overlap-based feasibility for 0..3 divisors
  bool solve_resub_overlap_multiword_0(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  // Exposed for tests: CPU overlap-based feasibility for 1..3 divisors
  bool solve_resub_overlap_multiword_1(int i, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  bool solve_resub_overlap_multiword_2(int i, int j, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  bool solve_resub_overlap_multiword_3(int i, int j, int k, const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);

  // Exposed for tests: enumerate all feasible 4-input combinations
  std::vector<std::vector<int>> find_feasible_4resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);

  // Exposed for tests: enumerate all feasible k-input combinations (k = 0..3)
  std::vector<std::vector<int>> find_feasible_0resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  std::vector<std::vector<int>> find_feasible_1resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  std::vector<std::vector<int>> find_feasible_2resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);
  std::vector<std::vector<int>> find_feasible_3resub(const std::vector<std::vector<uint64_t>>& truth_tables, int num_inputs);

  // (Note) Internal helpers for feasibility can remain in the .cpp; no header exposure needed.

  // CPU feasibility: ALL mode
  // For each window, test exactly K=min(4, #divisors) inputs (or fewer if #divisors < 4)
  void feasibility_check_cpu_all(std::vector<Window>::iterator it, std::vector<Window>::iterator end);

  // CPU feasibility: MIN-SIZE mode
  // For each window, try k=0,1,2,3,4 (bounded by #divisors) and stop at first non-empty set
  void feasibility_check_cpu_min(std::vector<Window>::iterator it, std::vector<Window>::iterator end);

  // Existing CPU feasibility (4-input search) — kept to avoid breaking current build
  void feasibility_check_cpu(std::vector<Window>::iterator it, std::vector<Window>::iterator end);

  // CUDA feasibility check with vector iterator interface (original - finds first solution)
  void feasibility_check_cuda(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);

  // CUDA feasibility check that finds ALL feasible combinations per window
  void feasibility_check_cuda_all(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);

} // namespace fresub
