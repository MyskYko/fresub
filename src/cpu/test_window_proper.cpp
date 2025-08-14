#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include "fresub_aig.hpp"
#include "window.hpp"

using namespace fresub;

void print_cut(const Cut& cut, const AIG& aig) {
    std::cout << "{";
    for (size_t i = 0; i < cut.leaves.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << cut.leaves[i];
        if (cut.leaves[i] <= aig.num_pis) {
            std::cout << "(PI)";
        }
    }
    std::cout << "}";
}

void print_node_list(const std::vector<int>& nodes) {
    std::cout << "[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.aig> [max_cut_size]\n";
        return 1;
    }
    
    std::string input_file = argv[1];
    int max_cut_size = (argc > 2) ? std::atoi(argv[2]) : 4;
    
    // Load AIG
    std::cout << "Loading AIG from " << input_file << "...\n";
    AIG aig;
    aig.read_aiger(input_file);
    
    std::cout << "AIG: " << aig.num_pis << " PIs, " << aig.num_pos << " POs, " 
              << aig.num_nodes << " nodes\n";
    std::cout << "Max cut size: " << max_cut_size << "\n\n";
    
    // Show AIG structure
    std::cout << "=== AIG STRUCTURE ===\n";
    std::cout << "Primary Inputs: ";
    for (int i = 1; i <= aig.num_pis; i++) {
        if (i > 1) std::cout << ", ";
        std::cout << i;
    }
    std::cout << "\n\n";
    
    std::cout << "Internal nodes:\n";
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
        
        std::cout << "  Node " << i << " = AND(";
        std::cout << fanin0;
        if (aig.is_complemented(aig.nodes[i].fanin0)) std::cout << "'";
        std::cout << ", " << fanin1;
        if (aig.is_complemented(aig.nodes[i].fanin1)) std::cout << "'";
        std::cout << ")\n";
    }
    
    // First do cut enumeration
    std::cout << "\n=== CUT ENUMERATION ===\n";
    CutEnumerator cut_enumerator(aig, max_cut_size);
    cut_enumerator.enumerate_cuts();
    
    // Collect ALL cuts and assign global cut IDs
    std::vector<std::pair<int, Cut>> all_cuts; // (target_node, cut)
    int cut_id = 0;
    
    for (int target = aig.num_pis + 1; target < aig.num_nodes; target++) {
        if (target >= static_cast<int>(aig.nodes.size()) || aig.nodes[target].is_dead) continue;
        
        const auto& cuts = cut_enumerator.get_cuts(target);
        for (const auto& cut : cuts) {
            // Skip trivial cuts
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
                continue;
            }
            if (cut.leaves.size() > max_cut_size) {
                continue;
            }
            all_cuts.emplace_back(target, cut);
        }
    }
    
    std::cout << "Total non-trivial cuts: " << all_cuts.size() << "\n\n";
    
    // Show all cuts with their IDs
    std::cout << "=== ALL CUTS WITH GLOBAL IDs ===\n";
    for (size_t i = 0; i < all_cuts.size(); i++) {
        std::cout << "Cut " << i << ": target=" << all_cuts[i].first << ", cut=";
        print_cut(all_cuts[i].second, aig);
        std::cout << "\n";
    }
    
    std::cout << "\n=== PROPER CUT PROPAGATION (ALL CUTS SIMULTANEOUSLY) ===\n\n";
    
    // Step 1: Create lists for each node to store cut IDs
    std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
    
    // Step 2: Initialize cut leaves with their cut IDs  
    std::cout << "STEP 1: Initialize cut leaves with cut IDs\n";
    for (size_t i = 0; i < all_cuts.size(); i++) {
        const auto& cut = all_cuts[i].second;
        for (int leaf : cut.leaves) {
            node_cut_lists[leaf].push_back(i);
        }
    }
    
    // Show initial state
    for (int i = 1; i <= aig.num_pis; i++) {
        if (!node_cut_lists[i].empty()) {
            std::cout << "  Node " << i << " (PI) has cut IDs: ";
            print_node_list(node_cut_lists[i]);
            std::cout << "\n";
        }
    }
    
    // Step 3: Propagate ALL cut IDs simultaneously in topological order
    std::cout << "\nSTEP 2: Propagate all cut IDs in topological order\n";
    
    for (int node = aig.num_pis + 1; node < aig.num_nodes; node++) {
        if (node >= static_cast<int>(aig.nodes.size()) || aig.nodes[node].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
        
        std::cout << "  Processing Node " << node << " (fanins: " << fanin0 << ", " << fanin1 << ")\n";
        
        // Find intersection of cut IDs from both fanins
        std::vector<int> common_cuts;
        std::set_intersection(node_cut_lists[fanin0].begin(), node_cut_lists[fanin0].end(),
                            node_cut_lists[fanin1].begin(), node_cut_lists[fanin1].end(),
                            std::back_inserter(common_cuts));
        
        if (!common_cuts.empty()) {
            // Add common cuts to this node
            for (int cut_id : common_cuts) {
                node_cut_lists[node].push_back(cut_id);
            }
            
            std::cout << "    Fanin0 cut IDs: ";
            print_node_list(node_cut_lists[fanin0]);
            std::cout << "\n    Fanin1 cut IDs: ";
            print_node_list(node_cut_lists[fanin1]);
            std::cout << "\n    Common cut IDs: ";
            print_node_list(common_cuts);
            std::cout << " → PROPAGATED to node " << node << "\n";
        } else {
            std::cout << "    No common cut IDs between fanins\n";
        }
        
        // Sort for consistency
        std::sort(node_cut_lists[node].begin(), node_cut_lists[node].end());
    }
    
    // Step 4: Create windows from propagated cut IDs
    std::cout << "\n=== STEP 3: CREATE WINDOWS ===\n\n";
    
    for (size_t cut_idx = 0; cut_idx < std::min((size_t)5, all_cuts.size()); cut_idx++) {
        int target = all_cuts[cut_idx].first;
        const auto& cut = all_cuts[cut_idx].second;
        
        std::cout << "Window from Cut " << cut_idx << ": target=" << target << ", cut=";
        print_cut(cut, aig);
        std::cout << "\n";
        
        // Collect all nodes that have this cut ID
        std::vector<int> window_nodes;
        for (int i = 1; i < aig.num_nodes; i++) {
            if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), cut_idx) 
                != node_cut_lists[i].end()) {
                window_nodes.push_back(i);
            }
        }
        
        std::cout << "  Window nodes: ";
        print_node_list(window_nodes);
        std::cout << " (total: " << window_nodes.size() << " nodes)\n";
        
        // Compute divisors (simplified - exclude target)
        std::vector<int> divisors;
        for (int node : window_nodes) {
            if (node != target) {
                divisors.push_back(node);
            }
        }
        
        std::cout << "  Divisors: ";
        print_node_list(divisors);
        std::cout << " (total: " << divisors.size() << " divisors)\n\n";
    }
    
    std::cout << "=== SUMMARY ===\n";
    std::cout << "This approach correctly propagates ALL cut IDs simultaneously,\n";
    std::cout << "creating a proper global view of cut dominance relationships.\n";
    std::cout << "Each node knows which cuts 'pass through' it, enabling\n";
    std::cout << "accurate window extraction for optimization.\n";
    
    return 0;
}