// Synthesis bridge - isolates exopt dependencies from fresub
#include "synthesis_bridge.hpp"

// Include exopt headers FIRST to avoid conflicts
#include <aig.hpp>          // exopt's aig
#include "synth.hpp"        // exopt's synthesis
#include "kissat_solver.hpp"

using namespace std;

SynthesisResult synthesize_circuit(const vector<vector<bool>>& br, 
                                  const vector<vector<bool>>& sim,
                                  int max_gates) {
    
    SynthesisResult result;
    result.success = false;
    result.original_gates = 0;
    result.synthesized_gates = 0;
    result.internal_aigman = nullptr;
    
    try {
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
        
    } catch (const exception& e) {
        result.success = false;
        result.description = string("Synthesis error: ") + e.what();
    } catch (...) {
        result.success = false;
        result.description = "Unknown synthesis error";
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