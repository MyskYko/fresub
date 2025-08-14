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

void print_cut_lists(const std::vector<std::vector<int>>& node_cut_lists, const AIG& aig, const std::string& phase) {
    std::cout << "\n--- Cut Lists " << phase << " ---\n";
    for (int i = 1; i < aig.num_nodes; i++) {
        if (!node_cut_lists[i].empty()) {
            std::cout << "  Node " << i;
            if (i <= aig.num_pis) std::cout << " (PI)";
            else std::cout << " (internal)";
            std::cout << ": ";
            print_node_list(node_cut_lists[i]);
            std::cout << "\n";
        }
    }
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
    
    // Show cuts for each node briefly
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        const auto& cuts = cut_enumerator.get_cuts(i);
        std::cout << "Node " << i << " has " << cuts.size() << " cuts: ";
        for (size_t j = 0; j < cuts.size(); j++) {
            if (j > 0) std::cout << ", ";
            print_cut(cuts[j], aig);
        }
        std::cout << "\n";
    }
    
    std::cout << "\n=== STEP-BY-STEP SIMULTANEOUS CUT PROPAGATION ===\n\n";
    
    // STEP 1: Collect ALL cuts and assign global cut IDs
    std::vector<std::pair<int, Cut>> all_cuts; // (target_node, cut)
    
    std::cout << "STEP 1: Collect all non-trivial cuts and assign global IDs\n";
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
    
    // Show all cuts with their global IDs
    for (size_t i = 0; i < all_cuts.size(); i++) {
        std::cout << "Cut " << i << ": target=" << all_cuts[i].first << ", cut=";
        print_cut(all_cuts[i].second, aig);
        std::cout << "\n";
    }
    
    // STEP 2: Create lists for each node to store cut IDs
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "STEP 2: Initialize node cut lists\n";
    std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
    
    // STEP 3: Initialize cut leaves with their cut IDs  
    std::cout << "\nSTEP 3: Add cut IDs to cut leaves\n";
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
        const auto& cut = all_cuts[cut_id].second;
        std::cout << "  Cut " << cut_id << " leaves: ";
        print_cut(cut, aig);
        std::cout << " → adding to nodes: ";
        for (int leaf : cut.leaves) {
            node_cut_lists[leaf].push_back(cut_id);
            std::cout << leaf << " ";
        }
        std::cout << "\n";
    }
    
    // Show initial state
    print_cut_lists(node_cut_lists, aig, "after initialization");
    
    // STEP 4: Propagate ALL cut IDs simultaneously in topological order
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "STEP 4: Propagate ALL cut IDs simultaneously in topological order\n\n";
    
    for (int node = aig.num_pis + 1; node < aig.num_nodes; node++) {
        if (node >= static_cast<int>(aig.nodes.size()) || aig.nodes[node].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
        
        std::cout << "Processing Node " << node << " = AND(" << fanin0 << ", " << fanin1 << ")\n";
        
        std::cout << "  Fanin0 (" << fanin0 << ") cut IDs: ";
        print_node_list(node_cut_lists[fanin0]);
        std::cout << "\n";
        
        std::cout << "  Fanin1 (" << fanin1 << ") cut IDs: ";
        print_node_list(node_cut_lists[fanin1]);
        std::cout << "\n";
        
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
            
            std::cout << "  Common cut IDs: ";
            print_node_list(common_cuts);
            std::cout << " → PROPAGATED to node " << node << "\n";
            
            // Show which cuts these IDs represent
            std::cout << "    Meaning: cuts ";
            for (size_t i = 0; i < common_cuts.size(); i++) {
                if (i > 0) std::cout << ", ";
                int cut_id = common_cuts[i];
                print_cut(all_cuts[cut_id].second, aig);
                std::cout << "(id=" << cut_id << ")";
            }
            std::cout << " now dominate node " << node << "\n";
        } else {
            std::cout << "  No common cut IDs → node " << node << " gets no cuts\n";
        }
        
        // Sort for consistency
        std::sort(node_cut_lists[node].begin(), node_cut_lists[node].end());
        std::cout << "\n";
    }
    
    // Show final cut propagation state
    print_cut_lists(node_cut_lists, aig, "after propagation");
    
    // STEP 5: Create windows from propagated cut IDs
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "STEP 5: Create windows from propagated cut IDs\n\n";
    
    int windows_shown = 0;
    for (size_t cut_id = 0; cut_id < all_cuts.size(); cut_id++) {
        if (windows_shown >= 5) { // Limit output
            std::cout << "... (showing first 5 windows only)\n";
            break;
        }
        
        int target = all_cuts[cut_id].first;
        const auto& cut = all_cuts[cut_id].second;
        
        std::cout << "=== WINDOW FROM CUT " << cut_id << " ===\n";
        std::cout << "Target: " << target << "\n";
        std::cout << "Cut: ";
        print_cut(cut, aig);
        std::cout << "\n\n";
        
        // Collect all nodes that have this cut ID
        std::vector<int> window_nodes;
        std::cout << "Nodes with cut ID " << cut_id << ":\n";
        for (int i = 1; i < aig.num_nodes; i++) {
            if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), cut_id) 
                != node_cut_lists[i].end()) {
                window_nodes.push_back(i);
                std::cout << "  Node " << i;
                if (i <= aig.num_pis) std::cout << " (PI - cut leaf)";
                else if (i == target) std::cout << " (TARGET)";
                else std::cout << " (internal - dominated by cut)";
                std::cout << "\n";
            }
        }
        
        std::cout << "\nWindow nodes: ";
        print_node_list(window_nodes);
        std::cout << " (total: " << window_nodes.size() << " nodes)\n";
        
        // Compute divisors (simplified - exclude target for now)
        std::vector<int> divisors;
        for (int node : window_nodes) {
            if (node != target) {
                divisors.push_back(node);
            }
        }
        
        std::cout << "Divisors (window - target): ";
        print_node_list(divisors);
        std::cout << " (total: " << divisors.size() << " divisors)\n";
        
        // Analysis
        std::cout << "\nWindow Analysis:\n";
        std::cout << "  - This cut separates target " << target << " from PIs\n";
        std::cout << "  - Cut has " << cut.leaves.size() << " inputs: ";
        print_cut(cut, aig);
        std::cout << "\n";
        std::cout << "  - Window contains all logic dominated by this cut\n";
        std::cout << "  - " << divisors.size() << " divisors available for resubstitution\n";
        std::cout << "  - Optimization: express target using divisor combinations\n";
        
        windows_shown++;
        std::cout << "\n" << std::string(40, '-') << "\n\n";
    }
    
    std::cout << "=== ALGORITHM SUMMARY ===\n";
    std::cout << "1. Collected " << all_cuts.size() << " cuts with global IDs\n";
    std::cout << "2. Initialized cut leaves with their cut IDs\n";
    std::cout << "3. Propagated ALL cut IDs simultaneously in topological order\n";
    std::cout << "4. Each node received cut IDs common to both its fanins\n";
    std::cout << "5. Created windows based on cut dominance relationships\n";
    std::cout << "6. This ensures global consistency and proper optimization opportunities\n";
    
    return 0;
}