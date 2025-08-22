#pragma once

#include <vector>
#include <cstdint>
#include <aig.hpp>  // Direct exopt header inclusion

namespace fresub {

  // Convert truth tables to exopt binary relation format
  void convert_to_exopt_format(const std::vector<std::vector<uint64_t>>& truth_tables, const std::vector<int>& selected_divisors, int num_inputs, std::vector<std::vector<bool>>& br);
  
  // Synthesize optimal circuit from binary relation
  // Returns synthesized aigman* or nullptr if synthesis fails
  aigman* synthesize_circuit(const std::vector<std::vector<bool>>& br, int max_gates);

}
