#include "synthesis_bridge.hpp"
#include "synth.hpp"        // exopt's synthesis
#include "kissat_solver.hpp"
#include <set>

using namespace std;

SynthesisResult synthesize_circuit(const vector<vector<bool>>& br, 
                                  const vector<vector<bool>>& sim,
                                  int max_gates) {
    
    SynthesisResult result;
    result.success = false;
    result.original_gates = 0;
    result.synthesized_gates = 0;
    result.internal_aigman = nullptr;
    
    // Create synthesis manager
    SynthMan<KissatSolver> synth_man(br, &sim);
    
    // Attempt synthesis
    aigman* aig = synth_man.ExSynth(max_gates);
    
    if (aig) {
        result.success = true;
        result.synthesized_gates = aig->nGates;
        result.internal_aigman = static_cast<void*>(aig);  // Store aigman for insertion
        
        result.description = "Synthesized AIG with " + to_string(aig->nGates) + " gates";
        
        // Note: DON'T delete aig here - it's needed for insertion
        // The insertion engine will handle cleanup
    } else {
        result.description = "Synthesis failed - no solution found within gate limit";
    }
    
    return result;
}

// Internal function to get aigman from synthesis result
aigman* get_synthesis_aigman(const SynthesisResult& result) {
    if (!result.success || !result.internal_aigman) {
        return nullptr;
    }
    return static_cast<aigman*>(result.internal_aigman);
}

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(const vector<uint64_t>& target_tt, 
                            const vector<vector<uint64_t>>& divisor_tts,
                            const vector<int>& selected_divisors,
                            int num_inputs,
                            const vector<int>& window_inputs,
                            const vector<int>& all_divisors,
                            vector<vector<bool>>& br,
                            vector<vector<bool>>& sim) {
    
    int num_patterns = 1 << num_inputs;
    
    // Filter out divisors that are window inputs
    set<int> window_input_set(window_inputs.begin(), window_inputs.end());
    vector<int> non_input_divisor_indices;
    
    for (int idx : selected_divisors) {
        if (idx >= 0 && idx < static_cast<int>(all_divisors.size())) {
            int divisor_node = all_divisors[idx];
            if (window_input_set.find(divisor_node) == window_input_set.end()) {
                non_input_divisor_indices.push_back(idx);
            }
        }
    }
    
    // Initialize br[input_pattern][output_pattern]
    br.clear();
    br.resize(num_patterns, vector<bool>(2, false));
    
    // Initialize sim[input_pattern][non_input_divisor]
    sim.clear();
    sim.resize(num_patterns, vector<bool>(non_input_divisor_indices.size(), false));
    
    for (int p = 0; p < num_patterns; p++) {
        // Extract target value from multi-word truth table
        int word_idx = p / 64;
        int bit_idx = p % 64;
        bool target_value = false;
        if (word_idx < static_cast<int>(target_tt.size())) {
            target_value = (target_tt[word_idx] >> bit_idx) & 1;
        }
        br[p][target_value ? 1 : 0] = true;
        
        // Set non-input divisor values for this input pattern
        for (size_t d = 0; d < non_input_divisor_indices.size(); d++) {
            int divisor_idx = non_input_divisor_indices[d];
            bool divisor_value = false;
            if (divisor_idx < static_cast<int>(divisor_tts.size()) && 
                word_idx < static_cast<int>(divisor_tts[divisor_idx].size())) {
                divisor_value = (divisor_tts[divisor_idx][word_idx] >> bit_idx) & 1;
            }
            sim[p][d] = divisor_value;
        }
    }
}
