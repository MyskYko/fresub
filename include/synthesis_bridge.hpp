#pragma once

#include <vector>
#include <string>
#include <cstdint>

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

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(const std::vector<uint64_t>& target_tt, 
                            const std::vector<std::vector<uint64_t>>& divisor_tts,
                            const std::vector<int>& selected_divisors,
                            int num_inputs,
                            const std::vector<int>& window_inputs,
                            const std::vector<int>& all_divisors,
                            std::vector<std::vector<bool>>& br,
                            std::vector<std::vector<bool>>& sim);