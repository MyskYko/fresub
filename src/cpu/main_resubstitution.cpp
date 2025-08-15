#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>

#include "fresub_aig.hpp"
#include "window.hpp"
#include "feasibility.hpp"
#include "synthesis_bridge.hpp"
#include "aig_insertion.hpp"
#include <aig.hpp>  // For complete aigman type

using namespace fresub;
using namespace std::chrono;

struct Config {
    std::string input_file;
    std::string output_file;
    int max_cut_size = 4;
    bool verbose = false;
    bool show_stats = false;
};

Config parse_args(int argc, char** argv) {
    Config config;
    
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            config.show_stats = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config.max_cut_size = std::atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            if (config.input_file.empty()) {
                config.input_file = argv[i];
            } else if (config.output_file.empty()) {
                config.output_file = argv[i];
            }
        }
        i++;
    }
    
    if (config.input_file.empty()) {
        std::cerr << "Usage: " << argv[0] << " [options] <input.aig> [output.aig]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -c <size>  Max cut size (default: 4)\n";
        std::cerr << "  -v         Verbose output\n";
        std::cerr << "  -s         Show statistics\n";
        exit(1);
    }
    
    return config;
}

// Execute complete resubstitution pipeline on a single window
bool resubstitute_window(AIG& aig, const Window& window, bool verbose) {
    if (verbose) {
        std::cout << "Processing window with target " << window.target_node 
                  << " (" << window.inputs.size() << " inputs, " 
                  << window.divisors.size() << " divisors)\n";
    }
    
    // Step 1: Feasibility check
    FeasibilityResult feasible = find_feasible_4resub(aig, window);
    if (!feasible.found) {
        if (verbose) std::cout << "  No feasible resubstitution found\n";
        return false;
    }
    
    if (verbose) {
        std::cout << "  ✓ Found feasible resubstitution using divisors: {";
        for (size_t i = 0; i < feasible.divisor_nodes.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << feasible.divisor_nodes[i];
        }
        std::cout << "}\n";
    }
    
    // Step 2: Exact synthesis - compute all truth tables efficiently at once
    auto all_truth_tables = aig.compute_truth_tables_for_window(
        window.target_node, window.inputs, window.nodes, window.divisors);
    
    // Extract individual truth tables for compatibility with existing code
    std::vector<uint64_t> divisor_tts;
    for (size_t i = 0; i < window.divisors.size(); i++) {
        if (i < all_truth_tables.size() && !all_truth_tables[i].empty()) {
            divisor_tts.push_back(all_truth_tables[i][0]); // First 64 bits
        } else {
            divisor_tts.push_back(0);
        }
    }
    
    // Target truth table is at the end
    uint64_t target_tt = 0;
    if (!all_truth_tables.empty() && !all_truth_tables.back().empty()) {
        target_tt = all_truth_tables.back()[0]; // First 64 bits
    }
    
    std::vector<std::vector<bool>> br, sim;
    convert_to_exopt_format(target_tt, divisor_tts, feasible.divisor_indices, 
                           window.inputs.size(), window.inputs, window.divisors, br, sim);
    
    SynthesisResult synthesis = synthesize_circuit(br, sim, 4);
    if (!synthesis.success) {
        if (verbose) std::cout << "  Synthesis failed: " << synthesis.description << "\n";
        return false;
    }
    
    if (verbose) {
        std::cout << "  ✓ Synthesis successful: " << synthesis.synthesized_gates << " gates\n";
    }
    
    // Step 3: AIG insertion
    AIGInsertion inserter(aig);
    aigman* synthesized_aigman = get_synthesis_aigman(synthesis);
    if (!synthesized_aigman) {
        if (verbose) std::cout << "  Could not retrieve synthesized circuit\n";
        return false;
    }
    
    InsertionResult insertion = inserter.insert_synthesized_circuit(
        window, feasible.divisor_indices, synthesized_aigman);
    
    if (!insertion.success) {
        if (verbose) std::cout << "  Circuit insertion failed\n";
        delete synthesized_aigman;
        return false;
    }
    
    // Step 4: Target replacement
    bool replacement_success = inserter.replace_target_with_circuit(
        window.target_node, insertion.new_output_node);
    
    delete synthesized_aigman;
    
    if (replacement_success && verbose) {
        std::cout << "  ✓ Successfully replaced target node " << window.target_node 
                  << " with " << insertion.new_nodes.size() << " new nodes\n";
    }
    
    return replacement_success;
}

int main(int argc, char** argv) {
    try {
        Config config = parse_args(argc, argv);
        
        // Load input AIG
        if (config.verbose) {
            std::cout << "Loading AIG from " << config.input_file << "...\n";
        }
        
        AIG aig;
        aig.read_aiger(config.input_file);
        
        // Initial statistics
        int initial_gates = 0;
        for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
            if (!aig.nodes[i].is_dead) initial_gates++;
        }
        
        if (config.show_stats) {
            std::cout << "Initial AIG: " << aig.num_pis << " PIs, " << aig.num_pos << " POs, " 
                      << initial_gates << " gates\n";
        }
        
        // Extract windows
        if (config.verbose) {
            std::cout << "Extracting windows with max cut size " << config.max_cut_size << "...\n";
        }
        
        auto start_time = high_resolution_clock::now();
        
        WindowExtractor extractor(aig, config.max_cut_size);
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        
        if (config.verbose) {
            std::cout << "Extracted " << windows.size() << " windows\n";
        }
        
        // Perform resubstitution
        int successful_resubs = 0;
        int attempted_resubs = 0;
        
        for (const auto& window : windows) {
            // Only process windows suitable for 4-input resubstitution
            if (window.inputs.size() >= 3 && window.inputs.size() <= 4 && 
                window.divisors.size() >= 4 && 
                window.target_node < aig.nodes.size() && 
                !aig.nodes[window.target_node].is_dead) {
                
                attempted_resubs++;
                if (resubstitute_window(aig, window, config.verbose)) {
                    successful_resubs++;
                }
            }
        }
        
        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end_time - start_time);
        
        // Final statistics
        int final_gates = 0;
        for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
            if (!aig.nodes[i].is_dead) final_gates++;
        }
        
        if (config.show_stats || config.verbose) {
            std::cout << "\nResubstitution complete:\n";
            std::cout << "  Windows extracted: " << windows.size() << "\n";
            std::cout << "  Windows attempted: " << attempted_resubs << "\n";
            std::cout << "  Successful resubstitutions: " << successful_resubs << "\n";
            std::cout << "  Time: " << duration.count() << " ms\n";
            std::cout << "  Initial gates: " << initial_gates << "\n";
            std::cout << "  Final gates: " << final_gates << "\n";
            
            int gate_change = final_gates - initial_gates;
            if (gate_change > 0) {
                std::cout << "  Gate change: +" << gate_change << " gates added\n";
            } else if (gate_change < 0) {
                std::cout << "  Gate change: " << (-gate_change) << " gates saved\n";
            } else {
                std::cout << "  Gate change: no change\n";
            }
        }
        
        // Write output if specified
        if (!config.output_file.empty()) {
            if (config.verbose) {
                std::cout << "Writing optimized AIG to " << config.output_file << "...\n";
            }
            aig.write_aiger(config.output_file, true);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}