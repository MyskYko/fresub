#include "insertion.hpp"
#include <functional>
#include <queue>
#include <iostream>

namespace fresub {

  Inserter::Inserter(aigman& aig) : aig(aig) {}

  bool Inserter::is_node_accessible(int node) const {
    if (node < 0 || node >= aig.nObjs) {
      return false;
    }
    return aig.vDeads.empty() || !aig.vDeads[node];
  }


  bool Inserter::is_candidate_valid(const Result& result) const {
    // (1) Check if target node still exists
    if (!is_node_accessible(result.target_node)) {
      return false;
    }
    // (2) Check if selected divisors still exist
    for (int divisor_node : result.selected_divisor_nodes) {
      if (!is_node_accessible(divisor_node)) {
	return false;
      }
    }
    // (3) Check if target node is NOT reachable to selected divisors
    if (!result.selected_divisor_nodes.empty()) {
      std::vector<int> target_nodes = {result.target_node};
      if (aig.reach(target_nodes, result.selected_divisor_nodes)) {
	return false;  // Target can reach selected divisors - result invalid
      }
    }
    return true;
  }

  std::vector<bool> Inserter::process_candidates_sequentially(const std::vector<Result>& results, bool verbose) const {
    std::vector<bool> applied_results(results.size(), false);
    int applied = 0, skipped = 0;
    if (verbose) {
      std::cout << "Processing " << results.size() << " resubstitution results sequentially...\n";
    }
    for (size_t i = 0; i < results.size(); i++) {
      const Result& result = results[i];
      // Check if result is still valid
      if (!is_candidate_valid(result)) {
	if (verbose) {
	  std::cout << "  Result " << i << " (target " << result.target_node << "): SKIPPED (invalid)\n";
	}
	skipped++;
	continue;
      }
      std::vector<int> outputs = {results[i].target_node << 1};
      aig.import(results[i].aig, results[i].selected_divisor_nodes, outputs);
      if (verbose) {
	std::cout << "  Result " << i << " (target " << result.target_node 
		  << "): APPLIED (synthesized with " << result.selected_divisor_nodes.size() 
		  << " divisors)\n";
      }
      applied_results[i] = true;
      applied++;
    }
    if (verbose) {
      std::cout << "Sequential processing complete: " << applied 
		<< " applied, " << skipped << " skipped\n";
    }
    return applied_results;
  }

} // namespace fresub
