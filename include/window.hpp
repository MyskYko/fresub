#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <aig.hpp>
#include <cut.hpp>

namespace fresub {

  struct FeasibleSet {
    std::vector<int> divisor_indices; // indices into window.divisors
    std::vector<aigman*> synths;      // synthesized subcircuits for this set
  };

  struct Window {
    int target_node;
    std::vector<int> inputs;     // Window inputs (cut leaves)
    std::vector<int> nodes;      // All nodes in window
    std::vector<int> divisors;   // Window nodes - MFFC(target)
    int cut_id;                  // ID of the cut that generated this window
    int mffc_size;
    std::vector<std::vector<uint64_t>> truth_tables;
    std::vector<FeasibleSet> feasible_sets; // optional: enriched storage per feasible set
  };

  // Extract all windows using exopt's cut enumeration.
  void window_extract_all(aigman& aig, int max_cut_size, bool verbose, std::vector<Window>& windows);

  // TFO computation within window bounds (exposed for testing)
  std::unordered_set<int> compute_tfo_in_window(aigman& aig, int root, const std::vector<int>& window_nodes);

} // namespace fresub
