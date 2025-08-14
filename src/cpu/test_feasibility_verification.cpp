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
#include <functional>
#include "fresub_aig.hpp"
#include "window.hpp"

using namespace fresub;

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

void print_truth_table_patterns(uint64_t tt, int num_inputs, const std::string& label) {
    std::cout << label << ":\n";
    int num_patterns = 1 << num_inputs;
    
    // Print header
    std::cout << "  Pattern: ";
    for (int i = num_inputs - 1; i >= 0; i--) {
        std::cout << "I" << i << " ";
    }
    std::cout << "| Output\n";
    std::cout << "  " << std::string(num_inputs * 3 + 8, '-') << "\n";
    
    // Print each pattern
    for (int p = 0; p < num_patterns; p++) {
        std::cout << "     " << std::setw(2) << p << ": ";
        for (int i = num_inputs - 1; i >= 0; i--) {
            std::cout << " " << ((p >> i) & 1) << " ";
        }
        std::cout << "|   " << ((tt >> p) & 1) << "\n";
    }
    std::cout << "  Binary: ";
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")\n\n";
}

// Verify that a 4-input function can be expressed using the given divisors
bool verify_4input_resubstitution(uint64_t target_tt, 
                                  const std::vector<uint64_t>& div_tts,
                                  const std::vector<int>& divisor_indices,
                                  int num_inputs) {
    
    if (divisor_indices.size() != 4) return false;
    
    std::cout << "VERIFICATION: Checking if target can be expressed with 4 divisors\n";
    
    uint64_t mask = (1ULL << (1 << num_inputs)) - 1;
    
    // Get the 4 divisor truth tables
    uint64_t d0 = div_tts[divisor_indices[0]];
    uint64_t d1 = div_tts[divisor_indices[1]];
    uint64_t d2 = div_tts[divisor_indices[2]];
    uint64_t d3 = div_tts[divisor_indices[3]];
    
    std::cout << "Divisor truth tables:\n";
    std::cout << "  D0: " << std::bitset<16>(d0) << " (0x" << std::hex << d0 << std::dec << ")\n";
    std::cout << "  D1: " << std::bitset<16>(d1) << " (0x" << std::hex << d1 << std::dec << ")\n";
    std::cout << "  D2: " << std::bitset<16>(d2) << " (0x" << std::hex << d2 << std::dec << ")\n";
    std::cout << "  D3: " << std::bitset<16>(d3) << " (0x" << std::hex << d3 << std::dec << ")\n";
    std::cout << "  Target: " << std::bitset<16>(target_tt) << " (0x" << std::hex << target_tt << std::dec << ")\n\n";
    
    // Try various 4-input Boolean functions
    std::vector<std::pair<std::string, uint64_t>> function_tests = {
        {"D0 & D1 & D2 & D3", (d0 & d1 & d2 & d3) & mask},
        {"D0 & D1 & D2 & ~D3", (d0 & d1 & d2 & (~d3)) & mask},
        {"D0 & D1 & ~D2 & D3", (d0 & d1 & (~d2) & d3) & mask},
        {"D0 & ~D1 & D2 & D3", (d0 & (~d1) & d2 & d3) & mask},
        {"~D0 & D1 & D2 & D3", ((~d0) & d1 & d2 & d3) & mask},
        {"D0 | D1 | D2 | D3", (d0 | d1 | d2 | d3) & mask},
        {"D0 ^ D1 ^ D2 ^ D3", (d0 ^ d1 ^ d2 ^ d3) & mask},
        {"(D0 & D1) | (D2 & D3)", ((d0 & d1) | (d2 & d3)) & mask},
        {"(D0 | D1) & (D2 | D3)", ((d0 | d1) & (d2 | d3)) & mask},
        {"D0 & D1 & D2", (d0 & d1 & d2) & mask},
        {"D0 & D1", (d0 & d1) & mask},
        {"D2 & D3", (d2 & d3) & mask},
        {"~(D0 & D1 & D2 & D3)", (~(d0 & d1 & d2 & d3)) & mask},
        {"~(D0 | D1 | D2 | D3)", (~(d0 | d1 | d2 | d3)) & mask},
    };
    
    bool found_match = false;
    for (const auto& test : function_tests) {
        if (test.second == target_tt) {
            std::cout << "✓ MATCH FOUND: Target = " << test.first << "\n";
            std::cout << "  Result: " << std::bitset<16>(test.second) << " (0x" << std::hex << test.second << std::dec << ")\n";
            found_match = true;
            break;
        }
    }
    
    if (!found_match) {
        std::cout << "✗ No simple 4-input Boolean function found\n";
        std::cout << "  This might require a more complex combination or the feasibility check\n";
        std::cout << "  detected a valid resubstitution that isn't obvious with simple operations\n";
    }
    
    return found_match;
}

void detailed_window_analysis(const AIG& aig, const Window& window, int window_id) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "DETAILED VERIFICATION: WINDOW " << window_id 
              << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(80, '=') << "\n";
    
    if (window.inputs.size() > 4) {
        std::cout << "Skipping - too many inputs\n";
        return;
    }
    
    // Compute all truth tables
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    std::vector<uint64_t> divisor_tts;
    
    for (int divisor : window.divisors) {
        uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        divisor_tts.push_back(tt);
    }
    
    // Show target function in detail
    print_truth_table_patterns(target_tt, window.inputs.size(), "TARGET FUNCTION " + std::to_string(window.target_node));
    
    // Show the first 4 divisors in detail
    std::cout << "FIRST 4 DIVISORS:\n";
    for (int i = 0; i < std::min(4, static_cast<int>(window.divisors.size())); i++) {
        print_truth_table_patterns(divisor_tts[i], window.inputs.size(), 
                                   "DIVISOR[" + std::to_string(i) + "] = NODE " + std::to_string(window.divisors[i]));
    }
    
    // Verify if target can be expressed with first 4 divisors
    if (window.divisors.size() >= 4) {
        std::vector<int> first_four = {0, 1, 2, 3};
        verify_4input_resubstitution(target_tt, divisor_tts, first_four, window.inputs.size());
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
    
    // Extract windows
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Total windows extracted: " << windows.size() << "\n";
    
    // Analyze windows where feasibility check succeeded
    int analyzed = 0;
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4 && 
            windows[i].divisors.size() >= 4) {
            detailed_window_analysis(aig, windows[i], i);
            analyzed++;
            
            if (analyzed >= 2) {
                std::cout << "... (analyzed first 2 windows in detail)\n";
                break;
            }
        }
    }
    
    return 0;
}