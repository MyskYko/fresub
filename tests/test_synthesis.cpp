#include <cassert>
#include <iostream>
#include <vector>

#include "synthesis.hpp"

using namespace fresub;

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
            aigman* result = synthesize_circuit(input.br, 10);
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                if (test.expected_gates > 0) {
                    ASSERT(result->nGates <= test.expected_gates + 1); // Allow some tolerance
                }
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
            
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
            aigman* result = synthesize_circuit(br, 10);
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
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
            aigman* result = synthesize_circuit(br, 10);
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    std::cout << "\n  ✓ Constant functions completed\n\n";
}

// ============================================================================
// TEST 3: Mockturtle Synthesis with Variable Input Counts
// ============================================================================
void test_mockturtle_variable_inputs() {
    std::cout << "=== Test 3: Mockturtle Variable Input Counts ===\n";
    
    // Test 0-input (constant) function
    {
        std::cout << "\n  Testing 0-input constant 0\n";
        std::vector<std::vector<bool>> br(1, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // Constant 0
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates == 0);
            ASSERT(result->nPis == 0);
            ASSERT(result->nPos == 1);
            ASSERT(result->nObjs == 1);  // Just constant node
            ASSERT(result->vPos.size() == 1);
            ASSERT(result->vPos[0] == 0);  // Output is constant 0
            std::cout << "\n    Structure: nObjs=" << result->nObjs 
                      << ", vObjs.size=" << result->vObjs.size()
                      << ", output=" << result->vPos[0];
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 1-input buffer function
    {
        std::cout << "\n  Testing 1-input buffer (identity)\n";
        std::vector<std::vector<bool>> br(2, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // 0 -> 0
        br[1][0] = false; br[1][1] = true;   // 1 -> 1
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates == 0);
            ASSERT(result->nPis == 1);
            ASSERT(result->nPos == 1);
            ASSERT(result->nObjs == 2);  // Constant + 1 PI
            ASSERT(result->vPos.size() == 1);
            ASSERT(result->vPos[0] == 2);  // Output is PI 1 (literal 2)
            std::cout << "\n    Structure: nObjs=" << result->nObjs 
                      << ", vObjs.size=" << result->vObjs.size()
                      << ", output=" << result->vPos[0];
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 1-input inverter function
    {
        std::cout << "\n  Testing 1-input inverter\n";
        std::vector<std::vector<bool>> br(2, std::vector<bool>(2));
        br[0][0] = false; br[0][1] = true;   // 0 -> 1
        br[1][0] = true;  br[1][1] = false;  // 1 -> 0
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates == 0);  // Inverter is free in AIG
            ASSERT(result->nPis == 1);
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 2-input AND function
    {
        std::cout << "\n  Testing 2-input AND\n";
        std::vector<std::vector<bool>> br(4, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // 00 -> 0
        br[1][0] = true;  br[1][1] = false;  // 01 -> 0
        br[2][0] = true;  br[2][1] = false;  // 10 -> 0
        br[3][0] = false; br[3][1] = true;   // 11 -> 1
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates == 1);
            ASSERT(result->nPis == 2);
            ASSERT(result->nPos == 1);
            ASSERT(result->nObjs == 4);  // Constant + 2 PIs + 1 gate
            ASSERT(result->vObjs.size() >= 6);  // Need space for gate at position 3
            // Verify gate structure: node 3 should have two fanins
            ASSERT(result->vObjs[6] != 0 || result->vObjs[7] != 0);  // Gate fanins
            std::cout << "\n    Structure: nObjs=" << result->nObjs 
                      << ", vObjs.size=" << result->vObjs.size()
                      << ", gate fanins=[" << result->vObjs[6] << "," << result->vObjs[7] << "]"
                      << ", output=" << result->vPos[0];
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 2-input XOR function
    {
        std::cout << "\n  Testing 2-input XOR\n";
        std::vector<std::vector<bool>> br(4, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // 00 -> 0
        br[1][0] = false; br[1][1] = true;   // 01 -> 1
        br[2][0] = false; br[2][1] = true;   // 10 -> 1
        br[3][0] = true;  br[3][1] = false;  // 11 -> 0
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates <= 3);  // XOR needs at most 3 AND gates
            ASSERT(result->nPis == 2);
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 3-input majority function
    {
        std::cout << "\n  Testing 3-input majority\n";
        std::vector<std::vector<bool>> br(8, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // 000 -> 0
        br[1][0] = true;  br[1][1] = false;  // 001 -> 0
        br[2][0] = true;  br[2][1] = false;  // 010 -> 0
        br[3][0] = false; br[3][1] = true;   // 011 -> 1
        br[4][0] = true;  br[4][1] = false;  // 100 -> 0
        br[5][0] = false; br[5][1] = true;   // 101 -> 1
        br[6][0] = false; br[6][1] = true;   // 110 -> 1
        br[7][0] = false; br[7][1] = true;   // 111 -> 1
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nGates <= 4);  // Majority needs at most 4 AND gates
            ASSERT(result->nPis == 3);
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test 3-input with don't cares
    {
        std::cout << "\n  Testing 3-input with don't cares\n";
        std::vector<std::vector<bool>> br(8, std::vector<bool>(2));
        br[0][0] = true;  br[0][1] = false;  // 000 -> 0
        br[1][0] = true;  br[1][1] = true;   // 001 -> don't care
        br[2][0] = true;  br[2][1] = false;  // 010 -> 0
        br[3][0] = false; br[3][1] = true;   // 011 -> 1
        br[4][0] = true;  br[4][1] = true;   // 100 -> don't care
        br[5][0] = false; br[5][1] = true;   // 101 -> 1
        br[6][0] = false; br[6][1] = true;   // 110 -> 1
        br[7][0] = false; br[7][1] = true;   // 111 -> 1
        
        aigman* result = synthesize_circuit_mockturtle(br, 10);
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates, " << result->nPis << " inputs)";
            ASSERT(result->nPis == 3);
            delete result;
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    std::cout << "\n  ✓ Mockturtle variable input tests completed\n\n";
}

// ============================================================================
// TEST 4: Multi-Input Functions
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
            aigman* result = synthesize_circuit(br, 10);
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                ASSERT(result->nGates == 3); // 4-input AND needs exactly 3 gates
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
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
            aigman* result = synthesize_circuit(input.br, 1); // Only 1 gate
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED") << "\n";
            if (result) delete result; // Clean up if somehow succeeded
            ASSERT(result == nullptr); // Should fail due to gate limit
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
        
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0x3}, // Input 0: 0011 (single word)
            {0x5}, // Input 1: 0101 (single word)
            {0x8}  // Target (AND): 1000 (single word)
        };
        std::vector<int> selected_divisors = {0, 1};
        int num_inputs = 2;
        
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
                               num_inputs, br);
        
        ASSERT(br.size() == 4);
        ASSERT(br[0].size() == 2);
        
        std::cout << "    ✓ Conversion successful (" << br.size() << " patterns)\n";
    }
    
    // Test 2: Function with internal divisors
    {
        std::cout << "\n  Testing function with internal divisors\n";
        
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0x3}, // Divisor 0 (single word)
            {0x5}, // Divisor 1 (single word)
            {0x1}, // Divisor 2 (single word)
            {0x7}, // Divisor 3 (single word)
            {0xF}  // Target: All ones (single word)
        };
        std::vector<int> selected_divisors = {0, 1, 2, 3};
        int num_inputs = 2;
        
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
                               num_inputs, br);
        
        ASSERT(br.size() == 16);
        
        std::cout << "    ✓ Conversion successful (" << " internal divisors)\n";
    }
    
    // Test 3: Multi-word truth table (7 inputs, 2 words)
    {
        std::cout << "\n  Testing multi-word truth table (7 inputs)\n";
        
        int num_inputs = 7;
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0x3333333333333333ULL, 0x5555555555555555ULL}, // Divisor 0: 2 words
            {0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL}, // Divisor 1: 2 words  
            {0x00000000FFFFFFFFULL, 0x0000FFFF0000FFFFULL}, // Divisor 2: 2 words
            {0x5555555555555555ULL, 0x3333333333333333ULL}  // Target: 2 words
        };
        std::vector<int> selected_divisors = {0, 1, 2};
        
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
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
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0xC}, // Input A: 1100  
            {0xA}, // Input B: 1010
            {0x8}  // Target (AND): 1000
        };
        std::vector<int> selected_divisors = {0, 1};
        int num_inputs = 2;
        
        // Step 1: Convert truth tables to synthesis format
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        // Step 2: Synthesize circuit
        aigman* result = synthesize_circuit(br, 10);
        
        std::cout << "    Step 2: " << (result ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates)";
            ASSERT(result->nGates == 1); // AND should be 1 gate
            delete result; // Clean up
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test Case 2: 3-input function with internal divisors
    {
        std::cout << "\n  Test 2: 3-input function with internal divisors\n";
        
        // Manual truth tables for 3-input case
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0xF0}, // Divisor 0: 11110000
            {0xCC}, // Divisor 1: 11001100  
            {0xAA}, // Divisor 2: 10101010
            {0x80}, // Divisor 3: 10000000 (internal, not an input)
            {0xFE}  // Target: Complex 3-input function: 11111110
        };
        std::vector<int> selected_divisors = {0, 1, 2, 3};
        int num_inputs = 3;
        
        // Step 1: Convert 
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        ASSERT(br.size() == 16); // 2^4 patterns
        
        // Step 2: Synthesize
        aigman* result = synthesize_circuit(br, 10);
        
        std::cout << "    Step 2: " << (result ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED");
        if (result) {
            std::cout << " (" << result->nGates << " gates)";
            delete result; // Clean up
        }
        std::cout << "\n";
        ASSERT(result != nullptr);
    }
    
    // Test Case 3: Multi-word truth table pipeline (7 inputs)
    {
        std::cout << "\n  Test 3: Multi-word pipeline (7 inputs)\n";
        
        // Multi-word truth tables (7 inputs = 128 patterns = 2 words)
        std::vector<std::vector<uint64_t>> truth_tables = {
            {0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL}, // Input pattern
            {0xCCCCCCCCCCCCCCCCULL, 0xCCCCCCCCCCCCCCCCULL}, // Input pattern
            {0xF0F0F0F0F0F0F0F0ULL, 0xF0F0F0F0F0F0F0F0ULL}, // Input pattern
            {0x0000000000000001ULL, 0x0000000000000000ULL}  // Target: Only pattern 0 → 1
        };
        std::vector<int> selected_divisors = {0, 1, 2};
        int num_inputs = 7;
        
        // Step 1: Convert multi-word truth tables
        std::vector<std::vector<bool>> br;
        generate_relation(truth_tables, selected_divisors,
                               num_inputs, br);
        
        std::cout << "    Step 1: ✓ Multi-word conversion successful\n";
        std::cout << "      Binary relation: " << br.size() << " patterns\n";
        
        ASSERT(br.size() == 8); // 2^3 patterns
        
        // Step 2: Synthesize (should fail - this is an infeasible synthesis)
        aigman* result = synthesize_circuit(br, 10);
        
        std::cout << "    Step 2: " << (result ? "✓ Synthesis SUCCESS" : "✗ Synthesis FAILED (expected)");
        if (result) {
            std::cout << " (unexpected success with " << result->nGates << " gates)";
            delete result; // Clean up
        }
        std::cout << "\n";
        ASSERT(result == nullptr); // Should fail
    }
    
    std::cout << "\n  ✓ End-to-end pipeline testing completed\n\n";
}

