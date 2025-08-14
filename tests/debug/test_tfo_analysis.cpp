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

void print_node_set(const std::unordered_set<int>& nodes, const std::string& label) {
    std::cout << label << ": {";
    std::vector<int> sorted_nodes(nodes.begin(), nodes.end());
    std::sort(sorted_nodes.begin(), sorted_nodes.end());
    for (size_t i = 0; i < sorted_nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << sorted_nodes[i];
    }
    std::cout << "}";
}

void print_node_vector(const std::vector<int>& nodes, const std::string& label) {
    std::cout << label << ": [";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

// Compute TFO (Transitive Fanout) of a node within window bounds
std::unordered_set<int> compute_tfo_in_window(const AIG& aig, int root, const std::vector<int>& window_nodes) {
    std::unordered_set<int> tfo;
    std::unordered_set<int> window_set(window_nodes.begin(), window_nodes.end());
    std::queue<int> to_visit;
    
    std::cout << "\n=== COMPUTING TFO FOR NODE " << root << " ===\n";
    
    to_visit.push(root);
    tfo.insert(root);
    
    std::cout << "Starting with root: " << root << "\n";
    
    while (!to_visit.empty()) {
        int current = to_visit.front();
        to_visit.pop();
        
        std::cout << "Processing node " << current << ", fanouts: ";
        
        if (current < static_cast<int>(aig.nodes.size()) && !aig.nodes[current].is_dead) {
            std::cout << "[";
            for (size_t i = 0; i < aig.nodes[current].fanouts.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << aig.nodes[current].fanouts[i];
            }
            std::cout << "]";
            
            for (int fanout : aig.nodes[current].fanouts) {
                // Only consider fanouts within the window
                if (window_set.find(fanout) != window_set.end() && tfo.find(fanout) == tfo.end()) {
                    tfo.insert(fanout);
                    to_visit.push(fanout);
                    std::cout << " → adding " << fanout << " to TFO";
                }
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "Final TFO: ";
    print_node_set(tfo, "");
    std::cout << " (size: " << tfo.size() << ")\n";
    
    return tfo;
}

// Correct divisor computation: Window nodes - MFFC(target) - TFO(target)
std::vector<int> compute_correct_divisors(const AIG& aig, int target, const std::vector<int>& window_nodes) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "COMPUTING CORRECT DIVISORS FOR TARGET " << target << "\n";
    std::cout << std::string(50, '=') << "\n";
    
    print_node_vector(window_nodes, "Window nodes");
    std::cout << "\n";
    
    // Step 1: Compute MFFC
    std::unordered_set<int> mffc;
    std::unordered_set<int> visited;
    std::queue<int> to_process;
    
    mffc.insert(target);
    to_process.push(target);
    visited.insert(target);
    
    while (!to_process.empty()) {
        int current = to_process.front();
        to_process.pop();
        
        if (current <= aig.num_pis || current >= static_cast<int>(aig.nodes.size()) || 
            aig.nodes[current].is_dead) {
            continue;
        }
        
        int fanin0 = aig.lit2var(aig.nodes[current].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[current].fanin1);
        
        for (int fanin : {fanin0, fanin1}) {
            if (fanin <= aig.num_pis || visited.count(fanin)) {
                continue;
            }
            
            bool can_add_to_mffc = true;
            if (fanin < static_cast<int>(aig.nodes.size()) && !aig.nodes[fanin].is_dead) {
                for (int fanout : aig.nodes[fanin].fanouts) {
                    if (mffc.find(fanout) == mffc.end()) {
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
    
    print_node_set(mffc, "MFFC");
    std::cout << "\n";
    
    // Step 2: Compute TFO within window
    std::unordered_set<int> tfo = compute_tfo_in_window(aig, target, window_nodes);
    
    // Step 3: Divisors = Window - MFFC - TFO
    std::vector<int> divisors;
    std::unordered_set<int> excluded;
    
    std::cout << "\nStep-by-step divisor computation:\n";
    for (int node : window_nodes) {
        std::cout << "  Node " << node << ": ";
        
        if (mffc.find(node) != mffc.end()) {
            std::cout << "EXCLUDED (in MFFC)\n";
            excluded.insert(node);
        } else if (tfo.find(node) != tfo.end()) {
            std::cout << "EXCLUDED (in TFO)\n";
            excluded.insert(node);
        } else {
            std::cout << "INCLUDED (valid divisor)\n";
            divisors.push_back(node);
        }
    }
    
    std::cout << "\nFinal computation:\n";
    print_node_vector(window_nodes, "Window nodes");
    std::cout << "\n";
    print_node_set(mffc, "- MFFC");
    std::cout << "\n";
    print_node_set(tfo, "- TFO");
    std::cout << "\n";
    print_node_set(excluded, "= Excluded");
    std::cout << "\n";
    print_node_vector(divisors, "= Valid divisors");
    std::cout << "\n";
    
    return divisors;
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
              << aig.num_nodes << " nodes\n\n";
    
    // Show AIG structure
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
        
        std::cout << ", fanouts: [";
        for (size_t j = 0; j < aig.nodes[i].fanouts.size(); j++) {
            if (j > 0) std::cout << ", ";
            std::cout << aig.nodes[i].fanouts[j];
        }
        std::cout << "]\n";
    }
    
    // Extract windows and analyze the problematic ones
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "\n=== ANALYZING PROBLEMATIC WINDOWS ===\n";
    
    // Focus on Window 3 (the one with the invalid divisor)
    if (windows.size() > 3) {
        const Window& window = windows[3];
        
        std::cout << "\nWINDOW 3 ANALYSIS:\n";
        std::cout << "Target: " << window.target_node << "\n";
        print_node_vector(window.nodes, "Window nodes");
        std::cout << "\n";
        print_node_vector(window.divisors, "Current (incorrect) divisors");
        std::cout << "\n";
        
        // Compute correct divisors
        std::vector<int> correct_divisors = compute_correct_divisors(aig, window.target_node, window.nodes);
        
        std::cout << "\nCOMPARISON:\n";
        std::cout << "  Current divisors: " << window.divisors.size() << " nodes\n";
        std::cout << "  Correct divisors: " << correct_divisors.size() << " nodes\n";
        std::cout << "  Difference: " << (window.divisors.size() - correct_divisors.size()) << " invalid divisors removed\n";
        
        // Show which divisors were invalid
        std::cout << "\nINVALID DIVISORS IDENTIFIED:\n";
        for (int div : window.divisors) {
            if (std::find(correct_divisors.begin(), correct_divisors.end(), div) == correct_divisors.end()) {
                std::cout << "  Node " << div << " (invalid - in TFO or MFFC)\n";
            }
        }
    }
    
    return 0;
}