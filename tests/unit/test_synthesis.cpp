#include "synthesis_bridge.hpp"
#include "feasibility.hpp"
#include <iostream>
#include <vector>
#include <cassert>

int total_tests = 0;
int passed_tests = 0;

#define ASSERT(cond) do { \
    total_tests++; \
    if (!(cond)) { \
        std::cerr << "Test failed: " << #cond << std::endl; \
    } else { \
        passed_tests++; \
    } \
} while(0)

// ============================================================================
// Helper function to create synthesis input matrices
// ============================================================================

struct SynthesisInput {
    std::vector<std::vector<bool>> br;
};

// Create synthesis input for a 2-input function
SynthesisInput create_2input_function(const std::string& truth_table_bits) {
    SynthesisInput input;
    input.br.resize(4, std::vector<bool>(2));
    
    // Set up binary relation based on truth table
    for (int p = 0; p < 4; p++) {
        bool output = (truth_table_bits[3-p] == '1'); // MSB first
        input.br[p][output ? 1 : 0] = true;
    }
    
    return input;
}

// ============================================================================
// TEST 1: Basic Logic Functions
// ============================================================================

void test_basic_logic_functions() {
    std::cout << "=== TESTING BASIC LOGIC FUNCTIONS ===\n";
    
    struct TestCase {
        std::string name;
        std::string truth_table;
        int expected_gates;
    };
    
    std::vector<TestCase> test_cases = {
        {"AND", "1000", 1},
        {"OR",  "1110", 1}, 
        {"XOR", "0110", 3},
        {"NAND","0111", 1}
    };
    
    for (const auto& test : test_cases) {
        std::cout << "\n  Testing " << test.name << " function (TT: " << test.truth_table << ")\n";
        
        auto input = create_2input_function(test.truth_table);
        
        try {
            SynthesisResult result = synthesize_circuit(input.br, {}, 10);
            std::cout << "    Result: " << (result.success ? "SUCCESS" : "FAILED");
            
            if (result.success) {
                std::cout << " (" << result.synthesized_gates << " gates)";
                if (test.expected_gates > 0) {
                    ASSERT(result.synthesized_gates <= test.expected_gates + 1); // Allow some tolerance
                }
            }
            std::cout << "\n";
            ASSERT(result.success);
            
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    std::cout << "\n  ✓ Basic logic functions completed\n\n";
}

// ============================================================================
// TEST 2: Constant Functions
// ============================================================================

void test_constant_functions() {
    std::cout << "=== TESTING CONSTANT FUNCTIONS ===\n";
    
    // Test constant 0
    {
        std::cout << "\n  Testing constant 0\n";
        std::vector<std::vector<bool>> br(2, std::vector<bool>(2));
        
        br[0][0] = true;  br[0][1] = false;  // Input 0 -> Output 0
        br[1][0] = true;  br[1][1] = false;  // Input 1 -> Output 0
        
        try {
            SynthesisResult result = synthesize_circuit(br, {}, 10);
            std::cout << "    Result: " << (result.success ? "SUCCESS" : "FAILED");
            if (result.success) {
                std::cout << " (" << result.synthesized_gates << " gates)";
            }
            std::cout << "\n";
            ASSERT(result.success);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    // Test constant 1
    {
        std::cout << "\n  Testing constant 1\n";
        std::vector<std::vector<bool>> br(2, std::vector<bool>(2));
        
        br[0][1] = true;  // Input 0 -> Output 1
        br[1][1] = true;  // Input 1 -> Output 1
        
        try {
            SynthesisResult result = synthesize_circuit(br, {}, 10);
            std::cout << "    Result: " << (result.success ? "SUCCESS" : "FAILED");
            if (result.success) {
                std::cout << " (" << result.synthesized_gates << " gates)";
            }
            std::cout << "\n";
            ASSERT(result.success);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    std::cout << "\n  ✓ Constant functions completed\n\n";
}

// ============================================================================
// TEST 3: Multi-Input Functions
// ============================================================================

void test_multi_input_functions() {
    std::cout << "=== TESTING MULTI-INPUT FUNCTIONS ===\n";
    
    // Test 4-input AND function
    {
        std::cout << "\n  Testing 4-input AND function\n";
        
        int num_patterns = 16;
        std::vector<std::vector<bool>> br(num_patterns, std::vector<bool>(2));
        
        for (int p = 0; p < num_patterns; p++) {
            bool output = (p == 15); // Only true when all inputs are 1
            br[p][output ? 1 : 0] = true;
        }
        
        try {
            SynthesisResult result = synthesize_circuit(br, {}, 10);
            std::cout << "    Result: " << (result.success ? "SUCCESS" : "FAILED");
            if (result.success) {
                std::cout << " (" << result.synthesized_gates << " gates)";
                ASSERT(result.synthesized_gates == 3); // 4-input AND needs exactly 3 gates
            }
            std::cout << "\n";
            ASSERT(result.success);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    std::cout << "\n  ✓ Multi-input functions completed\n\n";
}

// ============================================================================
// TEST 4: Error Conditions
// ============================================================================

void test_error_conditions() {
    std::cout << "=== TESTING ERROR CONDITIONS ===\n";
    
    // Test impossible synthesis (gate limit too low)
    {
        std::cout << "\n  Testing impossible synthesis (XOR with 1 gate limit)\n";
        
        auto input = create_2input_function("0110"); // XOR needs 3+ gates
        
        try {
            SynthesisResult result = synthesize_circuit(input.br, {}, 1); // Only 1 gate
            std::cout << "    Result: " << (result.success ? "SUCCESS" : "FAILED") << "\n";
            ASSERT(!result.success); // Should fail due to gate limit
        } catch (const std::exception& e) {
            std::cout << "    Expected failure: " << e.what() << "\n";
            ASSERT(true); // Exception is acceptable
        }
    }
    
    std::cout << "\n  ✓ Error condition testing completed\n\n";
}

// ============================================================================
// TEST 5: Conversion Function
// ============================================================================

void test_conversion_function() {
    std::cout << "=== TESTING TRUTH TABLE CONVERSION ===\n";
    
    // Test 1: Simple AND function conversion
    {
        std::cout << "\n  Testing AND function conversion\n";
        
        std::vector<uint64_t> target_tt = {0x8}; // AND: 1000 (single word)
        std::vector<std::vector<uint64_t>> divisor_tts = {{0x3}, {0x5}}; // Inputs: 0011, 0101 (single word each)
        std::vector<int> selected_divisors = {0, 1};
        int num_inputs = 2;
        std::vector<int> window_inputs = {1, 2};
        std::vector<int> all_divisors = {1, 2};
        
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        ASSERT(br.size() == 4);
        ASSERT(br[0].size() == 2);
        
        std::cout << "    ✓ Conversion successful (" << br.size() << " patterns)\n";
    }
    
    // Test 2: Function with internal divisors
    {
        std::cout << "\n  Testing function with internal divisors\n";
        
        std::vector<uint64_t> target_tt = {0xF}; // All ones (single word)
        std::vector<std::vector<uint64_t>> divisor_tts = {{0x3}, {0x5}, {0x1}, {0x7}}; // Single word each
        std::vector<int> selected_divisors = {0, 1, 2, 3};
        int num_inputs = 2;
        
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        ASSERT(br.size() == 16);
        
        std::cout << "    ✓ Conversion successful (" << " internal divisors)\n";
    }
    
    // Test 3: Multi-word truth table (7 inputs, 2 words)
    {
        std::cout << "\n  Testing multi-word truth table (7 inputs)\n";
        
        int num_inputs = 7;
        std::vector<uint64_t> target_tt = {0x5555555555555555ULL, 0x3333333333333333ULL}; // 2 words
        std::vector<std::vector<uint64_t>> divisor_tts = {
            {0x3333333333333333ULL, 0x5555555555555555ULL}, // Divisor 0: 2 words
            {0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL}, // Divisor 1: 2 words  
            {0x00000000FFFFFFFFULL, 0x0000FFFF0000FFFFULL}  // Divisor 2: 2 words
        };
        std::vector<int> selected_divisors = {0, 1, 2};
        
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        ASSERT(br.size() == 8); // 2^7 patterns
        ASSERT(br[0].size() == 2);
        
        std::cout << "    ✓ Multi-word conversion successful (" << br.size() << " patterns)\n";
    }
    
    std::cout << "\n  ✓ Conversion testing completed\n\n";
}

// ============================================================================
// TEST 6: End-to-End Pipeline (Convert → Synthesis)
// ============================================================================

void test_end_to_end_pipeline() {
    std::cout << "=== TESTING END-TO-END PIPELINE ===\n";
    std::cout << "Testing truth tables → convert → synthesis pipeline...\n";
    
    // Test Case 1: Simple 2-input AND with manual truth tables
    {
        std::cout << "\n  Test 1: 2-input AND function pipeline\n";
        
        // Manual truth tables for 2-input AND
        std::vector<uint64_t> target_tt = {0x8}; // AND: 1000
        std::vector<std::vector<uint64_t>> divisor_tts = {
            {0xC}, // Input A: 1100  
            {0xA}  // Input B: 1010
        };
        std::vector<int> selected_divisors = {0, 1};
        int num_inputs = 2;
        
        // Step 1: Convert truth tables to synthesis format
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        // Step 2: Synthesize circuit
        SynthesisResult result = synthesize_circuit(br, {}, 10);
        
        std::cout << "    Step 2: " << (result.success ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED");
        if (result.success) {
            std::cout << " (" << result.synthesized_gates << " gates)";
            ASSERT(result.synthesized_gates == 1); // AND should be 1 gate
        }
        std::cout << "\n";
        ASSERT(result.success);
    }
    
    // Test Case 2: 3-input function with internal divisors
    {
        std::cout << "\n  Test 2: 3-input function with internal divisors\n";
        
        // Manual truth tables for 3-input case
        std::vector<uint64_t> target_tt = {0xFE}; // Complex 3-input function: 11111110
        std::vector<std::vector<uint64_t>> divisor_tts = {
            {0xF0}, // Divisor 0: 11110000
            {0xCC}, // Divisor 1: 11001100  
            {0xAA}, // Divisor 2: 10101010
            {0x80}  // Divisor 3: 10000000 (internal, not an input)
        };
        std::vector<int> selected_divisors = {0, 1, 2, 3};
        int num_inputs = 3;
        
        // Step 1: Convert 
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        ASSERT(br.size() == 16); // 2^4 patterns
        
        // Step 2: Synthesize
        SynthesisResult result = synthesize_circuit(br, {}, 10);
        
        std::cout << "    Step 2: " << (result.success ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED");
        if (result.success) {
            std::cout << " (" << result.synthesized_gates << " gates)";
        }
        std::cout << "\n";
        ASSERT(result.success);
    }
    
    // Test Case 3: Multi-word truth table pipeline (7 inputs)
    {
        std::cout << "\n  Test 3: Multi-word pipeline (7 inputs)\n";
        
        // Multi-word truth tables (7 inputs = 128 patterns = 2 words)
        std::vector<uint64_t> target_tt = {0x0000000000000001ULL, 0x0000000000000000ULL}; // Only pattern 0 → 1
        std::vector<std::vector<uint64_t>> divisor_tts = {
            {0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL}, // Input pattern
            {0xCCCCCCCCCCCCCCCCULL, 0xCCCCCCCCCCCCCCCCULL}, // Input pattern
            {0xF0F0F0F0F0F0F0F0ULL, 0xF0F0F0F0F0F0F0F0ULL}  // Input pattern
        };
        std::vector<int> selected_divisors = {0, 1, 2};
        int num_inputs = 7;
        
        // Step 1: Convert multi-word truth tables
        std::vector<std::vector<bool>> br;
        convert_to_exopt_format(target_tt, divisor_tts, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Multi-word conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        ASSERT(br.size() == 8); // 2^3 patterns
        
        // Step 2: Synthesize (should fail - this is an infeasible synthesis)
        SynthesisResult result = synthesize_circuit(br, {}, 10);
        
        std::cout << "    Step 2: " << (result.success ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED (expected)");
        if (result.success) {
            std::cout << " (unexpected success with " << result.synthesized_gates << " gates)";
        }
        std::cout << "\n";
        ASSERT(!result.success); // Should fail
    }
    
    std::cout << "\n  ✓ End-to-end pipeline testing completed\n\n";
}

// ============================================================================
// MAIN TEST DRIVER
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "        SYNTHESIS TEST SUITE           \n";
    std::cout << "========================================\n\n";
    
    test_basic_logic_functions();
    test_constant_functions();
    test_multi_input_functions();
    test_error_conditions();
    test_conversion_function();
    test_end_to_end_pipeline();
    
    std::cout << "========================================\n";
    std::cout << "         TEST RESULTS SUMMARY          \n";
    std::cout << "========================================\n";
    
    if (passed_tests == total_tests) {
        std::cout << "🎉 ALL TESTS PASSED! (" << passed_tests << "/" << total_tests << ")\n\n";
        return 0;
    } else {
        std::cout << "❌ TESTS FAILED! (" << passed_tests << "/" << total_tests << ")\n\n";
        return 1;
    }
}
