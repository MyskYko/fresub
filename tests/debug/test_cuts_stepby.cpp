#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
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

void print_cut_detailed(const Cut& cut, const AIG& aig) {
    std::cout << "Cut ";
    print_cut(cut, aig);
    std::cout << " (size=" << cut.leaves.size() << ", sig=0x" << std::hex << cut.signature << std::dec << ")";
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
    
    // Show AIG structure first
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
    
    std::cout << "\n=== STEP-BY-STEP CUT ENUMERATION ===\n\n";
    
    // Custom step-by-step cut enumeration
    std::vector<std::vector<Cut>> cuts;
    cuts.resize(aig.num_nodes);
    
    // Step 1: Initialize PI cuts
    std::cout << "STEP 1: Initialize Primary Input cuts\n";
    for (int i = 1; i <= aig.num_pis; i++) {
        cuts[i].emplace_back(i);
        std::cout << "  Node " << i << " (PI): ";
        print_cut_detailed(cuts[i][0], aig);
        std::cout << "\n";
    }
    
    std::cout << "\nSTEP 2: Process internal nodes in topological order\n\n";
    
    // Step 2: Process internal nodes
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
        
        int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
        
        std::cout << "Processing Node " << i << " = AND(" << fanin0;
        if (aig.is_complemented(aig.nodes[i].fanin0)) std::cout << "'";
        std::cout << ", " << fanin1;
        if (aig.is_complemented(aig.nodes[i].fanin1)) std::cout << "'";
        std::cout << ")\n";
        
        // Show fanin cuts
        std::cout << "  Fanin0 (" << fanin0 << ") has " << cuts[fanin0].size() << " cuts: ";
        for (size_t j = 0; j < cuts[fanin0].size(); j++) {
            if (j > 0) std::cout << ", ";
            print_cut(cuts[fanin0][j], aig);
        }
        std::cout << "\n";
        
        std::cout << "  Fanin1 (" << fanin1 << ") has " << cuts[fanin1].size() << " cuts: ";
        for (size_t j = 0; j < cuts[fanin1].size(); j++) {
            if (j > 0) std::cout << ", ";
            print_cut(cuts[fanin1][j], aig);
        }
        std::cout << "\n";
        
        // Generate cuts by merging fanin cuts
        std::cout << "  Merging cuts:\n";
        int cut_num = 0;
        
        for (const auto& cut0 : cuts[fanin0]) {
            for (const auto& cut1 : cuts[fanin1]) {
                Cut new_cut;
                uint64_t signature = cut0.signature | cut1.signature;
                
                // Show the merge attempt
                std::cout << "    Attempt " << (cut_num + 1) << ": ";
                print_cut(cut0, aig);
                std::cout << " ∪ ";
                print_cut(cut1, aig);
                std::cout << " → ";
                
                // Quick pruning using signature
                if (cut0.leaves.size() + cut1.leaves.size() > max_cut_size && 
                    __builtin_popcountll(signature) > max_cut_size) {
                    std::cout << "PRUNED (signature indicates size > " << max_cut_size << ")\n";
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
                    std::cout << "PRUNED (actual size " << new_cut.leaves.size() << " > " << max_cut_size << ")\n";
                    continue;
                }
                
                // Check if dominated by existing cuts
                bool dominated = false;
                for (const auto& existing_cut : cuts[i]) {
                    if (existing_cut.leaves.size() <= new_cut.leaves.size() &&
                        (existing_cut.signature & new_cut.signature) == existing_cut.signature &&
                        std::includes(new_cut.leaves.begin(), new_cut.leaves.end(),
                                    existing_cut.leaves.begin(), existing_cut.leaves.end())) {
                        dominated = true;
                        break;
                    }
                }
                
                if (dominated) {
                    print_cut_detailed(new_cut, aig);
                    std::cout << " DOMINATED\n";
                    continue;
                }
                
                // Remove dominated cuts and add new cut
                auto it = std::remove_if(cuts[i].begin(), cuts[i].end(),
                    [&](const Cut& existing_cut) {
                        return new_cut.leaves.size() <= existing_cut.leaves.size() &&
                               (new_cut.signature & existing_cut.signature) == new_cut.signature &&
                               std::includes(existing_cut.leaves.begin(), existing_cut.leaves.end(),
                                           new_cut.leaves.begin(), new_cut.leaves.end());
                    });
                
                if (it != cuts[i].end()) {
                    std::cout << "DOMINATES " << (cuts[i].end() - it) << " existing cuts, ";
                    cuts[i].erase(it, cuts[i].end());
                }
                
                cuts[i].push_back(new_cut);
                print_cut_detailed(new_cut, aig);
                std::cout << " ADDED\n";
                
                cut_num++;
            }
        }
        
        // Add trivial cut
        cuts[i].emplace_back(i);
        std::cout << "  Adding trivial cut: ";
        print_cut_detailed(cuts[i].back(), aig);
        std::cout << " (always added)\n";
        
        // Summary for this node
        std::cout << "  → Node " << i << " final cuts (" << cuts[i].size() << " total):\n";
        for (size_t j = 0; j < cuts[i].size(); j++) {
            std::cout << "      " << (j + 1) << ". ";
            print_cut_detailed(cuts[i][j], aig);
            std::cout << "\n";
        }
        
        std::cout << "\n";
    }
    
    // Final summary
    std::cout << "=== FINAL SUMMARY ===\n";
    int total_cuts = 0;
    for (int i = 1; i < aig.num_nodes; i++) {
        if (!cuts[i].empty()) {
            total_cuts += cuts[i].size();
            std::cout << "Node " << i << ": " << cuts[i].size() << " cuts\n";
        }
    }
    std::cout << "Total cuts: " << total_cuts << "\n";
    
    return 0;
}