#pragma once

#include "window.hpp"
#include <aig.hpp>
#include <vector>
#include <unordered_set>
#include <functional>

namespace fresub {

  // Structure representing a resubstitution result with target and selected divisors
  struct Result {
    aigman* aig;  // pointer to synthesized subcircuit
    int target_node;
    std::vector<int> selected_divisor_nodes;  // actual node IDs, not indices
    
    Result(aigman* aig, int target_node, const std::vector<int>& selected_divisor_nodes)
      : aig(aig), target_node(target_node), selected_divisor_nodes(selected_divisor_nodes) {}
  };

  class Inserter {
  public:
    Inserter(aigman& aig);
    
    // Check if a resubstitution result is still valid
    bool is_candidate_valid(const Result& result) const;
    
    // Process results sequentially, applying valid ones
    std::vector<bool> process_candidates_sequentially(const std::vector<Result>& results, bool verbose = false) const;
    
  private:
    aigman& aig;
    
    // Check if node is still alive and accessible
    bool is_node_accessible(int node) const;
  };

} // namespace fresub
