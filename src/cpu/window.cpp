#include "window.hpp"
#include <queue>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>

namespace fresub {

WindowExtractor::WindowExtractor(aigman& aig, int max_cut_size)
    : aig(aig), max_cut_size(max_cut_size) {}

void WindowExtractor::extract_all_windows(std::vector<Window>& windows) {
    windows.clear();
    
    std::cout << "Enumerating cuts using exopt...\n";
    CutEnumeration(aig, cuts, max_cut_size);
    
    std::cout << "Creating windows from cuts...\n";
    create_windows_from_cuts(windows);
}

void WindowExtractor::create_windows_from_cuts(std::vector<Window>& windows) {
    // Collect ALL cuts and assign global cut IDs
    std::vector<std::pair<int, Cut>> all_cuts; // (target_node, cut)
    
    for (int target = aig.nPis + 1; target < aig.nObjs; target++) {
        for (const auto& cut : cuts[target]) {
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
                // Skip trivial cut
                continue;
            }
            if (cut.leaves.size() > static_cast<size_t>(max_cut_size)) {
                continue;
            }
            all_cuts.emplace_back(target, cut);
        }
    }
    
    // Propagate ALL cut IDs simultaneously
    propagate_all_cuts_simultaneously(all_cuts, windows);
}

void WindowExtractor::propagate_all_cuts_simultaneously(const std::vector<std::pair<int, Cut>>& all_cuts, 
                                                        std::vector<Window>& windows) {
    // Step 1: Create lists for each node to store cut IDs
    std::vector<std::vector<int>> node_cut_lists(aig.nObjs);
    
    // Step 2: Initialize cut leaves with their cut IDs  
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
        const auto& cut = all_cuts[cut_id].second;
        for (int leaf : cut.leaves) {
	    node_cut_lists[leaf].push_back(cut_id);
        }
    }
    
    // Step 3: Propagate ALL cut IDs simultaneously in topological order
    for (int node = aig.nPis + 1; node < aig.nObjs; node++) {
        
        // Get fanins from vObjs
        int fanin0 = lit2var(aig.vObjs[node * 2]);
        int fanin1 = lit2var(aig.vObjs[node * 2 + 1]);
        
        // Find intersection of cut IDs from both fanins
        std::vector<int> common_cuts;
        std::set_intersection(node_cut_lists[fanin0].begin(), node_cut_lists[fanin0].end(),
                            node_cut_lists[fanin1].begin(), node_cut_lists[fanin1].end(),
                            std::back_inserter(common_cuts));
        
        // Add common cuts to this node
        for (int cut_id : common_cuts) {
            node_cut_lists[node].push_back(cut_id);
        }
        
        // Sort for consistency
        std::sort(node_cut_lists[node].begin(), node_cut_lists[node].end());
    }
    
    // Step 4: Create windows from propagated cut IDs
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
        int target = all_cuts[cut_id].first;
        const auto& cut = all_cuts[cut_id].second;
        
        Window window;
        window.target_node = target;
        window.inputs = cut.leaves;
        window.cut_id = cut_id;
        window.nodes.clear();
        window.divisors.clear();
        
        // Collect all nodes that have this cut ID
        for (int i = 1; i < aig.nObjs; i++) {
            if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), cut_id) 
                != node_cut_lists[i].end()) {
                window.nodes.push_back(i);
            }
        }
        
        // Compute divisors = window nodes - MFFC(target) - TFO(target)
        std::unordered_set<int> mffc = compute_mffc(target);
        std::unordered_set<int> tfo = compute_tfo_in_window(target, window.nodes);
        
        for (int node : window.nodes) {
            if (mffc.find(node) == mffc.end() && tfo.find(node) == tfo.end()) {
                window.divisors.push_back(node);
            }
        }
        
        windows.push_back(window);
    }
}

std::unordered_set<int> WindowExtractor::compute_mffc(int root) const {
    std::unordered_set<int> mffc;
    std::unordered_set<int> visited;
    
    if(aig.vvFanouts.empty()) {
      aig.supportfanouts();
    }
    
    collect_mffc_recursive(root, mffc, visited);
    return mffc;
}

