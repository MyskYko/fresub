#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <unordered_set>
#include <queue>
#include <bitset>
#include <iomanip>
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

// Print truth table in binary format
void print_truth_table(uint64_t tt, int num_inputs) {
    int num_patterns = 1 << num_inputs;
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")";
}

// Compute truth table for a node within a window
uint64_t compute_truth_table_for_node(const AIG& aig, int node, 
                                      const std::vector<int>& window_inputs,
                                      const std::vector<int>& window_nodes) {
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 6) return 0;
    
    int num_patterns = 1 << num_inputs;
    std::map<int, uint64_t> node_tt;
    
    // Initialize primary input truth tables
    for (int i = 0; i < num_inputs; i++) {
        int pi = window_inputs[i];
        uint64_t pattern = 0;
        for (int p = 0; p < num_patterns; p++) {
            if ((p >> i) & 1) {
                pattern |= (1ULL << p);
            }
        }
        node_tt[pi] = pattern;
    }
    
    // Simulate internal nodes in topological order
    for (int current_node : window_nodes) {
        if (current_node <= aig.num_pis) continue;
        if (current_node >= static_cast<int>(aig.nodes.size()) || aig.nodes[current_node].is_dead) continue;
        
        uint32_t fanin0_lit = aig.nodes[current_node].fanin0;
        uint32_t fanin1_lit = aig.nodes[current_node].fanin1;
        
        int fanin0 = aig.lit2var(fanin0_lit);
        int fanin1 = aig.lit2var(fanin1_lit);
        
        bool fanin0_compl = aig.is_complemented(fanin0_lit);
        bool fanin1_compl = aig.is_complemented(fanin1_lit);
        
        if (node_tt.find(fanin0) == node_tt.end() || node_tt.find(fanin1) == node_tt.end()) {
            continue;
        }
        
        uint64_t tt0 = node_tt[fanin0];
        uint64_t tt1 = node_tt[fanin1];
        
        if (fanin0_compl) tt0 = ~tt0 & ((1ULL << num_patterns) - 1);
        if (fanin1_compl) tt1 = ~tt1 & ((1ULL << num_patterns) - 1);
        
        uint64_t result_tt = tt0 & tt1;
        node_tt[current_node] = result_tt;
    }
    
    return node_tt.find(node) != node_tt.end() ? node_tt[node] : 0;
}

