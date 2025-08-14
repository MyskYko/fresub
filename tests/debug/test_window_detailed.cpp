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

void print_node_vector(const std::vector<int>& nodes) {
    std::cout << "[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

// Print truth table in binary format with input labels
void print_truth_table_detailed(uint64_t tt, const std::vector<int>& inputs, const std::string& label) {
    int num_inputs = inputs.size();
    int num_patterns = 1 << num_inputs;
    
    std::cout << label << ":\n";
    
    // Print header
    std::cout << "  Inputs: ";
    for (int input : inputs) {
        std::cout << std::setw(3) << input;
    }
    std::cout << " | Output\n";
    std::cout << "  " << std::string(num_inputs * 3, '-') << "-+-------\n";
    
    // Print each pattern
    for (int p = 0; p < num_patterns; p++) {
        std::cout << "  ";
        for (int i = num_inputs - 1; i >= 0; i--) {
            std::cout << std::setw(3) << ((p >> i) & 1);
        }
        std::cout << " |   " << ((tt >> p) & 1) << "\n";
    }
    
    std::cout << "  Binary: ";
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")\n";
}

// Compute truth table for a node within a window
uint64_t compute_truth_table_for_node(const AIG& aig, int node, 
                                      const std::vector<int>& window_inputs,
                                      const std::vector<int>& window_nodes,
                                      std::map<int, uint64_t>& all_tts) {
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 6) return 0;
    
    int num_patterns = 1 << num_inputs;
    
    // Initialize primary input truth tables
    for (int i = 0; i < num_inputs; i++) {
        int pi = window_inputs[i];
        uint64_t pattern = 0;
        for (int p = 0; p < num_patterns; p++) {
            if ((p >> i) & 1) {
                pattern |= (1ULL << p);
            }
        }
        all_tts[pi] = pattern;
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
        
        if (all_tts.find(fanin0) == all_tts.end() || all_tts.find(fanin1) == all_tts.end()) {
            continue;
        }
        
        uint64_t tt0 = all_tts[fanin0];
        uint64_t tt1 = all_tts[fanin1];
        
        if (fanin0_compl) tt0 = ~tt0 & ((1ULL << num_patterns) - 1);
        if (fanin1_compl) tt1 = ~tt1 & ((1ULL << num_patterns) - 1);
        
        uint64_t result_tt = tt0 & tt1;
        all_tts[current_node] = result_tt;
    }
    
    return all_tts.find(node) != all_tts.end() ? all_tts[node] : 0;
}

