#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <unordered_set>
#include <queue>
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

void print_node_set(const std::unordered_set<int>& nodes) {
    std::cout << "{";
    bool first = true;
    for (int node : nodes) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}";
}

// Correct MFFC computation
std::unordered_set<int> compute_mffc_correct(const AIG& aig, int root) {
    std::unordered_set<int> mffc;
    std::unordered_set<int> visited;
    std::queue<int> to_process;
    
    std::cout << "\n=== COMPUTING MFFC FOR NODE " << root << " ===\n";
    
    // MFFC always includes the root
    mffc.insert(root);
    to_process.push(root);
    visited.insert(root);
    
    std::cout << "Step 1: Add root " << root << " to MFFC\n";
    std::cout << "Current MFFC: ";
    print_node_set(mffc);
    std::cout << "\n\n";
    
    while (!to_process.empty()) {
        int current = to_process.front();
        to_process.pop();
        
        if (current <= aig.num_pis || current >= static_cast<int>(aig.nodes.size()) || 
            aig.nodes[current].is_dead) {
            continue;
        }
        
        std::cout << "Step: Processing node " << current << "\n";
        
        // Get fanins
        int fanin0 = aig.lit2var(aig.nodes[current].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[current].fanin1);
        
        std::cout << "  Fanins: " << fanin0 << ", " << fanin1 << "\n";
        
        // Check each fanin
        for (int fanin : {fanin0, fanin1}) {
            if (fanin <= aig.num_pis) {
                std::cout << "    Fanin " << fanin << " is PI → skip\n";
                continue;
            }
            
            if (visited.count(fanin)) {
                std::cout << "    Fanin " << fanin << " already visited → skip\n";
                continue;
            }
            
            // Check if fanin has only one fanout (current node) or all fanouts are in MFFC
            std::cout << "    Checking fanin " << fanin << " fanouts: ";
            bool can_add_to_mffc = true;
            
            if (fanin < static_cast<int>(aig.nodes.size()) && !aig.nodes[fanin].is_dead) {
                for (int fanout : aig.nodes[fanin].fanouts) {
                    std::cout << fanout << " ";
                    if (mffc.find(fanout) == mffc.end()) {
                        // This fanout is not in MFFC, so fanin cannot be added
                        can_add_to_mffc = false;
                        std::cout << "(external) ";
                    } else {
                        std::cout << "(in-mffc) ";
                    }
                }
            }
            std::cout << "\n";
            
            if (can_add_to_mffc) {
                std::cout << "    → Adding fanin " << fanin << " to MFFC\n";
                mffc.insert(fanin);
                to_process.push(fanin);
            } else {
                std::cout << "    → Fanin " << fanin << " has external fanouts, cannot add\n";
            }
            
            visited.insert(fanin);
        }
        
        std::cout << "  Current MFFC: ";
        print_node_set(mffc);
        std::cout << "\n\n";
    }
    
    std::cout << "Final MFFC for node " << root << ": ";
    print_node_set(mffc);
    std::cout << " (size: " << mffc.size() << ")\n";
    
    return mffc;
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
    
    // Show AIG structure with fanouts
    std::cout << "=== AIG STRUCTURE WITH FANOUTS ===\n";
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
        std::cout << ")";
        
        // Show fanouts
        std::cout << ", fanouts: [";
        for (size_t j = 0; j < aig.nodes[i].fanouts.size(); j++) {
            if (j > 0) std::cout << ", ";
            std::cout << aig.nodes[i].fanouts[j];
        }
        std::cout << "]\n";
    }
    
    // Test MFFC computation on a few sample nodes
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "TESTING MFFC COMPUTATION\n";
    std::cout << std::string(60, '=') << "\n";
    
    // Test MFFC for several nodes
    std::vector<int> test_nodes;
    for (int i = aig.num_pis + 1; i < std::min(aig.num_nodes, aig.num_pis + 6); i++) {
        if (i < static_cast<int>(aig.nodes.size()) && !aig.nodes[i].is_dead) {
            test_nodes.push_back(i);
        }
    }
    
    for (int node : test_nodes) {
        std::unordered_set<int> mffc = compute_mffc_correct(aig, node);
        
        std::cout << "\n" << std::string(40, '-') << "\n";
    }
    
    // Now test window extraction with correct MFFC
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "WINDOW EXTRACTION WITH CORRECT MFFC\n";
    std::cout << std::string(60, '=') << "\n";
    
    // Get some cuts and create windows
    CutEnumerator cut_enumerator(aig, max_cut_size);
    cut_enumerator.enumerate_cuts();
    
    // Test first few cuts
    int cuts_tested = 0;
    for (int target = aig.num_pis + 1; target < aig.num_nodes && cuts_tested < 3; target++) {
        if (target >= static_cast<int>(aig.nodes.size()) || aig.nodes[target].is_dead) continue;
        
        const auto& cuts = cut_enumerator.get_cuts(target);
        for (const auto& cut : cuts) {
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) continue; // Skip trivial
            if (cut.leaves.size() > max_cut_size) continue;
            
            std::cout << "\n=== WINDOW ANALYSIS ===\n";
            std::cout << "Target: " << target << "\n";
            std::cout << "Cut: ";
            print_cut(cut, aig);
            std::cout << "\n\n";
            
            // For now, create simple window (all nodes in TFI of target up to cut)
            std::unordered_set<int> window_nodes_set;
            std::queue<int> to_visit;
            to_visit.push(target);
            window_nodes_set.insert(target);
            
            // Add cut leaves to window
            for (int leaf : cut.leaves) {
                window_nodes_set.insert(leaf);
            }
            
            // Simple TFI traversal within cut boundary
            while (!to_visit.empty()) {
                int node = to_visit.front();
                to_visit.pop();
                
                if (node <= aig.num_pis || node >= static_cast<int>(aig.nodes.size()) || 
                    aig.nodes[node].is_dead) continue;
                
                int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
                int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
                
                // Only add fanins if they're not cut leaves
                for (int fanin : {fanin0, fanin1}) {
                    bool is_cut_leaf = std::find(cut.leaves.begin(), cut.leaves.end(), fanin) != cut.leaves.end();
                    if (!is_cut_leaf && window_nodes_set.find(fanin) == window_nodes_set.end()) {
                        window_nodes_set.insert(fanin);
                        to_visit.push(fanin);
                    }
                }
            }
            
            std::vector<int> window_nodes(window_nodes_set.begin(), window_nodes_set.end());
            std::sort(window_nodes.begin(), window_nodes.end());
            
            std::cout << "Window nodes: ";
            for (size_t i = 0; i < window_nodes.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << window_nodes[i];
            }
            std::cout << " (total: " << window_nodes.size() << ")\n\n";
            
            // Compute MFFC
            std::unordered_set<int> mffc = compute_mffc_correct(aig, target);
            
            // Compute divisors = window nodes - MFFC
            std::vector<int> divisors;
            for (int node : window_nodes) {
                if (mffc.find(node) == mffc.end()) {
                    divisors.push_back(node);
                }
            }
            
            std::cout << "\nDivisors (window - MFFC): ";
            for (size_t i = 0; i < divisors.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << divisors[i];
            }
            std::cout << " (total: " << divisors.size() << ")\n";
            
            std::cout << "\nSummary:\n";
            std::cout << "  - Window has " << window_nodes.size() << " nodes\n";
            std::cout << "  - MFFC has " << mffc.size() << " nodes (removable if target optimized)\n";
            std::cout << "  - " << divisors.size() << " divisors available for resubstitution\n";
            
            cuts_tested++;
            if (cuts_tested >= 3) break;
            
            std::cout << "\n" << std::string(40, '-') << "\n";
        }
    }
    
    return 0;
}