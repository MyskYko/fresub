#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <aig.hpp>  // Direct exopt header inclusion

// Synthesis result with direct aigman pointer
struct SynthesisResult {
    bool success;
    int original_gates;
    int synthesized_gates;
    std::string description;
    
    // Direct pointer to synthesized aigman
    aigman* synthesized_aig;  // Direct aigman pointer
};

// Synthesize optimal circuit from binary relation
SynthesisResult synthesize_circuit(const std::vector<std::vector<bool>>& br, 
                                  int max_gates = 4);

// Convert truth tables to exopt binary relation format
void convert_to_exopt_format(const std::vector<uint64_t>& target_tt, 
                            const std::vector<std::vector<uint64_t>>& divisor_tts,
                            const std::vector<int>& selected_divisors,
                            int num_inputs,
                            std::vector<std::vector<bool>>& br);