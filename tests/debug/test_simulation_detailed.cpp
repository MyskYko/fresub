#include "fresub_aig.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <bitset>

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

void test_basic_simulation() {
    std::cout << "Testing basic simulation...\n";
    
    // Create simple AND gate: f = a & b
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=a&b
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.nodes[3].fanin0 = AIG::var2lit(1); // a
    aig.nodes[3].fanin1 = AIG::var2lit(2); // b
    
    aig.pos.push_back(AIG::var2lit(3));
    aig.num_pos = 1;
    
    // Test all input combinations for AND gate
    std::cout << "  Testing AND gate truth table...\n";
    
    // Test 00 -> 0
    std::vector<uint64_t> inputs = {0x0, 0x0};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0x0);
    
    // Test 01 -> 0  
    inputs = {0x0, 0xFFFFFFFFFFFFFFFF};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0x0);
    
    // Test 10 -> 0
    inputs = {0xFFFFFFFFFFFFFFFF, 0x0};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0x0);
    
    // Test 11 -> 1
    inputs = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0xFFFFFFFFFFFFFFFF);
    
    std::cout << "  Basic simulation tests completed\n";
}

void test_complex_simulation() {
    std::cout << "Testing complex logic simulation...\n";
    
    // Create OR gate: f = a | b = !(!a & !b)
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=!a&!b
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: !a & !b
    aig.nodes[3].fanin0 = AIG::var2lit(1, true);  // !a
    aig.nodes[3].fanin1 = AIG::var2lit(2, true);  // !b
    
    aig.pos.push_back(AIG::var2lit(3, true)); // Output is !(!a&!b) = a|b
    aig.num_pos = 1;
    
    // Test OR truth table
    std::cout << "  Testing OR truth table...\n";
    
    // Test 00 -> 0 (false OR false = false)
    std::vector<uint64_t> inputs = {0x0, 0x0};
    aig.simulate(inputs);
    uint64_t result = aig.get_sim_value(3); // Node 3 is !a&!b
    uint64_t or_result = ~result; // !(!a&!b) = a|b
    ASSERT(or_result == 0x0); // 0 OR 0 = 0
    
    // Test 01 -> 1 (false OR true = true)
    inputs = {0x0, 0xFFFFFFFFFFFFFFFF};
    aig.simulate(inputs);
    result = aig.get_sim_value(3);
    or_result = ~result;
    ASSERT(or_result == 0xFFFFFFFFFFFFFFFF); // 0 OR 1 = 1
    
    // Test 10 -> 1 (true OR false = true)
    inputs = {0xFFFFFFFFFFFFFFFF, 0x0};
    aig.simulate(inputs);
    result = aig.get_sim_value(3);
    or_result = ~result;
    ASSERT(or_result == 0xFFFFFFFFFFFFFFFF); // 1 OR 0 = 1
    
    // Test 11 -> 1 (true OR true = true)  
    inputs = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    aig.simulate(inputs);
    result = aig.get_sim_value(3);
    or_result = ~result;
    ASSERT(or_result == 0xFFFFFFFFFFFFFFFF); // 1 OR 1 = 1
    
    std::cout << "  Complex logic simulation tests completed\n";
}

void test_parallel_simulation() {
    std::cout << "Testing parallel simulation with bit vectors...\n";
    
    // Create simple OR gate: f = a | b = !(!a & !b)
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=!(!a&!b)
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: !(!a & !b) = a | b
    aig.nodes[3].fanin0 = AIG::var2lit(1, true); // !a
    aig.nodes[3].fanin1 = AIG::var2lit(2, true); // !b
    // Result is !(!a & !b) which equals (a | b)
    
    aig.pos.push_back(AIG::var2lit(3, true)); // Invert to get OR
    aig.num_pos = 1;
    
    // Test with bit patterns that represent multiple test cases
    std::cout << "  Testing OR gate with bit patterns...\n";
    
    // Pattern: alternating bits to test multiple cases simultaneously
    uint64_t pattern_a = 0xAAAAAAAAAAAAAAAA; // 1010101010...
    uint64_t pattern_b = 0xCCCCCCCCCCCCCCCC; // 1100110011...
    uint64_t expected_or = pattern_a | pattern_b; // Should be 1110111011...
    
    std::vector<uint64_t> inputs = {pattern_a, pattern_b};
    aig.simulate(inputs);
    
    // The result should match the expected OR pattern
    // Node 3 computes !a & !b, so we need to invert to get a | b
    uint64_t computed_result = ~aig.get_sim_value(3);
    ASSERT(computed_result == expected_or);
    
    std::cout << "  Parallel simulation tests completed\n";
}

