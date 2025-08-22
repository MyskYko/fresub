#include <cassert>
#include <iostream>

#include <aig.hpp>

#include "feasibility.hpp"
#include "simulation.hpp"
#include "window.hpp"

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

using namespace fresub;

void print_truth_table_multiword(const std::vector<uint64_t>& tt, int num_inputs, const std::string& label = "") {
    if (!label.empty()) std::cout << label << ": ";
    
    int num_patterns = 1 << num_inputs;
    for (int i = num_patterns - 1; i >= 0; i--) {
        int word_idx = i / 64;
        int bit_idx = i % 64;
        if (word_idx < tt.size()) {
            std::cout << ((tt[word_idx] >> bit_idx) & 1);
        } else {
            std::cout << "0";
        }
    }
    std::cout << " (patterns=" << num_patterns << ", words=" << tt.size() << ")";
}

void test_synthetic_truth_tables() {
    std::cout << "=== TESTING SYNTHETIC TRUTH TABLES ===\n";
    std::cout << "Testing feasibility enumeration with hand-crafted truth table data...\n";
    
    // Test Case 1: Simple feasible case (4 inputs, 1 word)
    {
        std::cout << "\n  Test 1: Simple feasible case (4 inputs, 1 word)\n";
        int num_inputs = 4;
        int num_patterns = 1 << num_inputs;
        int num_words = 1;
        
        // Create synthetic truth tables for 4-input test
        // Divisors: a, b, c, d (inputs)
        // Target: a & b & c (should be feasible)
        std::vector<std::vector<uint64_t>> divisors(4, std::vector<uint64_t>(num_words));
        std::vector<uint64_t> target(num_words);
        
        // Fill truth tables manually for 4 inputs (16 patterns)
        uint64_t pattern_a = 0xaaaa; // 1010101010101010 (bit 0)
        uint64_t pattern_b = 0xcccc; // 1100110011001100 (bit 1)  
        uint64_t pattern_c = 0xf0f0; // 1111000011110000 (bit 2)
        uint64_t pattern_d = 0xff00; // 1111111100000000 (bit 3)
        uint64_t target_abc = pattern_a & pattern_b & pattern_c; // a & b & c
        
        divisors[0][0] = pattern_a;
        divisors[1][0] = pattern_b;
        divisors[2][0] = pattern_c;
        divisors[3][0] = pattern_d;
        target[0] = target_abc;
        
        std::cout << "    Divisors:\n";
        print_truth_table_multiword(divisors[0], num_inputs, "      a");
        std::cout << "\n";
        print_truth_table_multiword(divisors[1], num_inputs, "      b");
        std::cout << "\n";
        print_truth_table_multiword(divisors[2], num_inputs, "      c");
        std::cout << "\n";
        print_truth_table_multiword(divisors[3], num_inputs, "      d");
        std::cout << "\n";
        print_truth_table_multiword(target, num_inputs, "    Target (a&b&c)");
        std::cout << "\n";
        
        // Combine divisors and target into single vector (target last)
        std::vector<std::vector<uint64_t>> combined_tts = divisors;
        combined_tts.push_back(target);
        
        // Test feasibility: should be feasible since target = a & b & c
        bool feasible = solve_resub_overlap_multiword(0, 1, 2, 3, combined_tts, num_inputs);
        std::cout << "    Feasibility result: " << (feasible ? "FEASIBLE" : "NOT FEASIBLE") << "\n";
        ASSERT(feasible == true); // Should be feasible
    }
    
    // Test Case 2: Infeasible case
    {
        std::cout << "\n  Test 2: Infeasible case (4 inputs, 1 word)\n";
        int num_inputs = 4;
        int num_words = 1;
        
        // Create truly infeasible case - onset and offset patterns that cannot be distinguished
        // All divisors have same value for patterns that target treats differently
        std::vector<std::vector<uint64_t>> divisors(4, std::vector<uint64_t>(num_words));
        std::vector<uint64_t> target(num_words);
        
        // Create divisors that all have the same pattern - they cannot distinguish anything
        uint64_t same_pattern = 0xaaaa; // All divisors identical
        divisors[0][0] = same_pattern;
        divisors[1][0] = same_pattern;
        divisors[2][0] = same_pattern;
        divisors[3][0] = same_pattern;
        
        // Target has different values where divisors cannot distinguish
        uint64_t target_pattern = 0xcccc; // Different from divisors
        target[0] = target_pattern;
        
        std::cout << "    All divisors identical: ";
        print_truth_table_multiword(divisors[0], num_inputs);
        std::cout << "\n    Target: ";
        print_truth_table_multiword(target, num_inputs);
        std::cout << "\n";
        
        // Combine divisors and target into single vector (target last)
        std::vector<std::vector<uint64_t>> combined_tts = divisors;
        combined_tts.push_back(target);
        
        bool feasible = solve_resub_overlap_multiword(0, 1, 2, 3, combined_tts, num_inputs);
        std::cout << "    Feasibility result: " << (feasible ? "FEASIBLE" : "NOT FEASIBLE") << "\n";
        ASSERT(feasible == false); // Should be infeasible - cannot distinguish onset from offset
    }
    
    std::cout << "\n  ✓ Synthetic truth table feasibility tests completed\n";
}

