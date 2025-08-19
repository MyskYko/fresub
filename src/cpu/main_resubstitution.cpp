#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>

#include "window.hpp"
#include "simulation.hpp"
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

ResubstitutionCandidate* create_resubstitution_candidate(aigman& aig, const Window& window, bool verbose);

int main(int argc, char** argv) {
  // Read arguments
  Config config;
  for (int i = 1; i < argc; i++) {
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
  }
  if (config.input_file.empty()) {
    std::cerr << "Usage: " << argv[0] << " [options] <input.aig> [output.aig]\n";
    std::cerr << "Options:\n";
    std::cerr << "  -c <size>  Max cut size (default: 4)\n";
    std::cerr << "  -v         Verbose output\n";
    std::cerr << "  -s         Show statistics\n";
    return 1;
  }
  
  // Load input AIG
  if (config.verbose) {
    std::cout << "Loading AIG from " << config.input_file << "...\n";
  }
  aigman aig;
  aig.read(config.input_file.c_str());
  int initial_gates = aig.nGates;
  if (config.show_stats) {
    std::cout << "Initial AIG: " << aig.nPis << " PIs, " << aig.nPos << " POs, " << initial_gates << " gates\n";
  }

  // Start measurement
  auto start_time = high_resolution_clock::now();
        
  // Extract windows
  if (config.verbose) {
    std::cout << "Extracting windows with max cut size " << config.max_cut_size << "...\n";
  }
  WindowExtractor extractor(aig, config.max_cut_size, config.verbose);
  std::vector<Window> windows;
  extractor.extract_all_windows(windows);
  if (config.verbose) {
    std::cout << "Extracted " << windows.size() << " windows\n";
  }
        
  // Collect resubstitution candidates
  std::vector<ResubstitutionCandidate> candidates;
  
  for (const auto& window : windows) {
    if (window.divisors.size() < 4 || (!aig.vDeads.empty() && aig.vDeads[window.target_node])) {
      continue;
    }
    
    if (config.verbose) {
      std::cout << "Processing window with target " << window.target_node 
		<< " (" << window.inputs.size() << " inputs, " 
		<< window.divisors.size() << " divisors)\n";
    }
    
    // Step 1: Compute truth tables for feasibility check
    auto all_truth_tables = compute_truth_tables_for_window(aig, window, config.verbose);
    if (all_truth_tables.empty()) {
      std::cerr << "  Could not compute truth tables\n";
      return 1;
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
    auto feasible_combinations = find_feasible_4resub(divisor_truth_tables, target_truth_table, window.inputs.size());
    if (feasible_combinations.empty()) {
      if (config.verbose) std::cout << "  No feasible resubstitution found\n";
      continue;
    }
    
    // Use the first feasible combination
    auto selected_divisor_indices = feasible_combinations[0];
    if (config.verbose) {
      std::cout << "  ✓ Found feasible resubstitution using divisor indices: {";
      for (size_t i = 0; i < selected_divisor_indices.size(); i++) {
	if (i > 0) std::cout << ", ";
	std::cout << selected_divisor_indices[i];
      }
      std::cout << "}\n";
    }
    
    // Step 3: Synthesis
    std::vector<std::vector<bool>> br;
    convert_to_exopt_format(target_truth_table, divisor_truth_tables, selected_divisor_indices, window.inputs.size(), br);
    
    SynthesisResult synthesis = synthesize_circuit(br, 10);
    if (!synthesis.success) {
      std::cerr << "  Synthesis failed: " << synthesis.description << "\n";
      return 1;
    }
    
    if (config.verbose) {
      std::cout << "  ✓ Synthesis successful: " << synthesis.synthesized_gates << " gates\n";
    }
    
    // Create selected divisor nodes from indices
    std::vector<int> selected_divisor_nodes;
    for (int idx : selected_divisor_indices) {
      if (idx < static_cast<int>(window.divisors.size())) {
	selected_divisor_nodes.push_back(window.divisors[idx]);
      }
    }
    
    // Create and return resubstitution candidate (to be processed later)
    if (config.verbose) {
      std::cout << "  ✓ Created resubstitution candidate for target node " 
		<< window.target_node << "\n";
    }
    candidates.emplace_back(synthesis.synthesized_aig, window.target_node, selected_divisor_nodes);
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
  
}

