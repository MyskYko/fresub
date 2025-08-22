#include "synthesis.hpp"
#include "synth.hpp"        // exopt's synthesis
#include "kissat_solver.hpp"
#include <set>

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
    for(int i = 0; !aig && i < max_gates; i++) {
      aig = synth_man.Synth(i);
    }
    // Return synthesized AIG or nullptr if synthesis failed
    // Note: DON'T delete aig here - it's needed for insertion
    // The caller will handle cleanup
    return aig;
  }

}
