#pragma once

#include <vector>
#include <string>

// Forward declaration for exopt type
class aigman;

// Clean interface for exopt synthesis - no exopt headers exposed
struct SynthesisResult {
    bool success;
    int original_gates;
    int synthesized_gates;
    std::string description;
    
    // Internal: pointer to synthesized aigman (not exposed to user)
    void* internal_aigman;  // Actually aigman*, but hidden from header
};

// Synthesize optimal circuit from binary relation and simulation matrix
SynthesisResult synthesize_circuit(const std::vector<std::vector<bool>>& br, 
                                  const std::vector<std::vector<bool>>& sim,
                                  int max_gates = 4);

// Internal function to get aigman from synthesis result (used by insertion engine)
aigman* get_synthesis_aigman(const SynthesisResult& result);