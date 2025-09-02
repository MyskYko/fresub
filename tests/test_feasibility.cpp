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

void test_small_k_helpers_and_enumerators() {
    std::cout << "\n=== TESTING SMALL-K FEASIBILITY HELPERS ===\n";

    // Base patterns for 4 inputs in 1 word (16 patterns),
    // duplicate across 64 bits to avoid needing masks
    const int num_inputs = 4;
    const uint64_t A = 0xaaaaaaaaaaaaaaaaull; // bit 0 pattern duplicated
    const uint64_t B = 0xccccccccccccccccull; // bit 1 pattern duplicated
    const uint64_t C = 0xf0f0f0f0f0f0f0f0ull; // bit 2 pattern duplicated
    const uint64_t D = 0xff00ff00ff00ff00ull; // bit 3 pattern duplicated

    // k=0: constant target
    {
        std::vector<std::vector<uint64_t>> tt_const0 = { {0x0000000000000000ull} };
        std::vector<std::vector<uint64_t>> tt_const1 = { {0xffffffffffffffffull} };
        std::vector<std::vector<uint64_t>> tt_nonconst = { {A} };
        ASSERT(solve_resub_overlap_multiword_0(tt_const0, num_inputs) == true);
        ASSERT(solve_resub_overlap_multiword_0(tt_const1, num_inputs) == true);
        ASSERT(solve_resub_overlap_multiword_0(tt_nonconst, num_inputs) == false);
    }

    // k=1: one divisor
    {
        // Divisors: [A], target=A -> feasible
        std::vector<std::vector<uint64_t>> tts = { {A}, {A} };
        ASSERT(solve_resub_overlap_multiword_1(0, tts, num_inputs) == true);
        // Target=B while divisor=A -> infeasible
        tts.back()[0] = B;
        ASSERT(solve_resub_overlap_multiword_1(0, tts, num_inputs) == false);
    }

    // k=2: two divisors
    {
        // Divisors: [A, B], target=A&B -> feasible
        std::vector<std::vector<uint64_t>> tts = { {A}, {B}, {A & B} };
        ASSERT(solve_resub_overlap_multiword_2(0, 1, tts, num_inputs) == true);
        // Target=C -> infeasible with divisors A,B
        tts.back()[0] = C;
        ASSERT(solve_resub_overlap_multiword_2(0, 1, tts, num_inputs) == false);
    }

    // k=3: three divisors
    {
        // Divisors: [A, B, C], target=(A&B)|C -> feasible
        std::vector<std::vector<uint64_t>> tts = { {A}, {B}, {C}, {(A & B) | C} };
        ASSERT(solve_resub_overlap_multiword_3(0, 1, 2, tts, num_inputs) == true);
        // Target=D -> infeasible with divisors A,B,C
        tts.back()[0] = D;
        ASSERT(solve_resub_overlap_multiword_3(0, 1, 2, tts, num_inputs) == false);
    }

    std::cout << "\n=== TESTING SMALL-K ENUMERATORS ===\n";
    // Enumerators k=0..3
    {
        // k=0: constant
        std::vector<std::vector<uint64_t>> tts0 = { {0xffffffffffffffffull} };
        std::vector<FeasibleSet> c0;
        find_feasible_0resub(tts0, num_inputs, c0);
        std::cout << "c0 size=" << c0.size() << "\n";
        ASSERT(c0.size() == 1);
        ASSERT(c0[0].divisor_indices.empty());

        std::vector<std::vector<uint64_t>> tts0b = { {A} };
        std::vector<FeasibleSet> c0b;
        find_feasible_0resub(tts0b, num_inputs, c0b);
        ASSERT(c0b.empty());

        // k=1: target=A
        std::vector<std::vector<uint64_t>> tts1 = { {A}, {A} };
        std::vector<FeasibleSet> c1;
        find_feasible_1resub(tts1, num_inputs, c1);
        ASSERT(c1.size() == 1 && c1[0].divisor_indices.size() == 1 && c1[0].divisor_indices[0] == 0);

        // k=2: target = A ^ B
        std::vector<std::vector<uint64_t>> tts2 = { {A}, {B}, {C}, {A ^ B} };
        std::vector<FeasibleSet> c2;
        find_feasible_2resub(tts2, num_inputs, c2);
        // Only pair {0,1} should be feasible
        ASSERT(c2.size() >= 1);
        bool has01 = false; bool others = false;
        for (auto& fs : c2) {
            const auto& v = fs.divisor_indices;
            if (v.size() == 2 && v[0] == 0 && v[1] == 1) has01 = true; else others = true;
        }
        ASSERT(has01 == true);
        ASSERT(others == false);

        // k=3: target = (A&B)|C with divisors [A,B,C,D]
        std::vector<std::vector<uint64_t>> tts3 = { {A}, {B}, {C}, {D}, {(A & B) | C} };
        std::vector<FeasibleSet> c3;
        find_feasible_3resub(tts3, num_inputs, c3);
        // Only {0,1,2} should be feasible
        ASSERT(c3.size() >= 1);
        bool has012 = false; bool others3 = false;
        for (auto& fs : c3) {
            const auto& v = fs.divisor_indices;
            if (v.size() == 3 && v[0] == 0 && v[1] == 1 && v[2] == 2) has012 = true; else others3 = true;
        }
        ASSERT(has012 == true);
        ASSERT(others3 == false);
    }

    std::cout << "✓ Small-k feasibility helpers and enumerators tests completed\n";
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
    
    std::vector<FeasibleSet> fs4;
    find_feasible_4resub(truth_tables, num_inputs, fs4);
    
    std::cout << "Found " << fs4.size() << " feasible 4-input combinations\n";
    for (size_t i = 0; i < fs4.size() && i < 5; i++) {
        const auto& combo = fs4[i].divisor_indices;
        if (combo.size() == 4) {
            std::cout << "  Combination " << i << ": [" 
                      << combo[0] << ", " << combo[1] << ", " 
                      << combo[2] << ", " << combo[3] << "]\n";
        }
    }
    
    ASSERT(fs4.size() > 0); // Should find some combinations
    std::cout << "✓ find_feasible_4resub working\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "       FEASIBILITY TEST SUITE          \n";
    std::cout << "========================================\n\n";
    
    test_synthetic_truth_tables();
    test_small_k_helpers_and_enumerators();
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
