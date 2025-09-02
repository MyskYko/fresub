#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <iostream>

#include <aig.hpp>

#include "feasibility.hpp"
#include "insertion.hpp"
#include "simulation.hpp"
#include "synthesis.hpp"
#include "window.hpp"

// Forward declare CUDA functions to avoid requiring CUDA headers when not needed
namespace fresub {
  void feasibility_check_cuda(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);
  void feasibility_check_cuda_all(std::vector<Window>::iterator begin, std::vector<Window>::iterator end);
}

using namespace fresub;
using namespace std::chrono;

struct Config {
    std::string input_file;
    std::string output_file;
    int max_cut_size = 4;
    bool verbose = false;
    bool show_stats = false;
    bool use_mockturtle = true;  // Default to mockturtle synthesis
    bool use_cuda = false;       // Default to CPU feasibility check
    bool use_cuda_all = false;   // Use CUDA to find all combinations
    bool feas_all = false;       // CPU feasibility: if true ALL, else MIN-SIZE
};


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
    } else if (strcmp(argv[i], "--exopt") == 0) {
      config.use_mockturtle = false;
    } else if (strcmp(argv[i], "--mockturtle") == 0) {
      config.use_mockturtle = true;
    } else if (strcmp(argv[i], "--cuda") == 0) {
      config.use_cuda = true;
    } else if (strcmp(argv[i], "--cuda-all") == 0) {
      config.use_cuda_all = true;
    } else if (strcmp(argv[i], "--feas-all") == 0) {
      config.feas_all = true;
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
    std::cerr << "  -c <size>     Max cut size (default: 4)\n";
    std::cerr << "  -v            Verbose output\n";
    std::cerr << "  -s            Show statistics\n";
    std::cerr << "  --exopt       Use SAT-based synthesis (exopt)\n";
    std::cerr << "  --mockturtle  Use library-based synthesis (mockturtle, default)\n";
    std::cerr << "  --cuda        Use CUDA for feasibility checking (first solution)\n";
    std::cerr << "  --cuda-all    Use CUDA for feasibility checking (all solutions)\n";
    std::cerr << "  --feas-all    CPU feasibility: ALL mode (default is MIN-SIZE)\n";
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
  if (config.verbose) {
    std::cout << "Using " << (config.use_mockturtle ? "mockturtle library-based" : "exopt SAT-based") << " synthesis\n";
    if (config.use_cuda_all) {
      std::cout << "Using CUDA feasibility checking (all combinations)\n";
    } else if (config.use_cuda) {
      std::cout << "Using CUDA feasibility checking (first combination)\n";
    } else if (config.feas_all) {
      std::cout << "Using CPU feasibility (ALL mode)\n";
    } else {
      std::cout << "Using CPU feasibility (MIN-SIZE mode)\n";
    }
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

  // Previously: excluded windows with <4 divisors. Now process all windows.
  
  // Compute truth tables
  for (auto& window : windows) {
    window.truth_tables = compute_truth_tables_for_window(aig, window, config.verbose);
  }

  // Feasibility check
  if (config.use_cuda_all) {
    feasibility_check_cuda_all(windows.begin(), windows.end());
  } else if (config.use_cuda) {
    feasibility_check_cuda(windows.begin(), windows.end());
  } else if (config.feas_all) {
    feasibility_check_cpu_all(windows.begin(), windows.end());
  } else {
    feasibility_check_cpu_min(windows.begin(), windows.end());
  }
  
  // Synthesis per feasible-set and best-of-feasible selection per window
  std::vector<Result> results;
  for (auto& window : windows) {
    if (config.verbose) {
      std::cout << "Processing window with target " << window.target_node
		<< " (" << window.inputs.size() << " inputs, "
		<< window.divisors.size() << " divisors)\n";
    }    
    if (window.feasible_sets.empty()) {
      if (config.verbose) std::cout << "  No feasible resubstitution found\n";
      continue;
    }
    if (config.verbose) {
      std::cout << "  ✓ Found " << window.feasible_sets.size() << " feasible resubstitution(s):\n";
      for (size_t combo_idx = 0; combo_idx < window.feasible_sets.size(); combo_idx++) {
        std::cout << "    [" << combo_idx << "]: {";
        for (size_t i = 0; i < window.feasible_sets[combo_idx].divisor_indices.size(); i++) {
          if (i > 0) std::cout << ", ";
          std::cout << window.feasible_sets[combo_idx].divisor_indices[i];
        }
        std::cout << "}\n";
      }
    }
    // 1) For each feasible set, synthesize one circuit and store in FeasibleSet::synths
    for (auto& fs : window.feasible_sets) {
      // Build binary relation for this feasible set
      std::vector<std::vector<bool>> br;
      generate_relation(window.truth_tables, fs.divisor_indices, window.inputs.size(), br);

      // Try selected synthesis engine with gate budget = mffc_size - 1
      aigman* synthesized_aig = nullptr;
      if (config.use_mockturtle) {
        synthesized_aig = synthesize_circuit_mockturtle(br, window.mffc_size - 1);
      } else {
        synthesized_aig = synthesize_circuit(br, window.mffc_size - 1);
      }
      if (synthesized_aig) {
        fs.synths.push_back(synthesized_aig);
        if (config.verbose) {
          std::cout << "  ✓ Synthesized set {";
          for (size_t i = 0; i < fs.divisor_indices.size(); i++) {
            if (i) std::cout << ", ";
            std::cout << fs.divisor_indices[i];
          }
          std::cout << "}: " << synthesized_aig->nGates << " gates\n";
        }
      } else if (config.verbose) {
        std::cout << "  ✗ Synthesis failed for set {";
        for (size_t i = 0; i < fs.divisor_indices.size(); i++) {
          if (i) std::cout << ", ";
          std::cout << fs.divisor_indices[i];
        }
        std::cout << "} within gate limit" << "\n";
      }
    }

    // 2) Select best-of-feasible candidate per window (smallest gates)
    int best_fs_idx = -1;
    int best_synth_idx = -1;
    int best_gates = std::numeric_limits<int>::max();
    for (size_t fi = 0; fi < window.feasible_sets.size(); ++fi) {
      auto& fs = window.feasible_sets[fi];
      for (size_t si = 0; si < fs.synths.size(); ++si) {
        if (fs.synths[si] && fs.synths[si]->nGates < best_gates) {
          best_gates = fs.synths[si]->nGates;
          best_fs_idx = static_cast<int>(fi);
          best_synth_idx = static_cast<int>(si);
        }
      }
    }

    if (best_fs_idx < 0) {
      if (config.verbose) std::cout << "  No synthesized candidates met the gate budget\n";
      // Clean up any failed synths (should be none), then continue
      continue;
    }

    // 3) Create result for the best candidate and free the rest to avoid leaks
    auto& best_fs = window.feasible_sets[best_fs_idx];
    aigman* chosen = best_fs.synths[best_synth_idx];
    if (config.verbose) {
      std::cout << "  → Chosen set {";
      for (size_t i = 0; i < best_fs.divisor_indices.size(); i++) {
        if (i) std::cout << ", ";
        std::cout << best_fs.divisor_indices[i];
      }
      std::cout << "} with " << chosen->nGates << " gates\n";
    }

    // Build selected_divisor_nodes from indices → node IDs
    std::vector<int> selected_divisor_nodes;
    selected_divisor_nodes.reserve(best_fs.divisor_indices.size());
    for (int idx : best_fs.divisor_indices) {
      selected_divisor_nodes.push_back(window.divisors[idx]);
    }
    results.emplace_back(chosen, window.target_node, selected_divisor_nodes);

    // Delete all other synthesized AIGs
    for (size_t fi = 0; fi < window.feasible_sets.size(); ++fi) {
      auto& fs = window.feasible_sets[fi];
      for (size_t si = 0; si < fs.synths.size(); ++si) {
        if (fi == static_cast<size_t>(best_fs_idx) && si == static_cast<size_t>(best_synth_idx)) continue;
        if (fs.synths[si]) {
          delete fs.synths[si];
          fs.synths[si] = nullptr;
        }
      }
      // Optionally clear to free memory; keep indices/nodes for logging/debug
      fs.synths.clear();
    }
  }
  
  // Process results sequentially to avoid conflicts
  if (config.verbose) {
    std::cout << "\nCollected " << results.size() << " resubstitution results\n";
    std::cout << "Processing results sequentially...\n";
  }
  Inserter inserter(aig);
  auto applied = inserter.process_candidates_sequentially(results, config.verbose);
  
  // Final statistics
  auto end_time = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end_time - start_time);
  int final_gates = aig.nGates;
  int successful_resubs = 0;
  for (bool success : applied) {
    if (success) successful_resubs++;
  }
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
