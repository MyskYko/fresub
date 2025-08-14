#include "window.hpp"
#include <queue>
#include <algorithm>
#include <cassert>
#include <bitset>
#include <iostream>

namespace fresub {

// Cut enumeration implementation (based on exopt)
CutEnumerator::CutEnumerator(AIG& aig, int max_cut_size)
    : aig(aig), max_cut_size(max_cut_size) {}

void CutEnumerator::enumerate_cuts() {
    cuts.resize(aig.num_nodes);
    
    // Initialize PI cuts (trivial cuts)
    for (int i = 1; i <= aig.num_pis; i++) {
        cuts[i].emplace_back(i);
    }
    
    // Enumerate cuts for internal nodes
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
        
        // Merge cuts from fanins
        for (const auto& cut0 : cuts[fanin0]) {
            for (const auto& cut1 : cuts[fanin1]) {
                Cut new_cut;
                uint64_t signature = cut0.signature | cut1.signature;
                
                // Quick pruning using signature
                if (cut0.leaves.size() + cut1.leaves.size() > max_cut_size && 
                    std::bitset<64>(signature).count() > max_cut_size) {
                    continue;
                }
                
                // Merge leaves
                new_cut.leaves.resize(cut0.leaves.size() + cut1.leaves.size());
                auto end_it = std::set_union(cut0.leaves.begin(), cut0.leaves.end(),
                                           cut1.leaves.begin(), cut1.leaves.end(),
                                           new_cut.leaves.begin());
                new_cut.leaves.resize(end_it - new_cut.leaves.begin());
                new_cut.signature = signature;
                
                if (new_cut.leaves.size() > max_cut_size) {
                    continue;
                }
                
                // Check if dominated by existing cuts
                bool dominated = false;
                for (const auto& cut : cuts[i]) {
                    if (dominate(cut, new_cut)) {
                        dominated = true;
                        break;
                    }
                }
                if (dominated) continue;
                
                // Remove dominated cuts and add new cut
                cuts[i].erase(std::remove_if(cuts[i].begin(), cuts[i].end(),
                    [&](const Cut& cut) { return dominate(new_cut, cut); }), cuts[i].end());
                cuts[i].push_back(new_cut);
            }
        }
        
        // Add trivial cut
        cuts[i].emplace_back(i);
    }
}

const std::vector<Cut>& CutEnumerator::get_cuts(int node) const {
    static std::vector<Cut> empty;
    if (node >= 0 && node < static_cast<int>(cuts.size())) {
        return cuts[node];
    }
    return empty;
}

bool CutEnumerator::dominate(const Cut& a, const Cut& b) const {
    if (a.leaves.size() > b.leaves.size()) {
        return false;
    }
    if ((a.signature & b.signature) != a.signature) {
        return false;
    }
    if (a.leaves.size() == b.leaves.size()) {
        return std::equal(a.leaves.begin(), a.leaves.end(), b.leaves.begin());
    }
    return std::includes(b.leaves.begin(), b.leaves.end(), a.leaves.begin(), a.leaves.end());
}

// Window extraction implementation
WindowExtractor::WindowExtractor(AIG& aig, int max_cut_size)
    : aig(aig), max_cut_size(max_cut_size), cut_enumerator(aig, max_cut_size) {}

void WindowExtractor::extract_all_windows(std::vector<Window>& windows) {
    windows.clear();
    
    std::cout << "Enumerating cuts...\n";
    cut_enumerator.enumerate_cuts();
    
    std::cout << "Creating windows from cuts...\n";
    create_windows_from_cuts(windows);
}

