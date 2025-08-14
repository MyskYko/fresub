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

// CPU implementation of the feasibility check from gresub/src/cpp/main.cpp
// This replaces basic pattern matching with proper word-level feasibility check
uint32_t solve_resub_overlap_cpu(int i, int j, int k, int l, 
                                const std::vector<uint64_t>& truth_tables,
                                uint64_t target_tt, int num_inputs) {
    
    if (i >= truth_tables.size() || j >= truth_tables.size() || 
        k >= truth_tables.size() || l >= truth_tables.size()) {
        return 0;
    }
    
    // Create mask with actual divisor positions (limited to 30 bits like gresub)
    uint32_t res = ((1U << (i % 30)) | (1U << (j % 30)) | (1U << (k % 30)) | (1U << (l % 30)));
    uint32_t qs[32] = {0};
    
    int num_patterns = 1 << num_inputs;
    if (num_patterns > 64) return 0; // Limited to 64-bit truth tables
    
    // Get truth tables for the 4 divisors
    uint64_t t_i = truth_tables[i];
    uint64_t t_j = truth_tables[j];
    uint64_t t_k = truth_tables[k];
    uint64_t t_l = truth_tables[l];
    
    // Create offset and onset from target truth table
    uint64_t mask = (1ULL << num_patterns) - 1;
    uint64_t t_on = target_tt & mask;          // Where target should be 1
    uint64_t t_off = (~target_tt) & mask;      // Where target should be 0
    
    // Process each bit position (pattern)
    for (int p = 0; p < num_patterns; p++) {
        uint32_t bit_i = (t_i >> p) & 1;
        uint32_t bit_j = (t_j >> p) & 1;
        uint32_t bit_k = (t_k >> p) & 1;
        uint32_t bit_l = (t_l >> p) & 1;
        uint32_t bit_off = (t_off >> p) & 1;
        uint32_t bit_on = (t_on >> p) & 1;
        
        // Apply the 32 qs update rules from solve_resub_overlap_t
        qs[0]  |= (bit_off &  bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[1]  |= (bit_on  &  bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[2]  |= (bit_off & ~bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[3]  |= (bit_on  & ~bit_i &  bit_j) & ( bit_k &  bit_l);
        qs[4]  |= (bit_off &  bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[5]  |= (bit_on  &  bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[6]  |= (bit_off & ~bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[7]  |= (bit_on  & ~bit_i & ~bit_j) & ( bit_k &  bit_l);
        qs[8]  |= (bit_off &  bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[9]  |= (bit_on  &  bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[10] |= (bit_off & ~bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[11] |= (bit_on  & ~bit_i &  bit_j) & (~bit_k &  bit_l);
        qs[12] |= (bit_off &  bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[13] |= (bit_on  &  bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[14] |= (bit_off & ~bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[15] |= (bit_on  & ~bit_i & ~bit_j) & (~bit_k &  bit_l);
        qs[16] |= (bit_off &  bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[17] |= (bit_on  &  bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[18] |= (bit_off & ~bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[19] |= (bit_on  & ~bit_i &  bit_j) & ( bit_k & ~bit_l);
        qs[20] |= (bit_off &  bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[21] |= (bit_on  &  bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[22] |= (bit_off & ~bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[23] |= (bit_on  & ~bit_i & ~bit_j) & ( bit_k & ~bit_l);
        qs[24] |= (bit_off &  bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[25] |= (bit_on  &  bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[26] |= (bit_off & ~bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[27] |= (bit_on  & ~bit_i &  bit_j) & (~bit_k & ~bit_l);
        qs[28] |= (bit_off &  bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[29] |= (bit_on  &  bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[30] |= (bit_off & ~bit_i & ~bit_j) & (~bit_k & ~bit_l);
        qs[31] |= (bit_on  & ~bit_i & ~bit_j) & (~bit_k & ~bit_l);
    }
    
    // Check for feasibility failure
    for (int h = 0; h < 16; h++) {
        int fail = ((qs[2*h] != 0) && (qs[2*h+1] != 0));
        res = fail ? 0 : res; // resubstitution fails
    }
    
    return res;
}

// Find 4-input resubstitutions using the feasibility check approach
uint32_t find_4resub_feasibility(const AIG& aig, const Window& window) {
    if (window.inputs.size() > 4 || window.divisors.size() < 4) {
        return 0; // Not enough divisors or too many inputs
    }
    
    // Compute truth tables for all divisors
    std::vector<uint64_t> divisor_tts;
    std::map<int, uint64_t> all_tts;
    
    for (int divisor : window.divisors) {
        uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        divisor_tts.push_back(tt);
        all_tts[divisor] = tt;
    }
    
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    
    // Try all combinations of 4 divisors
    uint32_t result = 0;
    int combinations_tested = 0;
    for (size_t i = 0; i < window.divisors.size() && result == 0; i++) {
        for (size_t j = i + 1; j < window.divisors.size() && result == 0; j++) {
            for (size_t k = j + 1; k < window.divisors.size() && result == 0; k++) {
                for (size_t l = k + 1; l < window.divisors.size() && result == 0; l++) {
                    combinations_tested++;
                    uint32_t test_result = solve_resub_overlap_cpu(i, j, k, l, divisor_tts, 
                                                                 target_tt, window.inputs.size());
                    if (test_result > 0) {
                        result = test_result;
                        // Debug: show which combination succeeded
                        std::cout << "  Debug: Combination (" << i << "," << j << "," << k << "," << l 
                                 << ") succeeded with divisors {" << window.divisors[i] << "," 
                                 << window.divisors[j] << "," << window.divisors[k] << "," 
                                 << window.divisors[l] << "}\n";
                    }
                }
            }
        }
    }
    
    std::cout << "  Debug: Tested " << combinations_tested << " combinations\n";
    
    return result;
}

void print_truth_table(uint64_t tt, int num_inputs, const std::string& label) {
    std::cout << label << ": ";
    int num_patterns = 1 << num_inputs;
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")";
}

void analyze_feasibility_check(const AIG& aig, const Window& window, int window_id) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "FEASIBILITY CHECK ANALYSIS: WINDOW " << window_id 
              << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(70, '=') << "\n";
    
    if (window.inputs.size() > 4) {
        std::cout << "Skipping - too many inputs (" << window.inputs.size() << ")\n";
        return;
    }
    
    if (window.divisors.size() < 4) {
        std::cout << "Skipping - need at least 4 divisors (have " << window.divisors.size() << ")\n";
        return;
    }
    
    std::cout << "Window: " << window.divisors.size() << " divisors, " 
              << window.inputs.size() << " inputs\n";
    
    // Show target truth table
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    print_truth_table(target_tt, window.inputs.size(), "Target " + std::to_string(window.target_node));
    std::cout << "\n";
    
    // Show first few divisor truth tables
    std::cout << "\nDivisor truth tables:\n";
    for (size_t i = 0; i < std::min((size_t)6, window.divisors.size()); i++) {
        uint64_t div_tt = compute_truth_table_for_node(aig, window.divisors[i], window.inputs, window.nodes);
        std::cout << "  ";
        print_truth_table(div_tt, window.inputs.size(), "Div[" + std::to_string(i) + "](" + std::to_string(window.divisors[i]) + ")");
        std::cout << "\n";
    }
    
    // Apply feasibility check
    std::cout << "\nAPPLYING FEASIBILITY CHECK:\n";
    uint32_t result = find_4resub_feasibility(aig, window);
    
    if (result > 0) {
        std::cout << "✓ FOUND 4-RESUB: Mask = 0x" << std::hex << result << std::dec << "\n";
        std::cout << "  Selected divisor array indices: {";
        bool first = true;
        for (int i = 0; i < 30; i++) {
            if ((result >> i) & 1) {
                if (!first) std::cout << ", ";
                std::cout << i;
                first = false;
            }
        }
        std::cout << "}\n";
        
        // Also show which actual divisor nodes these correspond to
        std::cout << "  Corresponding divisor nodes: {";
        first = true;
        for (int i = 0; i < 30; i++) {
            if ((result >> i) & 1) {
                if (!first) std::cout << ", ";
                if (i < window.divisors.size()) {
                    std::cout << window.divisors[i];
                } else {
                    std::cout << "INVALID[" << i << "]";
                }
                first = false;
            }
        }
        std::cout << "}\n";
    } else {
        std::cout << "✗ No 4-input resubstitution found with feasibility check\n";
    }
    
    // Compare with basic pattern matching for validation
    std::cout << "\nCOMPARISON WITH BASIC PATTERN MATCHING:\n";
    
    // Check 1-resub for comparison
    bool found_1resub = false;
    for (size_t i = 0; i < window.divisors.size() && !found_1resub; i++) {
        for (size_t j = i + 1; j < window.divisors.size() && !found_1resub; j++) {
            uint64_t tt1 = compute_truth_table_for_node(aig, window.divisors[i], window.inputs, window.nodes);
            uint64_t tt2 = compute_truth_table_for_node(aig, window.divisors[j], window.inputs, window.nodes);
            
            if ((tt1 & tt2) == target_tt) {
                std::cout << "  Basic: Found 1-resub " << window.divisors[i] << " AND " << window.divisors[j] << "\n";
                found_1resub = true;
            }
        }
    }
    
    if (!found_1resub) {
        std::cout << "  Basic: No 1-resub found\n";
    }
    
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
              << aig.num_nodes << " nodes\n";
    
    // Extract windows
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Total windows extracted: " << windows.size() << "\n";
    
    std::cout << "\n=== FEASIBILITY CHECK TESTING ===\n";
    std::cout << "Testing CPU implementation of solve_resub_overlap from gresub\n";
    
    // Find windows suitable for 4-input resubstitution
    int analyzed = 0;
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4 && 
            windows[i].divisors.size() >= 4) {
            analyze_feasibility_check(aig, windows[i], i);
            analyzed++;
            
            if (analyzed >= 3) {
                std::cout << "... (analyzed first 3 suitable windows)\n";
                break;
            }
        }
    }
    
    if (analyzed == 0) {
        std::cout << "No suitable windows found (need 3-4 inputs and ≥4 divisors)\n";
        
        // Show what windows we do have
        std::cout << "\nAvailable windows:\n";
        for (size_t i = 0; i < std::min((size_t)5, windows.size()); i++) {
            std::cout << "  Window " << i << ": " << windows[i].inputs.size() 
                     << " inputs, " << windows[i].divisors.size() << " divisors\n";
        }
    }
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Implemented CPU version of gresub feasibility check\n";
    std::cout << "This replaces basic pattern matching with proper 4-input resubstitution detection\n";
    std::cout << "The feasibility check uses 32 qs arrays to validate resubstitution correctness\n";
    
    return 0;
}