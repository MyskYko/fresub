#include "synthesis.hpp"

#include <iostream>
#include <cassert>

#include <kissat_solver.hpp>
#include <synth.hpp>

#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/topo_view.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <kitty/npn.hpp>

using namespace std;

namespace fresub {

  // Convert truth tables to exopt binary relation format
  void convert_to_exopt_format(const vector<vector<uint64_t>>& truth_tables, const vector<int>& selected_divisors, int num_inputs, vector<vector<bool>>& br) {
    // We compute target function in terms of selected divisors
    // br[divisor_pattern][target_value] = can this divisor pattern produce this target value?
    // Initialize with all true (everything is don't care initially)
    int num_selected = selected_divisors.size();
    int num_divisor_patterns = 1 << num_selected;
    int total_patterns = 1 << num_inputs;
    // Initialize br - all patterns are don't care initially
    br.clear();
    br.resize(num_divisor_patterns, vector<bool>(2, true));
    // For each input pattern, extract divisor values and target value
    for (int input_pattern = 0; input_pattern < total_patterns; input_pattern++) {
      int word_idx = input_pattern / 64;
      int bit_idx = input_pattern % 64;
      // Extract target value for this input pattern
      bool target_value = (truth_tables.back()[word_idx] >> bit_idx) & 1;
      // Extract selected divisor values for this input pattern
      int divisor_pattern = 0;
      for (int i = 0; i < num_selected; i++) {
	int divisor_idx = selected_divisors[i];
	bool divisor_value = (truth_tables[divisor_idx][word_idx] >> bit_idx) & 1;
	if (divisor_value) {
	  divisor_pattern |= (1 << i);
	}
      }
      // This divisor pattern cannot produce the opposite target value
      br[divisor_pattern][target_value ? 0 : 1] = false;
    }
  }
  
  aigman* synthesize_circuit(const vector<vector<bool>>& br, int max_gates) {
    // Create synthesis manager - pass NULL for sim since we don't use it
    SynthMan<KissatSolver> synth_man(br, nullptr);
    // Attempt synthesis
    aigman* aig = nullptr;
    for(int i = 0; !aig && i <= max_gates; i++) {
      aig = synth_man.Synth(i);
    }
    // Return synthesized AIG or nullptr if synthesis failed
    // Note: DON'T delete aig here - it's needed for insertion
    // The caller will handle cleanup
    return aig;
  }

  // Get or create static mockturtle library instance
  mockturtle::exact_library<mockturtle::aig_network, 4>& get_mockturtle_library() {
    static mockturtle::xag_npn_resynthesis<mockturtle::aig_network, mockturtle::aig_network, 
                                          mockturtle::xag_npn_db_kind::aig_complete> aig_resyn;
    static mockturtle::exact_library_params param = []() {
      mockturtle::exact_library_params p;
      p.verbose = false;
      return p;
    }();
    static mockturtle::exact_library<mockturtle::aig_network, 4> lib(aig_resyn, param);
    return lib;
  }

