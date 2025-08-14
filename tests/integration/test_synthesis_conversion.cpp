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

// CPU implementation of gresub feasibility check
uint32_t solve_resub_overlap_cpu(int i, int j, int k, int l, 
                                const std::vector<uint64_t>& truth_tables,
                                uint64_t target_tt, int num_inputs) {
    
    if (i >= truth_tables.size() || j >= truth_tables.size() || 
        k >= truth_tables.size() || l >= truth_tables.size()) {
        return 0;
    }
    
    uint32_t res = ((1U << (i % 30)) | (1U << (j % 30)) | (1U << (k % 30)) | (1U << (l % 30)));
    uint32_t qs[32] = {0};
    
    int num_patterns = 1 << num_inputs;
    if (num_patterns > 64) return 0;
    
    uint64_t t_i = truth_tables[i];
    uint64_t t_j = truth_tables[j];
    uint64_t t_k = truth_tables[k];
    uint64_t t_l = truth_tables[l];
    
    uint64_t mask = (1ULL << num_patterns) - 1;
    uint64_t t_on = target_tt & mask;
    uint64_t t_off = (~target_tt) & mask;
    
    for (int p = 0; p < num_patterns; p++) {
        uint32_t bit_i = (t_i >> p) & 1;
        uint32_t bit_j = (t_j >> p) & 1;
        uint32_t bit_k = (t_k >> p) & 1;
        uint32_t bit_l = (t_l >> p) & 1;
        uint32_t bit_off = (t_off >> p) & 1;
        uint32_t bit_on = (t_on >> p) & 1;
        
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
    
    for (int h = 0; h < 16; h++) {
        int fail = ((qs[2*h] != 0) && (qs[2*h+1] != 0));
        res = fail ? 0 : res;
    }
    
    return res;
}

// Find feasible 4-input resubstitution
struct FeasibilityResult {
    bool found;
    std::vector<int> divisor_indices;  // Which divisors in the window
    std::vector<int> divisor_nodes;    // Actual node IDs
    uint32_t mask;
};

FeasibilityResult find_feasible_4resub(const AIG& aig, const Window& window) {
    FeasibilityResult result;
    result.found = false;
    
    if (window.inputs.size() > 4 || window.divisors.size() < 4) {
        return result;
    }
    
    // Compute truth tables for all divisors
    std::vector<uint64_t> divisor_tts;
    for (int divisor : window.divisors) {
        uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        divisor_tts.push_back(tt);
    }
    
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    
    // Try all combinations of 4 divisors
    for (size_t i = 0; i < window.divisors.size(); i++) {
        for (size_t j = i + 1; j < window.divisors.size(); j++) {
            for (size_t k = j + 1; k < window.divisors.size(); k++) {
                for (size_t l = k + 1; l < window.divisors.size(); l++) {
                    uint32_t mask = solve_resub_overlap_cpu(i, j, k, l, divisor_tts, 
                                                          target_tt, window.inputs.size());
                    if (mask > 0) {
                        result.found = true;
                        result.divisor_indices = {static_cast<int>(i), static_cast<int>(j), 
                                                static_cast<int>(k), static_cast<int>(l)};
                        result.divisor_nodes = {window.divisors[i], window.divisors[j], 
                                              window.divisors[k], window.divisors[l]};
                        result.mask = mask;
                        return result;
                    }
                }
            }
        }
    }
    
    return result;
}

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(uint64_t target_tt, 
                            const std::vector<uint64_t>& divisor_tts,
                            const std::vector<int>& selected_divisors,
                            int num_inputs,
                            std::vector<std::vector<bool>>& br,
                            std::vector<std::vector<bool>>& sim) {
    
    int num_patterns = 1 << num_inputs;
    
    // Initialize br[input_pattern][output_pattern]
    // For single output: br[pattern][0] = target should be 0, br[pattern][1] = target should be 1
    br.clear();
    br.resize(num_patterns, std::vector<bool>(2, false));
    
    // Initialize sim[input_pattern][divisor]
    sim.clear();
    sim.resize(num_patterns, std::vector<bool>(selected_divisors.size(), false));
    
    std::cout << "CONVERTING TO EXOPT FORMAT:\n";
    std::cout << "  Input patterns: " << num_patterns << "\n";
    std::cout << "  Output values: 2 (0 or 1)\n";
    std::cout << "  Selected divisors: " << selected_divisors.size() << "\n\n";
    
    std::cout << "  Conversion table:\n";
    std::cout << "  Pattern | ";
    for (int i = num_inputs - 1; i >= 0; i--) {
        std::cout << "I" << i << " ";
    }
    std::cout << "| Target | Divisors\n";
    std::cout << "  " << std::string(9 + num_inputs * 3 + 8 + selected_divisors.size(), '-') << "\n";
    
    for (int p = 0; p < num_patterns; p++) {
        // Set target output value for this input pattern
        bool target_value = (target_tt >> p) & 1;
        br[p][target_value ? 1 : 0] = true;
        
        // Set divisor values for this input pattern
        for (size_t d = 0; d < selected_divisors.size(); d++) {
            bool divisor_value = (divisor_tts[selected_divisors[d]] >> p) & 1;
            sim[p][d] = divisor_value;
        }
        
        // Print this conversion
        std::cout << "     " << std::setw(2) << p << "  | ";
        for (int i = num_inputs - 1; i >= 0; i--) {
            std::cout << " " << ((p >> i) & 1) << " ";
        }
        std::cout << "|   " << (target_value ? '1' : '0') << "    | ";
        for (size_t d = 0; d < selected_divisors.size(); d++) {
            std::cout << (sim[p][d] ? '1' : '0');
        }
        std::cout << "\n";
    }
}

// Write exopt synthesis input file
void write_exopt_synthesis_file(const std::vector<std::vector<bool>>& br,
                               const std::vector<std::vector<bool>>& sim,
                               const std::string& filename) {
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing\n";
        return;
    }
    
    int num_patterns = br.size();
    int num_outputs = br[0].size();
    int num_divisors = sim[0].size();
    
    file << "# EXOPT Synthesis Input File\n";
    file << "# Generated from fresub feasibility check\n";
    file << "# Patterns: " << num_patterns << ", Outputs: " << num_outputs << ", Divisors: " << num_divisors << "\n\n";
    
    file << "# Binary Relation (br): br[input_pattern][output_pattern]\n";
    for (int p = 0; p < num_patterns; p++) {
        file << "br[" << p << "] = ";
        for (int o = 0; o < num_outputs; o++) {
            file << (br[p][o] ? '1' : '0');
        }
        file << "\n";
    }
    
    file << "\n# Simulation Matrix (sim): sim[input_pattern][divisor]\n";
    for (int p = 0; p < num_patterns; p++) {
        file << "sim[" << p << "] = ";
        for (int d = 0; d < num_divisors; d++) {
            file << (sim[p][d] ? '1' : '0');
        }
        file << "\n";
    }
    
    file << "\n# Usage: Can be used with exopt SynthMan constructor:\n";
    file << "# SynthMan<KissatSolver> synth_man(br, &sim);\n";
    file << "# aigman* result = synth_man.ExSynth(max_gates);\n";
    
    file.close();
    std::cout << "  ✓ Synthesis input written to " << filename << "\n";
}