void WindowExtractor::collect_mffc_recursive(int node, std::unordered_set<int>& mffc, 
                                            std::unordered_set<int>& visited) const {

    visited.insert(node);
    mffc.insert(node);
    
    // Get fanins from vObjs
    int fanin0 = lit2var(aig.vObjs[node * 2]);
    int fanin1 = lit2var(aig.vObjs[node * 2 + 1]);

    bool f;
    
    if( fanin0 >  aig.nPis && visited.find(fanin0) == visited.end() ) {
      f = true;
      for (int fanout : aig.vvFanouts[fanin0]) {
	if ( mffc.find(fanout) == mffc.end() ) {
	  f = false;
	  break;
	}
      }
      if (f) {
	collect_mffc_recursive(fanin0, mffc, visited);
      }
    }
    
    if(  fanin1 >  aig.nPis && visited.find(fanin1) == visited.end()) {
      f = true;
      for (int fanout : aig.vvFanouts[fanin1]) {
	if ( mffc.find(fanout) == mffc.end() ) {
	  f = false;
	  break;
	}
      }
      if (f) {
	collect_mffc_recursive(fanin1, mffc, visited);
      }
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

std::vector<std::vector<uint64_t>> WindowExtractor::compute_truth_tables_for_window(
    int target_node,
    const std::vector<int>& window_inputs,
    const std::vector<int>& window_nodes,
    const std::vector<int>& divisors,
    bool verbose) const {

  static const unsigned long long basepats[] = {0xaaaaaaaaaaaaaaaaull,
						0xccccccccccccccccull,
						0xf0f0f0f0f0f0f0f0ull,
						0xff00ff00ff00ff00ull,
						0xffff0000ffff0000ull,
						0xffffffff00000000ull};


    if (verbose) {
        std::cout << "\n--- COMPUTING TRUTH TABLES FOR WINDOW ---\n";
        std::cout << "Target: " << target_node << "\n";
        std::cout << "Window inputs: [";
        for (size_t i = 0; i < window_inputs.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << window_inputs[i];
        }
        std::cout << "]\nWindow nodes: [";
        for (size_t i = 0; i < window_nodes.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << window_nodes[i];
        }
        std::cout << "]\nDivisors: [";
        for (size_t i = 0; i < divisors.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << divisors[i];
        }
        std::cout << "]\n";
    }
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 20) {  // Reasonable limit for memory usage
        std::cout << "ERROR: Too many inputs (" << num_inputs << ") for truth table computation\n";
        return {};
    }
    
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;  // Ceiling division
    
    if (verbose) {
        std::cout << "Truth table size: " << num_patterns << " patterns = " 
                  << num_words << " words of 64 bits\n";
    }
    
    std::unordered_map<int, std::vector<uint64_t>> node_tt;

    if (verbose) std::cout << "Initializing primary input truth tables:\n";
    for(int i = 0; i < num_inputs; i++) {
      int wi = window_inputs[i];
      node_tt[wi].resize(num_words);
      if(i < 6) {
	for(int j = 0; j < num_words; j++) {
	  node_tt[wi][j] = basepats[i];
	}
      } else {
	for(int j = 0; j < num_words; j++) {
	  node_tt[wi][j] = (j >> (i - 6)) & 1? 0xffffffffffffffffull: 0ull;
	}
      }
      if (verbose) {
	std::cout << "  Input " << wi << " (bit " << i << "): ";
	// Print first word for debugging (if reasonable size)
	if (num_patterns <= 64) {
	  for (int b = num_patterns - 1; b >= 0; b--) {
	    std::cout << ((node_tt[wi][0] >> b) & 1);
	  }
	  std::cout << " (0x" << std::hex << node_tt[wi][0] << std::dec << ")";
	} else {
	  std::cout << "[" << num_words << " words, " << num_patterns << " patterns]";
	}
	std::cout << "\n";
      }
    }
      
    // Process window nodes in topological order
    if (verbose) std::cout << "\nProcessing window nodes:\n";
    for (int current_node : window_nodes) {
        if (std::find(window_inputs.begin(), window_inputs.end(), current_node) != window_inputs.end()) {
            continue; // Skip inputs, already processed
        }
        
        // Adapt to aigman structure: use aig.vObjs instead of nodes[].fanin0/fanin1
        int fanin0 = lit2var(aig.vObjs[current_node * 2]);
        int fanin1 = lit2var(aig.vObjs[current_node * 2 + 1]);
        bool comp0 = is_complemented(aig.vObjs[current_node * 2]);
        bool comp1 = is_complemented(aig.vObjs[current_node * 2 + 1]);
        
        // Apply complementation and compute AND word by word
        node_tt[current_node].resize(num_words);
        for (int w = 0; w < num_words; w++) {
            uint64_t val0 = comp0 ? ~node_tt[fanin0][w] : node_tt[fanin0][w];
            uint64_t val1 = comp1 ? ~node_tt[fanin1][w] : node_tt[fanin1][w];
            node_tt[current_node][w] = val0 & val1;
        }
        
        if (verbose) {
            std::cout << "  Node " << current_node << " = AND(";
            std::cout << fanin0 << (comp0 ? "'" : "") << ", ";
            std::cout << fanin1 << (comp1 ? "'" : "") << "):\n";
            if (num_patterns <= 64) {
                std::cout << "    ";
                for (int b = num_patterns - 1; b >= 0; b--) {
		  std::cout << ((node_tt[current_node][0] >> b) & 1);
                }
                std::cout << " (0x" << std::hex << node_tt[current_node][0] << std::dec << ")";
            } else {
                std::cout << "    [" << num_words << " words computed]";
            }
            std::cout << "\n";
        }
    }
    
    // Extract results as vector<vector<word>>
    // results[0..n-1] = divisors[0..n-1], results[n] = target
    std::vector<std::vector<uint64_t>> results;
    
    // Add all divisors first (indices 0..n-1)
    for (int divisor : divisors) {
      assert(node_tt.find(divisor) != node_tt.end());
      results.push_back(node_tt[divisor]);
    }
    
    // Add target node at the end (index n)
    assert(node_tt.find(target_node) != node_tt.end());
    results.push_back(node_tt[target_node]);
    
    if (verbose) {
        std::cout << "\nExtracted truth tables as vector<vector<word>>:\n";
        for (size_t i = 0; i < divisors.size(); i++) {
            std::cout << "  results[" << i << "] = divisor " << divisors[i] 
                      << " (" << results[i].size() << " words)\n";
        }
        std::cout << "  results[" << divisors.size() << "] = target " << target_node 
                  << " (" << results[divisors.size()].size() << " words)\n";
        std::cout << "  Total: " << results.size() << " truth tables\n";
    }
    
    return results;
}

} // namespace fresub
