#pragma once

#include <cstdint>

#include "window.hpp"

namespace fresub {

  // Truth table computation for window simulation
  // Returns vector<vector<word>> where:
  // - results[0..n-1] = divisors[0..n-1] truth tables
  // - results[n] = target truth table (at the end)
  std::vector<std::vector<uint64_t>> compute_truth_tables_for_window(aigman const& aig, Window const& window, bool verbose);

} // namespace fresub