void test_multi_output_simulation() {
    std::cout << "Testing multi-output simulation...\n";
    
    // Create circuit with multiple outputs: f1 = a & b, f2 = a | b
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 5; // 0=const, 1=a, 2=b, 3=a&b, 4=a|b
    aig.nodes.resize(5);
    
    for (int i = 0; i < 5; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: a & b
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Node 4: a | b = !(!a & !b)
    aig.nodes[4].fanin0 = AIG::var2lit(1, true); // !a
    aig.nodes[4].fanin1 = AIG::var2lit(2, true); // !b
    
    // Two outputs
    aig.pos.push_back(AIG::var2lit(3));        // f1 = a & b
    aig.pos.push_back(AIG::var2lit(4, true));  // f2 = !(~a & ~b) = a | b
    aig.num_pos = 2;
    
    // Test all combinations
    std::cout << "  Testing multi-output truth table...\n";
    
    // Test 00: AND=0, OR=0
    std::vector<uint64_t> inputs = {0x0, 0x0};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0x0); // AND result
    ASSERT(aig.get_sim_value(4) == 0xFFFFFFFFFFFFFFFF); // OR intermediate
    
    // Test 11: AND=1, OR=1  
    inputs = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(3) == 0xFFFFFFFFFFFFFFFF); // AND result
    ASSERT(aig.get_sim_value(4) == 0x0); // OR intermediate (!(!a&!b) when a=1,b=1)
    
    std::cout << "  Multi-output simulation tests completed\n";
}

void test_deep_circuit_simulation() {
    std::cout << "Testing deep circuit simulation...\n";
    
    // Create a deeper circuit: chain of AND gates
    // f = ((a & b) & c) & d
    AIG aig;
    aig.num_pis = 4;
    aig.num_nodes = 7; // 0=const, 1-4=PIs, 5=a&b, 6=(a&b)&c, 7=((a&b)&c)&d
    aig.nodes.resize(7);
    
    for (int i = 0; i < 7; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 5: a & b
    aig.nodes[5].fanin0 = AIG::var2lit(1);
    aig.nodes[5].fanin1 = AIG::var2lit(2);
    
    // Node 6: (a & b) & c
    aig.nodes[6].fanin0 = AIG::var2lit(5);
    aig.nodes[6].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(6));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test that only all-1s gives 1
    std::cout << "  Testing 3-input AND chain...\n";
    
    // Test 111 -> 1
    std::vector<uint64_t> inputs = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 
                                   0xFFFFFFFFFFFFFFFF, 0x0};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(6) == 0xFFFFFFFFFFFFFFFF);
    
    // Test 110 -> 0
    inputs = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0x0, 0x0};
    aig.simulate(inputs);
    ASSERT(aig.get_sim_value(6) == 0x0);
    
    // Test levels are computed correctly
    ASSERT(aig.get_level(5) == 1); // First level after PIs
    ASSERT(aig.get_level(6) == 2); // Second level
    
    std::cout << "  Deep circuit simulation tests completed\n";
}

int main() {
    std::cout << "=== Simulation Detailed Test ===\n\n";
    
    test_basic_simulation();
    test_complex_simulation(); 
    test_parallel_simulation();
    test_multi_output_simulation();
    test_deep_circuit_simulation();
    
    std::cout << "\n=== SIMULATION TEST RESULTS ===\n";
    std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "✅ All tests passed!\n";
        return 0;
    } else {
        std::cout << "❌ Some tests failed!\n";
        return 1;
    }
}