void analyze_specific_window(const AIG& aig, const Window& window, int window_id) {
    std::cout << std::string(70, '=') << "\n";
    std::cout << "DETAILED ANALYSIS: WINDOW " << window_id << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    // Basic window info
    std::cout << "WINDOW STRUCTURE:\n";
    std::cout << "  Cut: ";
    Cut cut;
    cut.leaves = window.inputs;
    print_cut(cut, aig);
    std::cout << " (" << window.inputs.size() << " inputs)\n";
    
    std::cout << "  Window nodes: ";
    print_node_vector(window.nodes);
    std::cout << " (" << window.nodes.size() << " total)\n";
    
    std::cout << "  Divisors: ";
    print_node_vector(window.divisors);
    std::cout << " (" << window.divisors.size() << " available)\n\n";
    
    // Show AIG structure within window
    std::cout << "AIG STRUCTURE WITHIN WINDOW:\n";
    for (int node : window.nodes) {
        if (node <= aig.num_pis) {
            std::cout << "  Node " << node << " = PI\n";
        } else if (node < static_cast<int>(aig.nodes.size()) && !aig.nodes[node].is_dead) {
            int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
            int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
            
            std::cout << "  Node " << node << " = AND(";
            std::cout << fanin0;
            if (aig.is_complemented(aig.nodes[node].fanin0)) std::cout << "'";
            std::cout << ", " << fanin1;
            if (aig.is_complemented(aig.nodes[node].fanin1)) std::cout << "'";
            std::cout << ")";
            
            // Show fanouts within window
            std::cout << ", fanouts in window: [";
            bool first = true;
            for (int fanout : aig.nodes[node].fanouts) {
                if (std::find(window.nodes.begin(), window.nodes.end(), fanout) != window.nodes.end()) {
                    if (!first) std::cout << ", ";
                    std::cout << fanout;
                    first = false;
                }
            }
            std::cout << "]\n";
        }
    }
    
    // Compute all truth tables
    std::cout << "\nTRUTH TABLE COMPUTATION:\n";
    std::map<int, uint64_t> all_tts;
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, 
                                                     window.inputs, window.nodes, all_tts);
    
    // Show target truth table
    print_truth_table_detailed(target_tt, window.inputs, "TARGET " + std::to_string(window.target_node));
    std::cout << "\n";
    
    // Show all divisor truth tables
    std::cout << "DIVISOR TRUTH TABLES:\n";
    for (int divisor : window.divisors) {
        if (all_tts.find(divisor) != all_tts.end()) {
            std::cout << "Divisor " << divisor << ": ";
            int num_patterns = 1 << window.inputs.size();
            for (int i = num_patterns - 1; i >= 0; i--) {
                std::cout << ((all_tts[divisor] >> i) & 1);
            }
            std::cout << " (0x" << std::hex << all_tts[divisor] << std::dec << ")";
            std::cout << " [" << __builtin_popcountll(all_tts[divisor]) << " ones]\n";
        }
    }
    
    // Detailed resubstitution analysis
    std::cout << "\nRESUBSTITUTION ANALYSIS:\n";
    
    // 0-resub
    std::cout << "\n0-RESUB (target = divisor):\n";
    bool found_0resub = false;
    for (int divisor : window.divisors) {
        if (all_tts.find(divisor) != all_tts.end()) {
            uint64_t div_tt = all_tts[divisor];
            uint64_t mask = (1ULL << (1 << window.inputs.size())) - 1;
            
            if (div_tt == target_tt) {
                std::cout << "  ✓ FOUND: " << window.target_node << " = " << divisor << "\n";
                std::cout << "    Savings: 1 gate (can remove target, replace with wire to divisor)\n";
                found_0resub = true;
            }
            
            uint64_t div_tt_compl = (~div_tt) & mask;
            if (div_tt_compl == target_tt) {
                std::cout << "  ✓ FOUND: " << window.target_node << " = NOT(" << divisor << ")\n";
                std::cout << "    Savings: 0 gates (replace AND with NOT)\n";
                found_0resub = true;
            }
        }
    }
    if (!found_0resub) {
        std::cout << "  No 0-resub opportunities\n";
    }
    
    // 1-resub (show first few)
    std::cout << "\n1-RESUB (target = div1 OP div2) - showing first few:\n";
    int resub_count = 0;
    for (size_t i = 0; i < window.divisors.size() && resub_count < 5; i++) {
        for (size_t j = i + 1; j < window.divisors.size() && resub_count < 5; j++) {
            int div1 = window.divisors[i];
            int div2 = window.divisors[j];
            
            if (all_tts.find(div1) == all_tts.end() || all_tts.find(div2) == all_tts.end()) continue;
            
            uint64_t tt1 = all_tts[div1];
            uint64_t tt2 = all_tts[div2];
            uint64_t mask = (1ULL << (1 << window.inputs.size())) - 1;
            
            struct {
                uint64_t result;
                const char* op_name;
                bool saves_gate;
            } combinations[] = {
                {tt1 & tt2, "AND", true},
                {tt1 | tt2, "OR", true},
                {tt1 ^ tt2, "XOR", false},
                {(tt1 & ~tt2) & mask, "AND NOT", false},
                {(~tt1 & tt2) & mask, "NOT AND", false}
            };
            
            for (auto& combo : combinations) {
                if (combo.result == target_tt) {
                    std::cout << "  ✓ FOUND: " << window.target_node << " = " 
                             << div1 << " " << combo.op_name << " " << div2 << "\n";
                    if (combo.saves_gate) {
                        std::cout << "    Savings: 0 gates (same complexity)\n";
                    } else {
                        std::cout << "    Savings: -1 gates (increases complexity)\n";
                    }
                    resub_count++;
                    break;
                }
            }
        }
    }
    
    // Show optimization potential
    std::cout << "\nOPTIMIZATION POTENTIAL:\n";
    std::cout << "  Current implementation: Target " << window.target_node;
    if (window.target_node < static_cast<int>(aig.nodes.size()) && !aig.nodes[window.target_node].is_dead) {
        int fanin0 = aig.lit2var(aig.nodes[window.target_node].fanin0);
        int fanin1 = aig.lit2var(aig.nodes[window.target_node].fanin1);
        std::cout << " = AND(" << fanin0;
        if (aig.is_complemented(aig.nodes[window.target_node].fanin0)) std::cout << "'";
        std::cout << ", " << fanin1;
        if (aig.is_complemented(aig.nodes[window.target_node].fanin1)) std::cout << "'";
        std::cout << ")";
    }
    std::cout << "\n";
    
    std::cout << "  Search space: " << window.divisors.size() << " divisors\n";
    std::cout << "  0-resub combinations: " << window.divisors.size() << " (direct + complement)\n";
    std::cout << "  1-resub combinations: " << (window.divisors.size() * (window.divisors.size() - 1) / 2 * 10) << " (pairs × operations)\n";
    
    // Function complexity analysis
    int target_ones = __builtin_popcountll(target_tt);
    int total_patterns = 1 << window.inputs.size();
    std::cout << "  Function complexity: " << target_ones << "/" << total_patterns 
             << " patterns are 1 (" << (100.0 * target_ones / total_patterns) << "%)\n";
    
    std::cout << "\n";
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
    
    // Analyze windows 3 and 4 specifically
    if (windows.size() > 3) {
        analyze_specific_window(aig, windows[3], 3);
    }
    
    if (windows.size() > 4) {
        analyze_specific_window(aig, windows[4], 4);
    }
    
    return 0;
}