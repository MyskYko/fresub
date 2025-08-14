#include "conflict.hpp"
#include <functional>
#include <queue>
#include <iostream>

namespace fresub {

ConflictResolver::ConflictResolver(AIG& aig) : aig(aig) {}

bool ConflictResolver::is_node_accessible(int node) const {
    if (node <= 0 || node >= aig.num_nodes) {
        return false;
    }
    return dead_nodes.find(node) == dead_nodes.end() && !aig.nodes[node].is_dead;
}

bool ConflictResolver::is_window_valid(const Window& window) const {
    // Check if target node is still accessible
    if (!is_node_accessible(window.target_node)) {
        return false;
    }
    
    // Check if all window inputs are still accessible
    for (int input : window.inputs) {
        if (!is_node_accessible(input)) {
            return false;
        }
    }
    
    // Check if required divisors are still accessible
    for (int divisor : window.divisors) {
        if (!is_node_accessible(divisor)) {
            return false;
        }
    }
    
    return true;
}

std::vector<int> ConflictResolver::compute_mffc(int node) const {
    std::vector<int> mffc;
    std::unordered_set<int> visited;
    std::queue<int> to_process;
    
    // Start with the target node
    to_process.push(node);
    visited.insert(node);
    
    while (!to_process.empty()) {
        int current = to_process.front();
        to_process.pop();
        
        if (current <= aig.num_pis) {
            continue; // Don't include PIs in MFFC
        }
        
        mffc.push_back(current);
        
        // Check if this node has fanout > 1 (not in MFFC except for original target)
        if (current != node && aig.nodes[current].fanouts.size() > 1) {
            continue;
        }
        
        // Add fanins to processing queue if they're in the fanout-free cone
        int fanin0_node = AIG::lit2var(aig.nodes[current].fanin0);
        int fanin1_node = AIG::lit2var(aig.nodes[current].fanin1);
        
        if (fanin0_node > aig.num_pis && visited.find(fanin0_node) == visited.end()) {
            // Check if fanin0 has only this node as fanout
            bool single_fanout = (aig.nodes[fanin0_node].fanouts.size() == 1);
            if (single_fanout) {
                to_process.push(fanin0_node);
                visited.insert(fanin0_node);
            }
        }
        
        if (fanin1_node > aig.num_pis && visited.find(fanin1_node) == visited.end()) {
            // Check if fanin1 has only this node as fanout
            bool single_fanout = (aig.nodes[fanin1_node].fanouts.size() == 1);
            if (single_fanout) {
                to_process.push(fanin1_node);
                visited.insert(fanin1_node);
            }
        }
    }
    
    return mffc;
}

void ConflictResolver::mark_mffc_dead(int target_node) {
    std::vector<int> mffc = compute_mffc(target_node);
    
    std::cout << "  Marking MFFC dead for node " << target_node << ": {";
    for (size_t i = 0; i < mffc.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << mffc[i];
        dead_nodes.insert(mffc[i]);
        aig.nodes[mffc[i]].is_dead = true;
    }
    std::cout << "}\n";
}

std::vector<bool> ConflictResolver::process_windows_sequentially(
    const std::vector<Window>& windows,
    std::function<bool(const Window&)> resubstitution_func) {
    
    std::vector<bool> results(windows.size(), false);
    int applied = 0, skipped = 0;
    
    std::cout << "Processing " << windows.size() << " windows sequentially...\n";
    
    for (size_t i = 0; i < windows.size(); i++) {
        const Window& window = windows[i];
        
        // Check if window is still valid
        if (!is_window_valid(window)) {
            std::cout << "  Window " << i << " (target " << window.target_node 
                      << "): SKIPPED (invalid nodes)\n";
            skipped++;
            continue;
        }
        
        // Try to apply resubstitution
        bool success = resubstitution_func(window);
        results[i] = success;
        
        if (success) {
            std::cout << "  Window " << i << " (target " << window.target_node 
                      << "): APPLIED\n";
            
            // MFFC marking is now handled automatically by replace_node()
            applied++;
        } else {
            std::cout << "  Window " << i << " (target " << window.target_node 
                      << "): FAILED (resubstitution unsuccessful)\n";
            skipped++;
        }
    }
    
    std::cout << "Sequential processing complete: " << applied 
              << " applied, " << skipped << " skipped\n";
    
    return results;
}

} // namespace fresub