void WindowExtractor::create_windows_from_cuts(std::vector<Window>& windows) {
    // Collect ALL cuts and assign global cut IDs
    std::vector<std::pair<int, Cut>> all_cuts; // (target_node, cut)
    
    for (int target = aig.num_pis + 1; target < aig.num_nodes; target++) {
        if (target >= static_cast<int>(aig.nodes.size()) || aig.nodes[target].is_dead) continue;
        
        const auto& node_cuts = cut_enumerator.get_cuts(target);
        
        for (const auto& cut : node_cuts) {
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
                // Skip trivial cut
                continue;
            }
            if (cut.leaves.size() > max_cut_size) {
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
    std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
    
    // Step 2: Initialize cut leaves with their cut IDs  
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
        const auto& cut = all_cuts[cut_id].second;
        for (int leaf : cut.leaves) {
            node_cut_lists[leaf].push_back(cut_id);
        }
    }
    
    // Step 3: Propagate ALL cut IDs simultaneously in topological order
    for (int node = aig.num_pis + 1; node < aig.num_nodes; node++) {
        if (node >= static_cast<int>(aig.nodes.size()) || aig.nodes[node].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
        
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
        for (int i = 1; i < aig.num_nodes; i++) {
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
        
        if (!window.nodes.empty()) {
            windows.push_back(window);
        }
    }
}

void WindowExtractor::propagate_cut_to_window(int cut_id, const Cut& cut, int target_node, Window& window) {
    window.target_node = target_node;
    window.inputs = cut.leaves;
    window.cut_id = cut_id;
    window.nodes.clear();
    window.divisors.clear();
    
    // Step 1: Create lists for each node
    std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
    
    // Step 2: Push cut ID to cut leaves' lists
    for (int leaf : cut.leaves) {
        node_cut_lists[leaf].push_back(cut_id);
    }
    
    // Step 3: Propagate cut ID in topological order
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
        
        // Check if both fanins have the cut ID
        bool has_fanin0 = std::find(node_cut_lists[fanin0].begin(), node_cut_lists[fanin0].end(), cut_id) 
                         != node_cut_lists[fanin0].end();
        bool has_fanin1 = std::find(node_cut_lists[fanin1].begin(), node_cut_lists[fanin1].end(), cut_id)
                         != node_cut_lists[fanin1].end();
        
        if (has_fanin0 && has_fanin1) {
            node_cut_lists[i].push_back(cut_id);
        }
    }
    
    // Step 4: Collect window nodes
    for (int i = 1; i < aig.num_nodes; i++) {
        if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), cut_id) != node_cut_lists[i].end()) {
            window.nodes.push_back(i);
        }
    }
    
    // Step 5: Compute divisors = window nodes - MFFC(target)
    std::unordered_set<int> mffc = compute_mffc(target_node);
    
    for (int node : window.nodes) {
        if (mffc.find(node) == mffc.end()) {
            window.divisors.push_back(node);
        }
    }
    
    // Do NOT add complemented versions - keep only the original nodes
}

// MFFC computation - Corrected implementation
std::unordered_set<int> WindowExtractor::compute_mffc(int root) const {
    std::unordered_set<int> mffc;
    std::unordered_set<int> visited;
    std::queue<int> to_process;
    
    // MFFC always includes the root
    mffc.insert(root);
    to_process.push(root);
    visited.insert(root);
    
    while (!to_process.empty()) {
        int current = to_process.front();
        to_process.pop();
        
        if (current <= aig.num_pis || current >= static_cast<int>(aig.nodes.size()) || 
            aig.nodes[current].is_dead) {
            continue;
        }
        
        // Get fanins
        int fanin0 = aig.lit2var(aig.nodes[current].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[current].fanin1);
        
        // Check each fanin
        for (int fanin : {fanin0, fanin1}) {
            if (fanin <= aig.num_pis) {
                continue; // Skip PIs
            }
            
            if (visited.count(fanin)) {
                continue; // Already processed
            }
            
            // Check if fanin has only fanouts that are in MFFC
            bool can_add_to_mffc = true;
            
            if (fanin < static_cast<int>(aig.nodes.size()) && !aig.nodes[fanin].is_dead) {
                for (int fanout : aig.nodes[fanin].fanouts) {
                    if (mffc.find(fanout) == mffc.end()) {
                        // This fanout is not in MFFC, so fanin cannot be added
                        can_add_to_mffc = false;
                        break;
                    }
                }
            }
            
            if (can_add_to_mffc) {
                mffc.insert(fanin);
                to_process.push(fanin);
            }
            
            visited.insert(fanin);
        }
    }
    
    return mffc;
}

// TFO computation within window bounds
std::unordered_set<int> WindowExtractor::compute_tfo_in_window(int root, const std::vector<int>& window_nodes) const {
    std::unordered_set<int> tfo;
    std::unordered_set<int> window_set(window_nodes.begin(), window_nodes.end());
    std::queue<int> to_visit;
    
    to_visit.push(root);
    tfo.insert(root);
    
    while (!to_visit.empty()) {
        int current = to_visit.front();
        to_visit.pop();
        
        if (current < static_cast<int>(aig.nodes.size()) && !aig.nodes[current].is_dead) {
            for (int fanout : aig.nodes[current].fanouts) {
                // Only consider fanouts within the window
                if (window_set.find(fanout) != window_set.end() && tfo.find(fanout) == tfo.end()) {
                    tfo.insert(fanout);
                    to_visit.push(fanout);
                }
            }
        }
    }
    
    return tfo;
}

void WindowExtractor::collect_mffc_recursive(int node, int root, 
                                           std::unordered_set<int>& mffc, 
                                           std::unordered_set<int>& visited) const {
    // This function is now unused - keeping for compatibility
}

bool WindowExtractor::has_external_fanout(int node, int root) const {
    // This function is now unused - keeping for compatibility
    return false;
}

} // namespace fresub