void analyze_synthesis_conversion(const AIG& aig, const Window& window, int window_id) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "SYNTHESIS CONVERSION: WINDOW " << window_id 
              << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(80, '=') << "\n";
    
    // Find feasible resubstitution
    FeasibilityResult feasible = find_feasible_4resub(aig, window);
    
    if (!feasible.found) {
        std::cout << "No feasible 4-input resubstitution found - skipping conversion\n";
        return;
    }
    
    std::cout << "FEASIBILITY RESULT:\n";
    std::cout << "  ✓ Found feasible 4-input resubstitution\n";
    std::cout << "  Mask: 0x" << std::hex << feasible.mask << std::dec << "\n";
    std::cout << "  Divisor indices: {";
    for (size_t i = 0; i < feasible.divisor_indices.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << feasible.divisor_indices[i];
    }
    std::cout << "}\n";
    std::cout << "  Divisor nodes: {";
    for (size_t i = 0; i < feasible.divisor_nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << feasible.divisor_nodes[i];
    }
    std::cout << "}\n\n";
    
    // Compute truth tables
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    std::vector<uint64_t> divisor_tts;
    for (int divisor : window.divisors) {
        uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        divisor_tts.push_back(tt);
    }
    
    // Convert to exopt format
    std::vector<std::vector<bool>> br, sim;
    convert_to_exopt_format(target_tt, divisor_tts, feasible.divisor_indices, 
                           window.inputs.size(), br, sim);
    
    // Write synthesis input file
    std::string filename = "synthesis_input_window_" + std::to_string(window_id) + ".txt";
    write_exopt_synthesis_file(br, sim, filename);
    
    std::cout << "\nSYNTHESIS READY!\n";
    std::cout << "  Data converted to exopt format successfully\n";
    std::cout << "  Ready for synthesis with: SynthMan<KissatSolver> synth_man(br, &sim)\n";
    std::cout << "  Expected result: Optimal circuit with ≤4 gates expressing target function\n";
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
    
    std::cout << "\n=== SYNTHESIS DATA CONVERSION TESTING ===\n";
    std::cout << "Converting feasible resubstitutions to exopt synthesis format\n";
    
    // Find windows suitable for synthesis
    int analyzed = 0;
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4 && 
            windows[i].divisors.size() >= 4) {
            analyze_synthesis_conversion(aig, windows[i], i);
            analyzed++;
            
            if (analyzed >= 2) {
                std::cout << "... (analyzed first 2 suitable windows)\n";
                break;
            }
        }
    }
    
    if (analyzed == 0) {
        std::cout << "No suitable windows found for synthesis conversion\n";
    }
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Successfully converted feasible resubstitutions to exopt format\n";
    std::cout << "Generated synthesis input files ready for exact synthesis\n";
    std::cout << "Next step: Use exopt SynthMan to synthesize optimal circuits\n";
    
    return 0;
}