void test_feasibility_with_aigman() {
    std::cout << "\n=== TESTING FEASIBILITY WITH AIGMAN ===\n";
    
    // Create the same hardcoded AIG as before
    aigman aig(3, 1);  // 3 PIs, 1 PO
    aig.vObjs.resize(9 * 2); 
    
    // Node 4 = AND(1, 2)
    aig.vObjs[4 * 2] = 2;     
    aig.vObjs[4 * 2 + 1] = 4; 
    
    // Node 5 = AND(2, 3)
    aig.vObjs[5 * 2] = 4;    
    aig.vObjs[5 * 2 + 1] = 6; 
    
    // Node 6 = AND(4, 5)
    aig.vObjs[6 * 2] = 8;     
    aig.vObjs[6 * 2 + 1] = 10; 
    
    // Node 7 = AND(4, 3)
    aig.vObjs[7 * 2] = 8;     
    aig.vObjs[7 * 2 + 1] = 6; 
    
    // Node 8 = AND(6, 7)
    aig.vObjs[8 * 2] = 12;    
    aig.vObjs[8 * 2 + 1] = 14; 
    
    aig.nGates = 5;          
    aig.nObjs = 9;           
    aig.vPos[0] = 16;        
    
    std::cout << "Created hardcoded AIG for feasibility testing\n";
    
    // Test feasibility analysis on windows
    WindowExtractor extractor(aig, 4, true);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    ASSERT(!windows.empty());
    
    int tested_windows = 0;
    int feasible_windows = 0;
    
    for (const auto& window : windows) {
        if (window.divisors.size() >= 4 && window.inputs.size() <= 6) {
            // Test feasibility using real AIG-derived truth tables
            auto truth_tables = fresub::compute_truth_tables_for_window(aig, window, false);
            
            if (truth_tables.size() >= 5) { // Need at least 4 divisors + 1 target
                // Take first 4 divisors + target (truth_tables already has target as last element)
                std::vector<std::vector<uint64_t>> selected_tts;
                for (int i = 0; i < 4; i++) {
                    selected_tts.push_back(truth_tables[i]);
                }
                selected_tts.push_back(truth_tables.back()); // Add target
                
                bool feasible = solve_resub_overlap_multiword(0, 1, 2, 3, selected_tts, window.inputs.size());
                
                if (feasible) {
                    feasible_windows++;
                }
                tested_windows++;
                
                std::cout << "  Window target=" << window.target_node 
                          << " inputs=" << window.inputs.size() 
                          << " divisors=" << window.divisors.size()
                          << " -> " << (feasible ? "FEASIBLE" : "NOT FEASIBLE") << "\n";
            }
        }
    }
    
    std::cout << "\nTested " << tested_windows << " windows with 4+ divisors\n";
    std::cout << "Found " << feasible_windows << " feasible, " 
              << (tested_windows - feasible_windows) << " infeasible\n";
    
    ASSERT(tested_windows > 0); // Should find some windows to test
    std::cout << "✓ AIG-based feasibility analysis completed\n";
}

void test_find_feasible_4resub() {
    std::cout << "\n=== TESTING FIND_FEASIBLE_4RESUB ===\n";
    
    // Create simple test case
    int num_inputs = 4;
    std::vector<std::vector<uint64_t>> truth_tables(7, std::vector<uint64_t>(1)); // 6 divisors + 1 target
    
    // Simple patterns for divisors
    truth_tables[0][0] = 0xaaaa; // a
    truth_tables[1][0] = 0xcccc; // b  
    truth_tables[2][0] = 0xf0f0; // c
    truth_tables[3][0] = 0xff00; // d
    truth_tables[4][0] = 0xaaaa & 0xcccc; // a & b
    truth_tables[5][0] = 0xf0f0 & 0xff00; // c & d
    // Target is last element
    truth_tables[6][0] = truth_tables[4][0] | truth_tables[5][0]; // (a & b) | (c & d)
    
    auto feasible_combinations = find_feasible_4resub(truth_tables, num_inputs);
    
    std::cout << "Found " << feasible_combinations.size() << " feasible 4-input combinations\n";
    for (size_t i = 0; i < feasible_combinations.size() && i < 5; i++) {
        const auto& combo = feasible_combinations[i];
        if (combo.size() == 4) {
            std::cout << "  Combination " << i << ": [" 
                      << combo[0] << ", " << combo[1] << ", " 
                      << combo[2] << ", " << combo[3] << "]\n";
        }
    }
    
    ASSERT(feasible_combinations.size() > 0); // Should find some combinations
    std::cout << "✓ find_feasible_4resub working\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "       FEASIBILITY TEST SUITE          \n";
    std::cout << "========================================\n\n";
    
    test_synthetic_truth_tables();
    test_feasibility_with_aigman(); 
    test_find_feasible_4resub();
    
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
