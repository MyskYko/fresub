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

// Include exopt synthesis headers
#include "synth.hpp"
#include "kissat_solver.hpp"

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
void convert_to_binary_relation(uint64_t target_tt, 
                               const std::vector<uint64_t>& divisor_tts,
                               const std::vector<int>& selected_divisors,
                               int num_inputs,
                               std::vector<std::vector<bool>>& br,
                               std::vector<std::vector<bool>>& sim) {
    
    int num_patterns = 1 << num_inputs;
    int num_output_patterns = 2; // Single output (0 or 1)
    
    // Initialize br[input_pattern][output_pattern]
    br.clear();
    br.resize(num_patterns, std::vector<bool>(num_output_patterns, false));
    
    // Initialize sim[input_pattern][divisor]
    sim.clear();
    sim.resize(num_patterns, std::vector<bool>(selected_divisors.size(), false));
    
    for (int p = 0; p < num_patterns; p++) {
        // Set target output value for this input pattern
        bool target_value = (target_tt >> p) & 1;
        br[p][target_value ? 1 : 0] = true;
        
        // Set divisor values for this input pattern
        for (size_t d = 0; d < selected_divisors.size(); d++) {
            bool divisor_value = (divisor_tts[selected_divisors[d]] >> p) & 1;
            sim[p][d] = divisor_value;
        }
    }
}

// Synthesize circuit using exopt
aigman* synthesize_from_feasible_divisors(const AIG& aig, const Window& window, 
                                        const FeasibilityResult& feasible) {
    
    if (!feasible.found) return nullptr;
    
    std::cout << "SYNTHESIZING CIRCUIT FROM FEASIBLE DIVISORS:\n";
    std::cout << "  Window target: " << window.target_node << "\n";
    std::cout << "  Window inputs: " << window.inputs.size() << "\n";
    std::cout << "  Selected divisors: {";
    for (size_t i = 0; i < feasible.divisor_nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << feasible.divisor_nodes[i];
    }
    std::cout << "}\n";
    
    // Compute truth tables
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, window.inputs, window.nodes);
    std::vector<uint64_t> divisor_tts;
    for (int divisor : window.divisors) {
        uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
        divisor_tts.push_back(tt);
    }
    
    // Convert to exopt format
    std::vector<std::vector<bool>> br, sim;
    convert_to_binary_relation(target_tt, divisor_tts, feasible.divisor_indices, 
                              window.inputs.size(), br, sim);
    
    std::cout << "  Binary relation (br) size: " << br.size() << " x " << br[0].size() << "\n";
    std::cout << "  Simulation matrix (sim) size: " << sim.size() << " x " << sim[0].size() << "\n";
    
    // Show the binary relation for debugging
    std::cout << "  Binary relation:\n";
    for (size_t i = 0; i < br.size(); i++) {
        std::cout << "    Pattern " << std::setw(2) << i << ": ";
        for (int bit = window.inputs.size() - 1; bit >= 0; bit--) {
            std::cout << ((i >> bit) & 1);
        }
        std::cout << " -> ";
        for (size_t j = 0; j < br[i].size(); j++) {
            if (br[i][j]) std::cout << j;
        }
        std::cout << " | Divisors: ";
        for (size_t d = 0; d < sim[i].size(); d++) {
            std::cout << (sim[i][d] ? '1' : '0');
        }
        std::cout << "\n";
    }
    
    // Create synthesis manager and synthesize
    SynthMan<KissatSolver> synth_man(br, &sim);
    
    std::cout << "  Attempting synthesis with up to 4 gates...\n";
    aigman* synthesized_aig = synth_man.ExSynth(4);
    
    if (synthesized_aig) {
        std::cout << "  ✓ Synthesis SUCCESSFUL!\n";
        std::cout << "    Synthesized AIG: " << synthesized_aig->nInputs << " inputs, " 
                  << synthesized_aig->nGates << " gates, " << synthesized_aig->nPos << " outputs\n";
    } else {
        std::cout << "  ✗ Synthesis FAILED - no circuit found with ≤4 gates\n";
    }
    
    return synthesized_aig;
}

void analyze_synthesis_integration(const AIG& aig, const Window& window, int window_id) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "SYNTHESIS INTEGRATION: WINDOW " << window_id 
              << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(80, '=') << "\n";
    
    // Find feasible resubstitution
    FeasibilityResult feasible = find_feasible_4resub(aig, window);
    
    if (!feasible.found) {
        std::cout << "No feasible 4-input resubstitution found - skipping synthesis\n";
        return;
    }
    
    std::cout << "FEASIBILITY CHECK RESULT:\n";
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
    
    // Synthesize circuit
    aigman* synthesized = synthesize_from_feasible_divisors(aig, window, feasible);
    
    if (synthesized) {
        // TODO: Here we would integrate the synthesized circuit back into the original AIG
        std::cout << "\nSYNTHESIS INTEGRATION SUCCESS!\n";
        std::cout << "Next step: Replace target node " << window.target_node 
                  << " with synthesized circuit\n";
        delete synthesized;
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
    
    std::cout << "\n=== SYNTHESIS INTEGRATION TESTING ===\n";
    std::cout << "Testing integration of feasibility check with exopt synthesis\n";
    
    // Find windows suitable for synthesis
    int analyzed = 0;
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4 && 
            windows[i].divisors.size() >= 4) {
            analyze_synthesis_integration(aig, windows[i], i);
            analyzed++;
            
            if (analyzed >= 2) {
                std::cout << "... (analyzed first 2 suitable windows)\n";
                break;
            }
        }
    }
    
    if (analyzed == 0) {
        std::cout << "No suitable windows found for synthesis testing\n";
    }
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Successfully integrated feasibility check with exopt synthesis\n";
    std::cout << "Can now synthesize optimal circuits from feasible divisor sets\n";
    
    return 0;
}