// Check for simple resubstitution opportunities
void check_resubstitution_opportunities(const AIG& aig, const Window& window) {
    std::cout << "\n=== RESUBSTITUTION ANALYSIS ===\n";
    
    if (window.inputs.size() > 4) {
        std::cout << "Skipping - too many inputs (" << window.inputs.size() << ")\n";
        return;
    }
    
    // Compute target truth table
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, 
                                                     window.inputs, window.nodes);
    
    std::cout << "Target " << window.target_node << ": ";
    print_truth_table(target_tt, window.inputs.size());
    std::cout << "\n\n";
    
    // Check 0-resub: target = divisor
    std::cout << "0-RESUB OPPORTUNITIES (target = divisor):\n";
    bool found_0resub = false;
    for (int divisor : window.divisors) {
        uint64_t div_tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        
        if (div_tt == target_tt) {
            std::cout << "  ✓ Target " << window.target_node << " = Divisor " << divisor << "\n";
            std::cout << "    Both have TT: ";
            print_truth_table(div_tt, window.inputs.size());
            std::cout << "\n";
            found_0resub = true;
        }
        
        // Check complement
        uint64_t div_tt_compl = ~div_tt & ((1ULL << (1 << window.inputs.size())) - 1);
        if (div_tt_compl == target_tt) {
            std::cout << "  ✓ Target " << window.target_node << " = NOT(Divisor " << divisor << ")\n";
            std::cout << "    Target TT: ";
            print_truth_table(target_tt, window.inputs.size());
            std::cout << "\n    Divisor TT: ";
            print_truth_table(div_tt, window.inputs.size());
            std::cout << "\n";
            found_0resub = true;
        }
    }
    if (!found_0resub) {
        std::cout << "  No 0-resub opportunities found\n";
    }
    
    // Check 1-resub: target = divisor1 AND divisor2
    std::cout << "\n1-RESUB OPPORTUNITIES (target = div1 AND div2):\n";
    bool found_1resub = false;
    
    // Precompute all divisor truth tables
    std::map<int, uint64_t> divisor_tts;
    for (int divisor : window.divisors) {
        divisor_tts[divisor] = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
    }
    
    for (size_t i = 0; i < window.divisors.size() && !found_1resub; i++) {
        for (size_t j = i + 1; j < window.divisors.size() && !found_1resub; j++) {
            int div1 = window.divisors[i];
            int div2 = window.divisors[j];
            
            uint64_t tt1 = divisor_tts[div1];
            uint64_t tt2 = divisor_tts[div2];
            
            // Try different combinations
            uint64_t combinations[] = {
                tt1 & tt2,          // div1 AND div2
                tt1 & ~tt2,         // div1 AND NOT(div2)
                ~tt1 & tt2,         // NOT(div1) AND div2
                ~tt1 & ~tt2,        // NOT(div1) AND NOT(div2)
                tt1 | tt2,          // div1 OR div2 
                tt1 | ~tt2,         // div1 OR NOT(div2)
                ~tt1 | tt2,         // NOT(div1) OR div2
                ~tt1 | ~tt2,        // NOT(div1) OR NOT(div2)
                tt1 ^ tt2,          // div1 XOR div2
                ~(tt1 ^ tt2)        // NOT(div1 XOR div2)
            };
            
            const char* op_names[] = {
                "AND", "AND NOT", "NOT AND", "NOT AND NOT",
                "OR", "OR NOT", "NOT OR", "NOT OR NOT", 
                "XOR", "XNOR"
            };
            
            uint64_t mask = (1ULL << (1 << window.inputs.size())) - 1;
            
            for (int op = 0; op < 10; op++) {
                uint64_t result = combinations[op] & mask;
                if (result == target_tt) {
                    std::cout << "  ✓ Target " << window.target_node << " = " 
                             << div1 << " " << op_names[op] << " " << div2 << "\n";
                    std::cout << "    Target TT: ";
                    print_truth_table(target_tt, window.inputs.size());
                    std::cout << "\n    Result TT: ";
                    print_truth_table(result, window.inputs.size());
                    std::cout << "\n";
                    found_1resub = true;
                    break;
                }
            }
        }
    }
    
    if (!found_1resub) {
        std::cout << "  No 1-resub opportunities found (checked " 
                 << (window.divisors.size() * (window.divisors.size() - 1) / 2) 
                 << " divisor pairs)\n";
    }
    
    std::cout << "\nSUMMARY:\n";
    std::cout << "  - Target function complexity: " << __builtin_popcountll(target_tt) 
             << " ones out of " << (1 << window.inputs.size()) << " patterns\n";
    std::cout << "  - Available divisors: " << window.divisors.size() << "\n";
    std::cout << "  - 0-resub checked: " << window.divisors.size() << " candidates\n";
    std::cout << "  - 1-resub checked: " << (window.divisors.size() * (window.divisors.size() - 1) / 2) << " pairs\n";
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
    
    // Extract windows
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Total windows: " << windows.size() << "\n\n";
    
    // Test resubstitution opportunities
    std::cout << "=== RESUBSTITUTION OPPORTUNITY ANALYSIS ===\n\n";
    
    int windows_analyzed = 0;
    for (const auto& window : windows) {
        if (windows_analyzed >= 5) {
            std::cout << "... (limiting to 5 windows for analysis)\n";
            break;
        }
        
        if (window.inputs.size() > 4) {
            continue; // Skip large windows
        }
        
        std::cout << std::string(60, '=') << "\n";
        std::cout << "WINDOW " << windows_analyzed << " - TARGET " << window.target_node << "\n";
        std::cout << "Cut: ";
        Cut cut;
        cut.leaves = window.inputs;
        print_cut(cut, aig);
        std::cout << " (" << window.inputs.size() << " inputs)\n";
        std::cout << "Divisors: " << window.divisors.size() << " available\n";
        
        check_resubstitution_opportunities(aig, window);
        
        windows_analyzed++;
        std::cout << "\n";
    }
    
    return 0;
}