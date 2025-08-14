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
    std::vector<int> sorted_nodes(nodes.begin(), nodes.end());
    std::sort(sorted_nodes.begin(), sorted_nodes.end());
    for (int node : sorted_nodes) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}";
}

void print_node_vector(const std::vector<int>& nodes) {
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
    
    // Extract windows using corrected algorithm
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "WINDOW EXTRACTION WITH CORRECT MFFC\n";
    std::cout << std::string(60, '=') << "\n";
    
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "\nTotal windows extracted: " << windows.size() << "\n\n";
    
    // Show detailed analysis for windows with interesting MFFCs
    std::cout << "=== DETAILED WINDOW ANALYSIS ===\n\n";
    
    int windows_shown = 0;
    for (const auto& window : windows) {
        if (windows_shown >= 5) {
            std::cout << "... (showing first 5 windows only)\n";
            break;
        }
        
        // Skip windows with trivial MFFCs (size 1)
        // Get MFFC using same method as window extraction
        std::unordered_set<int> mffc_set;
        std::unordered_set<int> visited;
        std::queue<int> to_process;
        
        int target = window.target_node;
        mffc_set.insert(target);
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
                        if (mffc_set.find(fanout) == mffc_set.end()) {
                            can_add_to_mffc = false;
                            break;
                        }
                    }
                }
                
                if (can_add_to_mffc) {
                    mffc_set.insert(fanin);
                    to_process.push(fanin);
                }
                
                visited.insert(fanin);
            }
        }
        
        // Only show windows with interesting MFFCs (size > 1)
        if (mffc_set.size() <= 1) {
            continue;
        }
        
        std::cout << "=== WINDOW " << windows_shown << " ===\n";
        std::cout << "Target: " << window.target_node << "\n";
        std::cout << "Cut: ";
        Cut cut;
        cut.leaves = window.inputs;
        print_cut(cut, aig);
        std::cout << "\n\n";
        
        std::cout << "Window nodes: ";
        print_node_vector(window.nodes);
        std::cout << " (total: " << window.nodes.size() << " nodes)\n\n";
        
        std::cout << "MFFC computation for target " << window.target_node << ":\n";
        std::cout << "  MFFC: ";
        print_node_set(mffc_set);
        std::cout << " (size: " << mffc_set.size() << " nodes)\n";
        
        std::cout << "  Meaning: If target " << window.target_node << " is optimized, these " 
                 << mffc_set.size() << " nodes can be removed\n\n";
        
        std::cout << "Divisors (window - MFFC): ";
        print_node_vector(window.divisors);
        std::cout << " (total: " << window.divisors.size() << " divisors)\n\n";
        
        // Verification
        std::cout << "Verification:\n";
        std::cout << "  - Window size: " << window.nodes.size() << "\n";
        std::cout << "  - MFFC size: " << mffc_set.size() << "\n";
        std::cout << "  - Divisors: " << window.divisors.size() << "\n";
        std::cout << "  - Check: " << window.nodes.size() << " - " << mffc_set.size() 
                 << " = " << (window.nodes.size() - mffc_set.size()) << "\n";
        std::cout << "  - Divisors reported: " << window.divisors.size() << "\n";
        if (window.divisors.size() == window.nodes.size() - mffc_set.size()) {
            std::cout << "  ✓ MFFC computation is correct!\n";
        } else {
            std::cout << "  ✗ MFFC computation has issues\n";
        }
        
        std::cout << "\nOptimization potential:\n";
        std::cout << "  - Current function: " << window.inputs.size() << "-input\n";
        std::cout << "  - Available divisors: " << window.divisors.size() << "\n";
        std::cout << "  - Can save up to " << (mffc_set.size() - 1) << " gates if successful\n";
        
        windows_shown++;
        std::cout << "\n" << std::string(50, '-') << "\n\n";
    }
    
    if (windows_shown == 0) {
        std::cout << "No windows with interesting MFFCs (size > 1) found.\n";
        std::cout << "This suggests the circuit is already well-optimized.\n";
    }
    
    std::cout << "\n=== MFFC SUMMARY ===\n";
    std::map<int, int> mffc_size_distribution;
    
    for (const auto& window : windows) {
        // Compute MFFC size for each window
        std::unordered_set<int> mffc_set;
        std::unordered_set<int> visited;
        std::queue<int> to_process;
        
        int target = window.target_node;
        mffc_set.insert(target);
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
                        if (mffc_set.find(fanout) == mffc_set.end()) {
                            can_add_to_mffc = false;
                            break;
                        }
                    }
                }
                
                if (can_add_to_mffc) {
                    mffc_set.insert(fanin);
                    to_process.push(fanin);
                }
                
                visited.insert(fanin);
            }
        }
        
        mffc_size_distribution[mffc_set.size()]++;
    }
    
    std::cout << "MFFC size distribution across all windows:\n";
    for (const auto& [size, count] : mffc_size_distribution) {
        std::cout << "  Size " << size << ": " << count << " windows\n";
    }
    
    return 0;
}