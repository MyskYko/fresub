#include "insertion.hpp"

#include <iostream>
#include <cassert>
#include <queue>
#include <tuple>
#include <functional>

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

  // Internal heap item for gain-based processing
  struct HeapItem {
    int gain;
    int window_idx;
    int fs_idx;
    int synth_idx;
  };

  struct HeapCmp {
    bool operator()(HeapItem const& a, HeapItem const& b) const {
      return a.gain < b.gain; // max-heap by gain
    }
  };

  int Inserter::process_windows_heap(std::vector<Window>& windows, bool verbose) const {
    if (verbose) {
      std::cout << "Building gain heap from windows and feasible sets...\n";
    }

    std::priority_queue<HeapItem, std::vector<HeapItem>, HeapCmp> heap;

    // Build heap of all synthesized candidates; require positive gain
    for (size_t wi = 0; wi < windows.size(); ++wi) {
      auto& win = windows[wi];
      for (size_t fi = 0; fi < win.feasible_sets.size(); ++fi) {
        auto& fs = win.feasible_sets[fi];
        for (size_t si = 0; si < fs.synths.size(); ++si) {
          auto* synth = fs.synths[si];
          if (!synth) continue;
          int gain = win.mffc_size - synth->nGates;
          assert(gain > 0 && "Non-beneficial candidate should be filtered before insertion heap");
          heap.push(HeapItem{gain, static_cast<int>(wi), static_cast<int>(fi), static_cast<int>(si)});
        }
      }
    }

    int applied = 0;
    int skipped = 0;
    if (verbose) {
      std::cout << "Processing heap with " << heap.size() << " candidates...\n";
    }
    while (!heap.empty()) {
      auto item = heap.top();
      heap.pop();

      auto& win = windows[item.window_idx];
      auto& fs = win.feasible_sets[item.fs_idx];
      if (item.synth_idx >= static_cast<int>(fs.synths.size())) continue;
      aigman* synth = fs.synths[item.synth_idx];
      if (!synth) continue; // may have been consumed/cleaned in a prior step

      // Validate target and divisors still exist and are acyclic
      if (!is_node_accessible(win.target_node)) {
        skipped++;
        continue;
      }
      std::vector<int> selected_nodes;
      selected_nodes.reserve(fs.divisor_indices.size());
      for (int idx : fs.divisor_indices) {
        int node = win.divisors[idx];
        if (!is_node_accessible(node)) { selected_nodes.clear(); break; }
        selected_nodes.push_back(node);
      }
      if (selected_nodes.empty() && !fs.divisor_indices.empty()) {
        skipped++;
        continue;
      }
      if (!selected_nodes.empty()) {
        std::vector<int> target_nodes = {win.target_node};
        if (aig.reach(target_nodes, selected_nodes)) {
          skipped++;
          continue;
        }
      }

      // Import synthesized circuit to replace target
      std::vector<int> outputs = {win.target_node << 1};
      aig.import(synth, selected_nodes, outputs);
      if (verbose) {
        std::cout << "Applied candidate: target=" << win.target_node
                  << ", divs=" << selected_nodes.size()
                  << ", gates=" << synth->nGates
                  << ", gain=" << item.gain << "\n";
      }
      applied++;
    }

    if (verbose) {
      std::cout << "Heap processing complete: " << applied << " applied, " << skipped << " skipped\n";
    }
    return applied;
  }

} // namespace fresub
