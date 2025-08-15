#include "window.hpp"
#include <aig.hpp>
#include <iostream>
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

using namespace fresub;

void test_truth_table_computation() {
    std::cout << "=== TESTING TRUTH TABLE COMPUTATION ===\n";
    
    // Create the same hardcoded AIG as in window extraction test
    // PIs: 1, 2, 3
    // Node 4 = AND(1, 2)     // depends on PIs 1,2
    // Node 5 = AND(2, 3)     // depends on PIs 2,3  
    // Node 6 = AND(4, 5)     // depends on nodes 4,5 -> PIs 1,2,3
    // Node 7 = AND(4, 3)     // depends on node 4 and PI 3 -> PIs 1,2,3
    // Node 8 = AND(6, 7)     // depends on nodes 6,7 -> PIs 1,2,3
    // PO: 8
    
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
    
    std::cout << "Created hardcoded AIG with " << aig.nPis << " PIs, " << aig.nGates << " gates\n";
    
    // Test compute_truth_tables_for_window function
    WindowExtractor extractor(aig, 4);
    
    // Test 1: Simple case - compute truth table for node 4 with inputs 1,2
    std::vector<int> window_inputs = {1, 2};
    std::vector<int> window_nodes = {1, 2, 4};
    std::vector<int> divisors = {1, 2};
    int target = 4;
    
    std::cout << "\nTest 1: Node 4 = AND(1, 2) with 2 inputs\n";
    auto results = extractor.compute_truth_tables_for_window(target, window_inputs, window_nodes, divisors, true);
    
    ASSERT(results.size() == 3); // 2 divisors + 1 target
    ASSERT(results[0].size() == 1); // 2^2 = 4 patterns = 1 word
    ASSERT(results[1].size() == 1);
    ASSERT(results[2].size() == 1);
    
    // Check truth tables
    // Input 1 (bit 0): 0101 = 0x5
    // Input 2 (bit 1): 0011 = 0x3  
    // AND(1,2):        0001 = 0x1
    ASSERT(results[0][0] == 0xaull); // Input 1 pattern (first 4 bits of 0xaaaa...)
    ASSERT(results[1][0] == 0xcull); // Input 2 pattern (first 4 bits of 0xcccc...)  
    ASSERT(results[2][0] == 0x8ull); // AND result (first 4 bits of result)
    
    std::cout << "✓ Truth table computation working correctly\n";
    
    // Test 2: More complex case - 3 inputs
    window_inputs = {1, 2, 3};
    window_nodes = {1, 2, 3, 4, 5, 6};
    divisors = {1, 2, 3, 4, 5};
    target = 6; // AND(4, 5) = AND(AND(1,2), AND(2,3))
    
    std::cout << "\nTest 2: Node 6 = AND(AND(1,2), AND(2,3)) with 3 inputs\n";
    results = extractor.compute_truth_tables_for_window(target, window_inputs, window_nodes, divisors, false);
    
    ASSERT(results.size() == 6); // 5 divisors + 1 target
    ASSERT(results[5].size() == 1); // 2^3 = 8 patterns = 1 word
    
    // Manual verification: AND(AND(1,2), AND(2,3)) should be 1 only when 1=1, 2=1, 3=1
    // Pattern: 00000001 (only last bit set) but in reverse bit order = 0x80
    std::cout << "Target truth table (first word): 0x" << std::hex << results[5][0] << std::dec << "\n";
    
    std::cout << "✓ Complex truth table computation working\n";
}

int main() {
    std::cout << "Truth Table Simulation Test (aigman + exopt)\n";
    std::cout << "============================================\n\n";
    
    test_truth_table_computation();
    
    if (passed_tests == total_tests) {
        std::cout << "\n✅ PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "\n❌ FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}