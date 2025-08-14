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
    
    std::cout << "\n=== STEP-BY-STEP WINDOW CREATION ===\n\n";
    
    // Now create windows step by step
    int cut_id = 0;
    
    // Process each node's cuts to create windows
    for (int target = aig.num_pis + 1; target < aig.num_nodes; target++) {
        if (target >= static_cast<int>(aig.nodes.size()) || aig.nodes[target].is_dead) continue;
        
        const auto& node_cuts = cut_enumerator.get_cuts(target);
        
        std::cout << "=== TARGET NODE " << target << " ===\n";
        
        for (const auto& cut : node_cuts) {
            // Skip trivial cut
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
                std::cout << "Skipping trivial cut for target " << target << "\n\n";
                continue;
            }
            
            if (cut.leaves.size() > max_cut_size) {
                continue;
            }
            
            std::cout << "Creating window from cut " << cut_id << ": ";
            print_cut(cut, aig);
            std::cout << " (target=" << target << ")\n";
            
            // Step 1: Create lists for each node
            std::cout << "\nSTEP 1: Initialize node lists\n";
            std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
            
            // Step 2: Push cut ID to cut leaves' lists
            std::cout << "\nSTEP 2: Add cut ID " << cut_id << " to cut leaves\n";
            for (int leaf : cut.leaves) {
                node_cut_lists[leaf].push_back(cut_id);
                std::cout << "  Node " << leaf << " list: ";
                print_node_list(node_cut_lists[leaf]);
                std::cout << "\n";
            }
            
            // Step 3: Propagate cut ID in topological order
            std::cout << "\nSTEP 3: Propagate cut ID in topological order\n";
            
            for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
                if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
                
                int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
                int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
                
                // Check if both fanins have the cut ID
                bool has_fanin0 = std::find(node_cut_lists[fanin0].begin(), 
                                           node_cut_lists[fanin0].end(), cut_id) != node_cut_lists[fanin0].end();
                bool has_fanin1 = std::find(node_cut_lists[fanin1].begin(), 
                                           node_cut_lists[fanin1].end(), cut_id) != node_cut_lists[fanin1].end();
                
                std::cout << "  Node " << i << ": fanin0=" << fanin0;
                if (has_fanin0) std::cout << "✓"; else std::cout << "✗";
                std::cout << ", fanin1=" << fanin1;
                if (has_fanin1) std::cout << "✓"; else std::cout << "✗";
                
                if (has_fanin0 && has_fanin1) {
                    node_cut_lists[i].push_back(cut_id);
                    std::cout << " → ADD cut_id " << cut_id;
                }
                std::cout << "\n";
            }
            
            // Step 4: Collect window nodes
            std::cout << "\nSTEP 4: Collect nodes with cut ID " << cut_id << "\n";
            std::vector<int> window_nodes;
            
            for (int i = 1; i < aig.num_nodes; i++) {
                if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), cut_id) 
                    != node_cut_lists[i].end()) {
                    window_nodes.push_back(i);
                    std::cout << "  Node " << i;
                    if (i <= aig.num_pis) std::cout << " (PI)";
                    else if (i == target) std::cout << " (TARGET)";
                    else std::cout << " (internal)";
                    std::cout << "\n";
                }
            }
            
            std::cout << "\nWindow nodes: ";
            print_node_list(window_nodes);
            std::cout << " (total: " << window_nodes.size() << " nodes)\n";
            
            // Step 5: Compute MFFC and divisors (simplified)
            std::cout << "\nSTEP 5: Compute divisors (window nodes - MFFC)\n";
            std::vector<int> divisors;
            
            // For now, simple heuristic: all window nodes except target are divisors
            // In real implementation, would compute proper MFFC
            for (int node : window_nodes) {
                if (node != target) {
                    divisors.push_back(node);
                }
            }
            
            std::cout << "  MFFC(target=" << target << "): simplified as just target\n";
            std::cout << "  Divisors: ";
            print_node_list(divisors);
            std::cout << " (total: " << divisors.size() << " divisors)\n";
            
            // Show window summary
            std::cout << "\n=== WINDOW SUMMARY ===\n";
            std::cout << "Cut ID: " << cut_id << "\n";
            std::cout << "Target: " << target << "\n";
            std::cout << "Cut leaves (inputs): ";
            print_cut(cut, aig);
            std::cout << "\n";
            std::cout << "Window nodes: ";
            print_node_list(window_nodes);
            std::cout << "\n";
            std::cout << "Divisors: ";
            print_node_list(divisors);
            std::cout << "\n";
            
            // Analysis
            std::cout << "\nWindow Analysis:\n";
            std::cout << "  - Input function: " << cut.leaves.size() << "-input\n";
            std::cout << "  - Window contains: " << window_nodes.size() << " nodes\n";
            std::cout << "  - Available divisors: " << divisors.size() << "\n";
            std::cout << "  - Optimization goal: Express target " << target 
                     << " using combinations of " << divisors.size() << " divisors\n";
            
            cut_id++;
            std::cout << "\n" << std::string(60, '=') << "\n\n";
            
            // Limit output for readability
            if (cut_id >= 5) {
                std::cout << "... (stopping after 5 windows for readability)\n";
                goto done;
            }
        }
    }
    
    done:
    std::cout << "\n=== WINDOW CREATION INSIGHTS ===\n";
    std::cout << "1. Each cut defines a boundary that separates logic\n";
    std::cout << "2. Cut propagation finds all nodes 'dominated' by the cut\n";
    std::cout << "3. Window = all nodes reachable through the cut boundary\n";
    std::cout << "4. Divisors = nodes in window that can help optimize target\n";
    std::cout << "5. Different cuts for same target = different optimization views\n";
    
    return 0;
}