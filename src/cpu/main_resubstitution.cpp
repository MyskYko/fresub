#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>

#include "window.hpp"
#include "feasibility.hpp"
#include "synthesis_bridge.hpp"
#include "conflict.hpp"
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

// Create resubstitution candidate from a window (without applying it)
ResubstitutionCandidate* create_resubstitution_candidate(aigman& aig, const Window& window, bool verbose) {
    if (verbose) {
        std::cout << "Processing window with target " << window.target_node 
                  << " (" << window.inputs.size() << " inputs, " 
                  << window.divisors.size() << " divisors)\n";
    }
    
    // Step 1: Compute truth tables for feasibility check
    WindowExtractor extractor(aig, 4);
    auto all_truth_tables = extractor.compute_truth_tables_for_window(
        window.target_node, window.inputs, window.nodes, window.divisors);
    
    if (all_truth_tables.empty()) {
        if (verbose) std::cout << "  Could not compute truth tables\n";
        return nullptr;
    }
    
    // Extract divisor truth tables and target truth table
    std::vector<std::vector<uint64_t>> divisor_truth_tables;
    for (size_t i = 0; i < window.divisors.size(); i++) {
        if (i < all_truth_tables.size()) {
            divisor_truth_tables.push_back(all_truth_tables[i]);
        } else {
            divisor_truth_tables.push_back({0});
        }
    }
    
    std::vector<uint64_t> target_truth_table;
    if (!all_truth_tables.empty()) {
        target_truth_table = all_truth_tables.back();
    } else {
        target_truth_table = {0};
    }
    
    // Step 2: Feasibility check
    auto feasible_combinations = find_feasible_4resub(
        divisor_truth_tables, target_truth_table, window.inputs.size());
    
    if (feasible_combinations.empty()) {
        if (verbose) std::cout << "  No feasible resubstitution found\n";
        return nullptr;
    }
    
    // Use the first feasible combination
    auto selected_divisor_indices = feasible_combinations[0];
    
    if (verbose) {
        std::cout << "  ✓ Found feasible resubstitution using divisor indices: {";
        for (size_t i = 0; i < selected_divisor_indices.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << selected_divisor_indices[i];
        }
        std::cout << "}\n";
    }
    
    // Step 3: Synthesis
    std::vector<std::vector<bool>> br;
    convert_to_exopt_format(target_truth_table, divisor_truth_tables, 
                           selected_divisor_indices, window.inputs.size(), br);
    
    SynthesisResult synthesis = synthesize_circuit(br, 4);
    if (!synthesis.success) {
        if (verbose) std::cout << "  Synthesis failed: " << synthesis.description << "\n";
        return nullptr;
    }
    
    if (verbose) {
        std::cout << "  ✓ Synthesis successful: " << synthesis.synthesized_gates << " gates\n";
    }
    
    // Step 4: Create resubstitution candidate (don't apply yet)
    aigman* synthesized_aigman = synthesis.synthesized_aig;
    if (!synthesized_aigman) {
        if (verbose) std::cout << "  Could not retrieve synthesized circuit\n";
        return nullptr;
    }
    
    // Create selected divisor nodes from indices
    std::vector<int> selected_divisor_nodes;
    for (int idx : selected_divisor_indices) {
        if (idx < static_cast<int>(window.divisors.size())) {
            selected_divisor_nodes.push_back(window.divisors[idx]);
        }
    }
    
    // Create and return resubstitution candidate (to be processed later)
    ResubstitutionCandidate* candidate = new ResubstitutionCandidate(synthesized_aigman, window.target_node, selected_divisor_nodes);
    
    if (verbose) {
        std::cout << "  ✓ Created resubstitution candidate for target node " 
                  << window.target_node << "\n";
    }
    
    return candidate;
}

int main(int argc, char** argv) {
    try {
        Config config = parse_args(argc, argv);
        
        // Load input AIG
        if (config.verbose) {
            std::cout << "Loading AIG from " << config.input_file << "...\n";
        }
        
        aigman aig;
        aig.read(config.input_file.c_str());
        
        // Initial statistics
        int initial_gates = aig.nGates;
        
        if (config.show_stats) {
            std::cout << "Initial AIG: " << aig.nPis << " PIs, " << aig.nPos << " POs, " 
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
        
        // Collect resubstitution candidates
        std::vector<ResubstitutionCandidate> candidates;
        int attempted_resubs = 0;
        
        for (const auto& window : windows) {
            // Only process windows suitable for 4-input resubstitution
            if (window.inputs.size() >= 3 && window.inputs.size() <= 4 && 
                window.divisors.size() >= 4 && 
                window.target_node < aig.nObjs && 
                (aig.vDeads.empty() || !aig.vDeads[window.target_node])) {
                
                attempted_resubs++;
                ResubstitutionCandidate* candidate = create_resubstitution_candidate(aig, window, config.verbose);
                if (candidate) {
                    candidates.push_back(*candidate);
                    delete candidate; // Transfer ownership to vector
                }
            }
        }
        
        if (config.verbose) {
            std::cout << "\nCollected " << candidates.size() << " resubstitution candidates\n";
            std::cout << "Processing candidates sequentially...\n";
        }
        
        // Process candidates sequentially to avoid conflicts
        ConflictResolver resolver(aig);
        auto results = resolver.process_candidates_sequentially(candidates, config.verbose);
        
        // Count successful applications
        int successful_resubs = 0;
        for (bool success : results) {
            if (success) successful_resubs++;
        }
        
        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end_time - start_time);
        
        // Final statistics
        int final_gates = aig.nGates;
        
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
            aig.write(config.output_file.c_str());
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
