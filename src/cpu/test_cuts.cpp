#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include "fresub_aig.hpp"
#include "window.hpp"

using namespace fresub;

void print_cut(const Cut& cut, const AIG& aig) {
    std::cout << "[";
    for (size_t i = 0; i < cut.leaves.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << cut.leaves[i];
        if (cut.leaves[i] <= aig.num_pis) {
            std::cout << "(PI)";
        }
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
    
    // Create cut enumerator
    CutEnumerator cut_enum(aig, max_cut_size);
    
    std::cout << "=== CUT ENUMERATION ===\n\n";
    
    // Enumerate cuts
    std::cout << "Enumerating cuts...\n";
    cut_enum.enumerate_cuts();
    
    // Analyze cut statistics
    std::cout << "\n=== CUT STATISTICS ===\n";
    
    int total_cuts = 0;
    std::map<int, int> cut_size_dist;
    std::map<int, int> cuts_per_node_dist;
    
    for (int i = 1; i < aig.num_nodes; i++) {
        const auto& cuts = cut_enum.get_cuts(i);
        if (!cuts.empty()) {
            total_cuts += cuts.size();
            cuts_per_node_dist[cuts.size()]++;
            
            for (const auto& cut : cuts) {
                cut_size_dist[cut.leaves.size()]++;
            }
        }
    }
    
    std::cout << "Total cuts: " << total_cuts << "\n";
    std::cout << "Average cuts per node: " << (double)total_cuts / aig.num_nodes << "\n\n";
    
    std::cout << "Cut size distribution:\n";
    for (const auto& [size, count] : cut_size_dist) {
        std::cout << "  Size " << size << ": " << count << " cuts\n";
    }
    
    std::cout << "\nCuts per node distribution:\n";
    for (const auto& [num_cuts, count] : cuts_per_node_dist) {
        std::cout << "  " << count << " nodes have " << num_cuts << " cuts\n";
    }
    
    // Show detailed cuts for first few nodes
    std::cout << "\n=== DETAILED CUT ANALYSIS ===\n\n";
    
    // Primary inputs - trivial cuts
    std::cout << "Primary inputs (trivial cuts):\n";
    for (int i = 1; i <= std::min(3, aig.num_pis); i++) {
        const auto& cuts = cut_enum.get_cuts(i);
        std::cout << "  Node " << i << " (PI): ";
        for (const auto& cut : cuts) {
            print_cut(cut, aig);
            std::cout << " ";
        }
        std::cout << "\n";
    }
    
    // First few internal nodes
    std::cout << "\nFirst internal nodes:\n";
    int shown = 0;
    for (int i = aig.num_pis + 1; i < aig.num_nodes && shown < 5; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        const auto& cuts = cut_enum.get_cuts(i);
        if (cuts.empty()) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
        
        std::cout << "\nNode " << i << " = AND(" << fanin0;
        if (aig.is_complemented(aig.nodes[i].fanin0)) std::cout << "'";
        std::cout << ", " << fanin1;
        if (aig.is_complemented(aig.nodes[i].fanin1)) std::cout << "'";
        std::cout << ")\n";
        
        std::cout << "  " << cuts.size() << " cuts:\n";
        
        // Group cuts by size
        std::map<int, std::vector<Cut>> cuts_by_size;
        for (const auto& cut : cuts) {
            cuts_by_size[cut.leaves.size()].push_back(cut);
        }
        
        for (const auto& [size, size_cuts] : cuts_by_size) {
            std::cout << "    Size " << size << " (" << size_cuts.size() << " cuts): ";
            for (size_t j = 0; j < std::min((size_t)3, size_cuts.size()); j++) {
                if (j > 0) std::cout << ", ";
                print_cut(size_cuts[j], aig);
            }
            if (size_cuts.size() > 3) {
                std::cout << ", ... (" << (size_cuts.size() - 3) << " more)";
            }
            std::cout << "\n";
        }
        
        shown++;
    }
    
    // Find some interesting nodes to examine
    std::cout << "\n=== INTERESTING NODES ===\n\n";
    
    // Node with most cuts
    int max_cuts_node = -1;
    int max_cuts_count = 0;
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        const auto& cuts = cut_enum.get_cuts(i);
        if (static_cast<int>(cuts.size()) > max_cuts_count) {
            max_cuts_count = cuts.size();
            max_cuts_node = i;
        }
    }
    
    if (max_cuts_node >= 0) {
        std::cout << "Node with most cuts: " << max_cuts_node 
                  << " (" << max_cuts_count << " cuts)\n";
        
        const auto& cuts = cut_enum.get_cuts(max_cuts_node);
        std::map<int, int> size_counts;
        for (const auto& cut : cuts) {
            size_counts[cut.leaves.size()]++;
        }
        std::cout << "  Cut sizes: ";
        for (const auto& [size, count] : size_counts) {
            std::cout << count << "×size" << size << " ";
        }
        std::cout << "\n";
    }
    
    // Find a node at medium depth
    aig.compute_levels();
    int target_level = 15;
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (aig.get_level(i) == target_level) {
            const auto& cuts = cut_enum.get_cuts(i);
            if (cuts.size() > 5) {
                std::cout << "\nNode " << i << " at level " << target_level 
                          << " (" << cuts.size() << " cuts):\n";
                
                // Show cut composition
                std::cout << "  Cut composition:\n";
                for (size_t j = 0; j < std::min((size_t)5, cuts.size()); j++) {
                    const auto& cut = cuts[j];
                    std::cout << "    Cut " << j << ": ";
                    print_cut(cut, aig);
                    
                    // Analyze cut
                    int pi_count = 0;
                    int min_level = 1000, max_level = 0;
                    for (int leaf : cut.leaves) {
                        if (leaf <= aig.num_pis) {
                            pi_count++;
                        } else {
                            int level = aig.get_level(leaf);
                            min_level = std::min(min_level, level);
                            max_level = std::max(max_level, level);
                        }
                    }
                    
                    std::cout << " - " << pi_count << " PIs";
                    if (pi_count < static_cast<int>(cut.leaves.size())) {
                        std::cout << ", levels " << min_level << "-" << max_level;
                    }
                    std::cout << "\n";
                }
                break;
            }
        }
    }
    
    std::cout << "\n=== CUT ENUMERATION INSIGHTS ===\n";
    std::cout << "1. Each cut represents a way to separate a node from PIs\n";
    std::cout << "2. Cut leaves are the 'inputs' to compute that node\n";
    std::cout << "3. Smaller cuts = simpler logic functions\n";
    std::cout << "4. More cuts = more optimization opportunities\n";
    std::cout << "5. Trivial cut (node itself) always exists\n";
    
    return 0;
}