  // Helper function to try synthesis with a specific truth table
  aigman* try_synthesis_with_truth_table(uint16_t truth_table, int num_inputs, int max_gates) {
    // Extend truth table to 4 inputs if needed
    uint16_t extended_truth_table = truth_table;
    if (num_inputs < 4) {
      // Extend by doubling: shift and OR for each missing input
      for (int missing = num_inputs; missing < 4; missing++) {
        int shift_amount = 1 << missing; // 2^missing
        extended_truth_table |= (extended_truth_table << shift_amount);
      }
    }

    try {
      // Get the static library instance
      auto& lib = get_mockturtle_library();
      
      // Create truth table object
      using TT = kitty::static_truth_table<4>;
      TT tt;
      std::vector<uint16_t> words = {extended_truth_table};
      kitty::create_from_words(tt, words.begin(), words.end());
      
      // Perform NPN canonicalization
      auto npn_result = kitty::exact_npn_canonization(tt);
      auto canonical_tt = std::get<0>(npn_result);
      
      // Get supergates for the canonical truth table
      auto supergates = lib.get_supergates(canonical_tt);
      
      if (!supergates || supergates->empty()) {
        return nullptr;
      }
      
      // Find minimum area implementation that meets gate count constraint
      auto best_gate = supergates->end();
      for (auto it = supergates->begin(); it != supergates->end(); ++it) {
        int estimated_gates = static_cast<int>(std::ceil(it->area));
        if (estimated_gates <= max_gates) {
          if (best_gate == supergates->end() || it->area < best_gate->area) {
            best_gate = it;
          }
        }
      }
      
      if (best_gate == supergates->end()) {
        return nullptr;
      }
      
      // Build the optimal network using cleanup_dangling
      // Create a new AIG network for our result
      mockturtle::aig_network result_ntk;
      
      // Create primary inputs for our result network
      std::vector<mockturtle::aig_network::signal> pis;
      for (int i = 0; i < 4; i++) {
        pis.push_back(result_ntk.create_pi());
      }
      
      // Apply input permutation from NPN transformation
      auto perm = std::get<2>(npn_result);  // Permutation vector
      auto neg = std::get<1>(npn_result);   // Negation bitmask
      
      std::vector<mockturtle::aig_network::signal> permuted_pis(4);
      for (int i = 0; i < 4; i++) {
        // perm[i] tells us which original input corresponds to canonical input i
        int orig_input = perm[i];
        auto signal = pis[orig_input];
        
        // Apply input negation if needed
        if ((neg >> orig_input) & 1) {
          signal = result_ntk.create_not(signal);
        }
        
        permuted_pis[i] = signal;
      }
      
      // Get the database network from the library
      const auto& db = lib.get_database();
      
      // Create topo view of the database from the supergate root
      mockturtle::topo_view topo_db{db, best_gate->root};
      
      // Use cleanup_dangling to extract the logic with our permuted PIs
      auto extracted_signals = mockturtle::cleanup_dangling(topo_db, result_ntk, permuted_pis.begin(), permuted_pis.end());
      
      // Apply output polarity from NPN transformation
      auto output_signal = extracted_signals.front();
      // The output polarity is typically bit 4 (after the 4 input bits) in the negation bitmask
      bool output_negated = (neg >> 4) & 1;
      if (output_negated) {
        output_signal = result_ntk.create_not(output_signal);
      }
      
      // Create output
      result_ntk.create_po(output_signal);
      
      // Convert mockturtle AIG to aigman
      aigman* result_aig = new aigman(num_inputs, 1);
      
      // Map mockturtle nodes to aigman nodes  
      std::unordered_map<mockturtle::aig_network::node, int> node_map;
      
      // Map primary inputs (only first num_inputs in aigman)
      int pi_count = 0;
      result_ntk.foreach_pi([&](auto const& n, auto i) {
        (void)i; // Suppress unused parameter warning
        if (pi_count < num_inputs) {
          node_map[n] = pi_count + 1; // aigman PIs start at 1
          pi_count++;
        } else {
          // Unused inputs in the extended truth table - map to constant 0
          node_map[n] = 0; // This will create literal 0 (constant false)
        }
      });
      
      // Process gates in topological order
      int next_node = num_inputs + 1; // First internal node in aigman
      result_ntk.foreach_gate([&](auto const& n) {
        // Get fanins using foreach_fanin
        std::vector<int> fanin_lits;
        result_ntk.foreach_fanin(n, [&](auto const& fanin_signal, auto i) {
          (void)i; // Suppress unused parameter warning
          int fanin_node = result_ntk.get_node(fanin_signal);
          int fanin_lit = node_map[fanin_node] * 2 + (result_ntk.is_complemented(fanin_signal) ? 1 : 0);
          fanin_lits.push_back(fanin_lit);
        });
        
        // AIG gates have exactly 2 fanins
        assert(fanin_lits.size() == 2);
        
        // Add gate to aigman
        if (next_node * 2 + 1 >= static_cast<int>(result_aig->vObjs.size())) {
          result_aig->vObjs.resize((next_node + 1) * 2);
        }
        
        result_aig->vObjs[next_node * 2] = fanin_lits[0];
        result_aig->vObjs[next_node * 2 + 1] = fanin_lits[1];
        
        node_map[n] = next_node;
        next_node++;
      });
      
      // Set output
      result_ntk.foreach_po([&](auto const& s, auto i) {
        (void)i; // Suppress unused parameter warning
        auto fanin_node = result_ntk.get_node(s);
        int output_lit = node_map[fanin_node] * 2 + (result_ntk.is_complemented(s) ? 1 : 0);
        result_aig->vPos[0] = output_lit;
      });
      
      // Update aigman metadata
      result_aig->nGates = next_node - num_inputs - 1;
      result_aig->nObjs = next_node;
      
      return result_aig;
      
    } catch (const std::exception& e) {
      return nullptr;
    }
  }

