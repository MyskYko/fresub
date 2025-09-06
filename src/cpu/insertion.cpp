#include "insertion.hpp"

#include <iostream>
#include <cassert>
#include <queue>
#include "aig_utils.hpp"

namespace fresub {

  Inserter::Inserter(aigman& aig) : aig(aig) {}

  bool Inserter::is_node_accessible(int node) const {
    if (node < 0 || node >= aig.nObjs) {
      return false;
    }
    return aig.vDeads.empty() || !aig.vDeads[node];
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
          int estimated_gain = win.mffc_size - synth->nGates;
          assert(estimated_gain > 0 && "Non-beneficial candidate should be filtered before insertion heap");
          heap.push(HeapItem{estimated_gain, static_cast<int>(wi), static_cast<int>(fi), static_cast<int>(si)});
        }
      }
    }

    int applied = 0;
    int skipped = 0;
    if (verbose) {
      std::cout << "Processing heap with " << heap.size() << " candidates...\n";
    }
    // Reusable deref buffer for MFFC computation
    std::vector<int> deref;
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

      // Recompute current MFFC-based gain for the target node
      // Exclude selected divisors by priming their deref counts
      auto mffc_now = compute_mffc_excluding_divisors(aig, win.target_node, deref, selected_nodes);
      int current_gain = static_cast<int>(mffc_now.size()) - synth->nGates;
      if (current_gain <= 0) {
        // No longer beneficial after prior insertions
        skipped++;
        continue;
      }

      // Import synthesized circuit to replace target
      int gates_before = aig.nGates;
      std::vector<int> outputs = {win.target_node << 1};
      aig.import(synth, selected_nodes, outputs);
      int actual_gain = gates_before - aig.nGates;
      if (verbose) {
        std::cout << "Applied candidate: target=" << win.target_node
                  << ", divs=" << selected_nodes.size()
                  << ", gates=" << synth->nGates
                  << ", gain=" << current_gain
                  << ", actual_gain=" << actual_gain << "\n";
      }
      // Note: actual_gain may exceed current_gain due to constant propagation and downstream simplifications
      assert(actual_gain >= current_gain);
      applied++;
    }

    if (verbose) {
      std::cout << "Heap processing complete: " << applied << " applied, " << skipped << " skipped\n";
    }
    return applied;
  }

} // namespace fresub
