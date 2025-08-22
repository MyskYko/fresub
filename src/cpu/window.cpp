#include "window.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>

namespace fresub {

  WindowExtractor::WindowExtractor(aigman& aig, int max_cut_size, bool verbose)
    : aig(aig), max_cut_size(max_cut_size), verbose(verbose) {}

  void WindowExtractor::extract_all_windows(std::vector<Window>& windows) {
    assert(aig.fSorted);
    windows.clear();
    cuts.clear();
    
    if(verbose) std::cout << "Enumerating cuts using exopt...\n";
    CutEnumeration(aig, cuts, max_cut_size);
    
    if(verbose) std::cout << "Creating windows from cuts...\n";
    create_windows_from_cuts(windows);
  }

  void WindowExtractor::create_windows_from_cuts(std::vector<Window>& windows) {
    // Collect ALL cuts and assign global cut IDs
    std::vector<std::pair<int, Cut*>> all_cuts; // (target_node, cut)
    for (int target = aig.nPis + 1; target < aig.nObjs; target++) {
      for (auto& cut : cuts[target]) {
	if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
	  continue; // Skip trivial cut
	}
	assert(cut.leaves.size() <= static_cast<size_t>(max_cut_size));
	all_cuts.emplace_back(target, &cut);
      }
    }
    
    // Create lists for each node to store cut IDs
    std::vector<std::vector<int>> node_cut_lists(aig.nObjs);
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
      const Cut* cut = all_cuts[cut_id].second;
      for (int leaf : cut->leaves) {
	node_cut_lists[leaf].push_back(cut_id);
      }
    }

    // Propagate ALL cut IDs simultaneously in topological order
    std::vector<int> common_cuts; // Temporary storage
    for (int node = aig.nPis + 1; node < aig.nObjs; node++) {
      int fanin0 = lit2var(aig.vObjs[node * 2]);
      int fanin1 = lit2var(aig.vObjs[node * 2 + 1]);
      // Find intersection of cut IDs from both fanins
      common_cuts.clear();
      common_cuts.reserve(node_cut_lists[fanin0].size() + node_cut_lists[fanin1].size());
      std::set_intersection(node_cut_lists[fanin0].begin(), node_cut_lists[fanin0].end(),
                            node_cut_lists[fanin1].begin(), node_cut_lists[fanin1].end(),
                            std::back_inserter(common_cuts));
      // Merge the two sorted ranges
      std::vector<int> temp_result;
      temp_result.reserve(node_cut_lists[node].size() + common_cuts.size());
      std::set_union(node_cut_lists[node].begin(), node_cut_lists[node].end(),
		     common_cuts.begin(), common_cuts.end(),
		     std::back_inserter(temp_result));
      // Replace the old vector with the newly created, sorted union
      node_cut_lists[node] = temp_result;
    }

    // Create windows from propagated cut IDs
    windows.resize(all_cuts.size());
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
      windows[cut_id].target_node = all_cuts[cut_id].first;
      windows[cut_id].inputs = all_cuts[cut_id].second->leaves;
      windows[cut_id].cut_id = cut_id;
    }
    for (int i = 1; i < aig.nObjs; i++) {
      for (int cut_id : node_cut_lists[i]) {
	windows[cut_id].nodes.push_back(i);
      }
    }

    // Compute divisors = window nodes - MFFC(target) - TFO(target)
    for(auto& window: windows) {
      std::unordered_set<int> mffc = compute_mffc(window.target_node);
      std::unordered_set<int> tfo = compute_tfo_in_window(window.target_node, window.nodes);
      for (int node : window.nodes) {
	if (mffc.find(node) == mffc.end() && tfo.find(node) == tfo.end()) {
	  window.divisors.push_back(node);
	}
      }
      window.mffc_size = mffc.size();
    }
  }

  std::unordered_set<int> WindowExtractor::compute_mffc(int root) const {
    std::unordered_set<int> mffc;
    if (aig.vvFanouts.empty()) {
      aig.supportfanouts();
    }
    mffc.insert(root);
    int fanin0 = lit2var(aig.vObjs[root * 2]);
    int fanin1 = lit2var(aig.vObjs[root * 2 + 1]);
    collect_mffc_recursive(fanin0, mffc);
    collect_mffc_recursive(fanin1, mffc);
    return mffc;
  }

  void WindowExtractor::collect_mffc_recursive(int node, std::unordered_set<int>& mffc) const {
    if (node <= aig.nPis) {
      return;
    }
    bool all_fanouts_in_mffc = true;
    for (int fanout : aig.vvFanouts[node]) {
      if (mffc.find(fanout) == mffc.end()) {
        all_fanouts_in_mffc = false;
        break;
      }
    }
    if (all_fanouts_in_mffc) {
      mffc.insert(node);
      int fanin0 = lit2var(aig.vObjs[node * 2]);
      int fanin1 = lit2var(aig.vObjs[node * 2 + 1]);
      collect_mffc_recursive(fanin0, mffc);
      collect_mffc_recursive(fanin1, mffc);
    }
  }

  std::unordered_set<int> WindowExtractor::compute_tfo_in_window(int root, const std::vector<int>& window_nodes) const {
    std::unordered_set<int> tfo;
    std::unordered_set<int> window_set(window_nodes.begin(), window_nodes.end());
    if(aig.vvFanouts.empty()) {
      aig.supportfanouts();
    }
    std::queue<int> to_visit;
    to_visit.push(root);
    while (!to_visit.empty()) {
      int current = to_visit.front();
      to_visit.pop();
      if (tfo.find(current) == tfo.end()) {
	tfo.insert(current);
	for (int fanout : aig.vvFanouts[current]) {
	  if (window_set.find(fanout) != window_set.end() && tfo.find(fanout) == tfo.end()) {
	    to_visit.push(fanout);
	  }
	}
      }
    }
    return tfo;
  }

} // namespace fresub