  aigman* synthesize_circuit_mockturtle(const vector<vector<bool>>& br, int max_gates) {
    // Determine number of inputs from BR size using ceiling log2
    int br_size = br.size();
    int num_inputs = 0;
    while ((1 << num_inputs) < br_size) {
      num_inputs++;
    }
    
    // Identify don't care patterns and fixed patterns
    vector<bool> dont_care_patterns;
    uint16_t fixed_truth_table = 0;
    bool has_impossible = false;
    
    for (size_t pattern = 0; pattern < br.size(); pattern++) {
      if (br[pattern][1] && !br[pattern][0]) {
        // Only output 1 is allowed
        fixed_truth_table |= (1 << pattern);
        dont_care_patterns.push_back(false);
      } else if (!br[pattern][1] && br[pattern][0]) {
        // Only output 0 is allowed (nothing to set in truth table)
        dont_care_patterns.push_back(false);
      } else if (br[pattern][1] && br[pattern][0]) {
        // Both outputs allowed - don't care
        dont_care_patterns.push_back(true);
      } else {
        // Neither output allowed - impossible
        has_impossible = true;
        break;
      }
    }
    
    if (has_impossible) {
      return nullptr;
    }
    
    // Count don't care patterns
    int num_dont_cares = 0;
    for (bool dc : dont_care_patterns) {
      if (dc) num_dont_cares++;
    }
    
    
    // If no don't cares, try the fixed truth table
    if (num_dont_cares == 0) {
      return try_synthesis_with_truth_table(fixed_truth_table, num_inputs, max_gates);
    }
    
    // Exhaustive search over all don't care assignments
    aigman* best_result = nullptr;
    int best_gates = INT_MAX;
    
    // Collect indices of don't care patterns
    vector<int> dont_care_indices;
    for (size_t pattern = 0; pattern < dont_care_patterns.size(); pattern++) {
      if (dont_care_patterns[pattern]) {
        dont_care_indices.push_back(pattern);
      }
    }
    
    int num_assignments = 1 << num_dont_cares; // 2^num_dont_cares possible assignments
    
    for (int assignment = 0; assignment < num_assignments; assignment++) {
      uint16_t truth_table = fixed_truth_table;
      
      // Apply this assignment to don't care patterns
      for (int i = 0; i < num_dont_cares; i++) {
        if ((assignment >> i) & 1) {
          truth_table |= (1 << dont_care_indices[i]);
        }
      }
      
      aigman* result = try_synthesis_with_truth_table(truth_table, num_inputs, max_gates);
      if (result && result->nGates < best_gates) {
        if (best_result) delete best_result;
        best_result = result;
        best_gates = result->nGates;
      } else if (result) {
        delete result;
      }
      
      // Early termination if we found a 0-gate solution
      if (best_gates == 0) {
        break;
      }
    }
    
    
    return best_result;
  }

}