// ============================================================================
// TEST 7: Mockturtle Synthesis
// ============================================================================

void test_mockturtle_synthesis() {
    std::cout << "=== TESTING MOCKTURTLE SYNTHESIS ===\n";
    
    // Test 1: 4-input AND function
    {
        std::cout << "\n  Testing 4-input AND with mockturtle\n";
        
        std::vector<std::vector<bool>> br(16, std::vector<bool>(2, true));
        
        // Set up 4-input AND: only pattern 15 (1111) should output 1
        for (int p = 0; p < 16; p++) {
            bool output = (p == 15); // Only all inputs = 1 gives output 1
            br[p][output ? 1 : 0] = true;
            br[p][output ? 0 : 1] = false;
        }
        
        try {
            aigman* result = synthesize_circuit_mockturtle(br, 10); // Allow up to 10 gates
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    // Test 2: 4-input XOR function
    {
        std::cout << "\n  Testing 4-input XOR with mockturtle\n";
        
        std::vector<std::vector<bool>> br(16, std::vector<bool>(2, true));
        
        // Set up 4-input XOR: output 1 if odd number of inputs are 1
        for (int p = 0; p < 16; p++) {
            int bit_count = __builtin_popcount(p);
            bool output = (bit_count % 2 == 1);
            br[p][output ? 1 : 0] = true;
            br[p][output ? 0 : 1] = false;
        }
        
        try {
            aigman* result = synthesize_circuit_mockturtle(br, 10); // Allow up to 10 gates
            std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED");
            if (result) {
                std::cout << " (" << result->nGates << " gates)";
                delete result; // Clean up
            }
            std::cout << "\n";
            ASSERT(result != nullptr);
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << "\n";
            ASSERT(false);
        }
    }
    
    // Test 3: Gate limit constraint
    {
        std::cout << "\n  Testing gate limit constraint (XOR with 1 gate limit)\n";
        
        // 4-input XOR: should need more than 1 gate
        std::vector<std::vector<bool>> br(16, std::vector<bool>(2, true));
        for (int p = 0; p < 16; p++) {
            // XOR: output 1 when odd number of inputs are 1
            int count = 0;
            for (int i = 0; i < 4; i++) {
                if ((p >> i) & 1) count++;
            }
            bool output = (count % 2) == 1;
            br[p][output ? 1 : 0] = true;
            br[p][output ? 0 : 1] = false;
        }
        
        aigman* result = synthesize_circuit_mockturtle(br, 1); // Very restrictive limit
        std::cout << "    Result: " << (result ? "SUCCESS" : "FAILED (expected)") << "\n";
        if (result) delete result; // Clean up if somehow succeeded
        ASSERT(result == nullptr); // Should fail due to gate limit
    }
    
    std::cout << "\n  ✓ Mockturtle synthesis testing completed\n\n";
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
    test_mockturtle_variable_inputs();  // Test variable input counts for mockturtle
    test_multi_input_functions();
    test_error_conditions();
    test_conversion_function();
    test_end_to_end_pipeline();
    test_mockturtle_synthesis();
    
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
