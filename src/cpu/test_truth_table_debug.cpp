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

// Print truth table in binary format
void print_truth_table(uint64_t tt, int num_inputs, const std::string& label) {
    std::cout << label << ": ";
    int num_patterns = 1 << num_inputs;
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")";
}

// Compute truth table for a node within a window using bit-parallel simulation
uint64_t compute_truth_table_for_node(const AIG& aig, int node, 
                                      const std::vector<int>& window_inputs,
                                      const std::vector<int>& window_nodes,
                                      bool verbose = false) {
    
    if (verbose) {
        std::cout << "\n=== COMPUTING TRUTH TABLE FOR NODE " << node << " ===\n";
        std::cout << "Window inputs: ";
        print_node_vector(window_inputs);
        std::cout << "\n";
        std::cout << "Window nodes: ";
        print_node_vector(window_nodes);
        std::cout << "\n\n";
    }
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 6) {
        std::cout << "ERROR: Too many inputs (" << num_inputs << ") for 64-bit truth table\n";
        return 0;
    }
    
    int num_patterns = 1 << num_inputs;
    
    // Create node to truth table mapping
    std::map<int, uint64_t> node_tt;
    
    // Step 1: Initialize primary input truth tables
    if (verbose) std::cout << "STEP 1: Initialize primary input truth tables\n";
    for (int i = 0; i < num_inputs; i++) {
        int pi = window_inputs[i];
        
        // Create bit-parallel pattern for this input
        uint64_t pattern = 0;
        for (int p = 0; p < num_patterns; p++) {
            if ((p >> i) & 1) {
                pattern |= (1ULL << p);
            }
        }
        
        node_tt[pi] = pattern;
        
        if (verbose) {
            std::cout << "  PI " << pi << " (input " << i << "): ";
            print_truth_table(pattern, num_inputs, "");
            std::cout << "\n";
        }
    }
    
    // Step 2: Simulate internal nodes in topological order
    if (verbose) std::cout << "\nSTEP 2: Simulate internal nodes in topological order\n";
    
    for (int current_node : window_nodes) {
        if (current_node <= aig.num_pis) {
            continue; // Skip PIs, already handled
        }
        
        if (current_node >= static_cast<int>(aig.nodes.size()) || aig.nodes[current_node].is_dead) {
            continue;
        }
        
        // Get fanins
        uint32_t fanin0_lit = aig.nodes[current_node].fanin0;
        uint32_t fanin1_lit = aig.nodes[current_node].fanin1;
        
        int fanin0 = aig.lit2var(fanin0_lit);
        int fanin1 = aig.lit2var(fanin1_lit);
        
        bool fanin0_compl = aig.is_complemented(fanin0_lit);
        bool fanin1_compl = aig.is_complemented(fanin1_lit);
        
        if (verbose) {
            std::cout << "\n  Processing Node " << current_node << " = AND(";
            std::cout << fanin0;
            if (fanin0_compl) std::cout << "'";
            std::cout << ", " << fanin1;
            if (fanin1_compl) std::cout << "'";
            std::cout << ")\n";
        }
        
        // Check if fanins have truth tables
        if (node_tt.find(fanin0) == node_tt.end()) {
            if (verbose) std::cout << "    ERROR: Fanin0 " << fanin0 << " not simulated yet\n";
            continue;
        }
        if (node_tt.find(fanin1) == node_tt.end()) {
            if (verbose) std::cout << "    ERROR: Fanin1 " << fanin1 << " not simulated yet\n";
            continue;
        }
        
        // Get fanin truth tables
        uint64_t tt0 = node_tt[fanin0];
        uint64_t tt1 = node_tt[fanin1];
        
        // Apply complements if needed
        if (fanin0_compl) {
            tt0 = ~tt0 & ((1ULL << num_patterns) - 1);
        }
        if (fanin1_compl) {
            tt1 = ~tt1 & ((1ULL << num_patterns) - 1);
        }
        
        // Compute AND
        uint64_t result_tt = tt0 & tt1;
        node_tt[current_node] = result_tt;
        
        if (verbose) {
            std::cout << "    Fanin0 " << fanin0;
            if (fanin0_compl) std::cout << "'";
            std::cout << ": ";
            print_truth_table(fanin0_compl ? (~node_tt[fanin0] & ((1ULL << num_patterns) - 1)) : node_tt[fanin0], 
                             num_inputs, "");
            std::cout << "\n";
            
            std::cout << "    Fanin1 " << fanin1;
            if (fanin1_compl) std::cout << "'";
            std::cout << ": ";
            print_truth_table(fanin1_compl ? (~node_tt[fanin1] & ((1ULL << num_patterns) - 1)) : node_tt[fanin1], 
                             num_inputs, "");
            std::cout << "\n";
            
            std::cout << "    Result " << current_node << ": ";
            print_truth_table(result_tt, num_inputs, "");
            std::cout << "\n";
        }
    }
    
    // Return truth table for the requested node
    if (node_tt.find(node) != node_tt.end()) {
        if (verbose) {
            std::cout << "\nFinal truth table for node " << node << ": ";
            print_truth_table(node_tt[node], num_inputs, "");
            std::cout << "\n";
        }
        return node_tt[node];
    } else {
        if (verbose) std::cout << "\nERROR: Node " << node << " not found in simulation results\n";
        return 0;
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
    std::cout << "Max cut size: " << max_cut_size << "\n\n";
    
    // Extract windows
    std::cout << "=== EXTRACTING WINDOWS ===\n";
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Total windows: " << windows.size() << "\n\n";
    
    // Test truth table computation on first few windows
    std::cout << "=== TRUTH TABLE COMPUTATION TESTING ===\n\n";
    
    int windows_tested = 0;
    for (const auto& window : windows) {
        if (windows_tested >= 3) { // Limit to 3 windows for debugging
            std::cout << "... (limiting to 3 windows for debugging)\n";
            break;
        }
        
        // Skip windows with too many inputs
        if (window.inputs.size() > 4) {
            continue;
        }
        
        std::cout << std::string(60, '=') << "\n";
        std::cout << "WINDOW " << windows_tested << " - TARGET " << window.target_node << "\n";
        std::cout << std::string(60, '=') << "\n";
        
        std::cout << "Cut: ";
        Cut cut;
        cut.leaves = window.inputs;
        print_cut(cut, aig);
        std::cout << " (" << window.inputs.size() << " inputs)\n";
        
        std::cout << "Window nodes: ";
        print_node_vector(window.nodes);
        std::cout << " (" << window.nodes.size() << " nodes)\n";
        
        std::cout << "Divisors: ";
        print_node_vector(window.divisors);
        std::cout << " (" << window.divisors.size() << " divisors)\n\n";
        
        // Compute truth table for target
        std::cout << "COMPUTING TRUTH TABLE FOR TARGET " << window.target_node << ":\n";
        uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, 
                                                         window.inputs, window.nodes, true);
        
        std::cout << "\n" << std::string(40, '-') << "\n";
        std::cout << "COMPUTING TRUTH TABLES FOR DIVISORS:\n";
        
        // Compute truth tables for first few divisors
        int divisors_shown = 0;
        for (int divisor : window.divisors) {
            if (divisors_shown >= 3) { // Limit divisors for readability
                std::cout << "... (showing first 3 divisors only)\n";
                break;
            }
            
            std::cout << "\nDivisor " << divisor << ":\n";
            uint64_t divisor_tt = compute_truth_table_for_node(aig, divisor, 
                                                              window.inputs, window.nodes, false);
            
            std::cout << "  ";
            print_truth_table(divisor_tt, window.inputs.size(), "");
            std::cout << "\n";
            
            // Check if this divisor can implement the target
            if (divisor_tt == target_tt) {
                std::cout << "  *** EXACT MATCH! Target can be replaced by divisor " << divisor << " ***\n";
            }
            
            divisors_shown++;
        }
        
        std::cout << "\n" << std::string(40, '-') << "\n";
        std::cout << "SUMMARY FOR WINDOW " << windows_tested << ":\n";
        std::cout << "  Target " << window.target_node << ": ";
        print_truth_table(target_tt, window.inputs.size(), "");
        std::cout << "\n";
        std::cout << "  Function has " << __builtin_popcountll(target_tt) << " ones out of " 
                 << (1 << window.inputs.size()) << " patterns\n";
        std::cout << "  Available for resubstitution: " << window.divisors.size() << " divisors\n";
        
        windows_tested++;
        std::cout << "\n\n";
    }
    
    std::cout << "=== TRUTH TABLE COMPUTATION INSIGHTS ===\n";
    std::cout << "1. Primary inputs get standard bit-parallel patterns\n";
    std::cout << "2. Internal nodes simulated in topological order using AND operations\n";
    std::cout << "3. Complemented inputs handled by bitwise NOT\n";
    std::cout << "4. Truth tables enable exact function matching for resubstitution\n";
    std::cout << "5. Bit-parallel simulation is efficient for up to 6 inputs (64-bit)\n";
    
    return 0;
}