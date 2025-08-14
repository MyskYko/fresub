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
            
            // Check if fanin has only fanouts that are in MFFC
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
        std::cerr << "Usage: " << argv[0] << " <input.aig>\n";
        return 1;
    }
    
    std::string input_file = argv[1];
    
    // Load AIG
    std::cout << "Loading AIG from " << input_file << "...\n";
    AIG aig;
    aig.read_aiger(input_file);
    
    std::cout << "AIG: " << aig.num_pis << " PIs, " << aig.num_pos << " POs, " 
              << aig.num_nodes << " nodes\n\n";
    
    // Show AIG structure with fanouts
    std::cout << "=== AIG STRUCTURE WITH FANOUTS ===\n";
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
    
    // Test specific nodes that should have larger MFFCs
    std::vector<int> test_nodes = {10, 14}; // These appear to be outputs with no fanouts
    
    for (int node : test_nodes) {
        if (node < aig.num_nodes) {
            compute_mffc_correct(aig, node);
            std::cout << "\n" << std::string(50, '-') << "\n";
        }
    }
    
    return 0;
}