#include "../include/aig.hpp"
#include "../include/window.hpp" 
#include "../include/resub.hpp"
#include <iostream>
#include <cassert>

extern int total_tests;
extern int passed_tests;

#define ASSERT(cond) do { \
    total_tests++; \
    if (!(cond)) { \
        std::cerr << "Test failed: " << #cond << std::endl; \
    } else { \
        passed_tests++; \
    } \
} while(0)

using namespace fresub;

void test_simple_synthesis() {
    std::cout << "Testing simple synthesis...\n";
    
    // Create AIG with basic structure for synthesis testing
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 6; // 0=const, 1-3=PIs, 4=1&2, 5=4&3
    aig.nodes.resize(6);
    
    for (int i = 0; i < 6; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 4: input1 & input2
    aig.nodes[4].fanin0 = AIG::var2lit(1);
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    
    // Node 5: node4 & input3  
    aig.nodes[5].fanin0 = AIG::var2lit(4);
    aig.nodes[5].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(5));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Create window around node 5
    Window window;
    window.root = 5;
    window.inputs = {1, 2, 3}; // Primary inputs
    window.divisors = {4}; // Intermediate node as divisor
    
    // Test synthesis with simple AND function
    std::cout << "  Testing AND function synthesis...\n";
    
    TruthTable target_and3(3);
    target_and3.set_bit(7, true); // 111 -> 1 (3-input AND)
    
    std::vector<int> selected_divisors = {0}; // Use divisor 4 (1&2)
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_and3);
    ASSERT(result >= 0); // Should succeed or fail gracefully
    
    std::cout << "  Simple synthesis tests completed\n";
}

void test_or_synthesis() {
    std::cout << "Testing OR synthesis...\n";
    
    // Create AIG for OR synthesis testing
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1-2=PIs, 3=placeholder
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.pos.push_back(AIG::var2lit(3));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Create window
    Window window;
    window.root = 3;
    window.inputs = {1, 2}; // Primary inputs
    window.divisors = {}; // No intermediate divisors
    
    // Test OR synthesis: f = a | b
    std::cout << "  Testing OR function synthesis...\n";
    
    TruthTable target_or(2);
    target_or.set_bit(1, true); // 01 -> 1
    target_or.set_bit(2, true); // 10 -> 1  
    target_or.set_bit(3, true); // 11 -> 1
    
    std::vector<int> selected_divisors = {}; // No divisors, build from inputs
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_or);
    ASSERT(result >= 0);
    
    std::cout << "  OR synthesis tests completed\n";
}

void test_xor_synthesis() {
    std::cout << "Testing XOR synthesis...\n";
    
    // Create AIG with potential divisors for XOR
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 6; // 0=const, 1-2=PIs, 3=1&2, 4=!(1&2), 5=result
    aig.nodes.resize(6);
    
    for (int i = 0; i < 6; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: a & b
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Node 4: !(a & b) - intermediate for building XOR
    aig.nodes[4].fanin0 = AIG::var2lit(3, true);
    aig.nodes[4].fanin1 = AIG::var2lit(0); // AND with constant 1
    
    aig.pos.push_back(AIG::var2lit(5));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Create window with useful divisors for XOR
    Window window;
    window.root = 5;
    window.inputs = {1, 2};
    window.divisors = {3, 4}; // a&b and !(a&b)
    
    // Test XOR synthesis: f = a XOR b
    std::cout << "  Testing XOR function synthesis...\n";
    
    TruthTable target_xor(2);
    target_xor.set_bit(1, true); // 01 -> 1
    target_xor.set_bit(2, true); // 10 -> 1
    
    std::vector<int> selected_divisors = {1}; // Use !(a&b) divisor
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_xor);
    ASSERT(result >= 0);
    
    std::cout << "  XOR synthesis tests completed\n";
}

void test_majority_synthesis() {
    std::cout << "Testing majority function synthesis...\n";
    
    // Create AIG with 3 inputs for majority function
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 8; // 0=const, 1-3=PIs, 4=1&2, 5=2&3, 6=1&3, 7=result
    aig.nodes.resize(8);
    
    for (int i = 0; i < 8; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Create pairwise AND divisors
    aig.nodes[4].fanin0 = AIG::var2lit(1); // a & b
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    
    aig.nodes[5].fanin0 = AIG::var2lit(2); // b & c
    aig.nodes[5].fanin1 = AIG::var2lit(3);
    
    aig.nodes[6].fanin0 = AIG::var2lit(1); // a & c
    aig.nodes[6].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(7));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Create window with all pairwise ANDs as divisors
    Window window;
    window.root = 7;
    window.inputs = {1, 2, 3};
    window.divisors = {4, 5, 6}; // All pairwise ANDs
    
    // Test majority synthesis: MAJ(a,b,c) = (a&b) | (b&c) | (a&c)
    std::cout << "  Testing majority function synthesis...\n";
    
    TruthTable target_maj(3);
    target_maj.set_bit(3, true); // 011 -> 1
    target_maj.set_bit(5, true); // 101 -> 1
    target_maj.set_bit(6, true); // 110 -> 1  
    target_maj.set_bit(7, true); // 111 -> 1
    
    std::vector<int> selected_divisors = {0, 1, 2}; // Use all pairwise ANDs
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_maj);
    ASSERT(result >= 0);
    
    std::cout << "  Majority synthesis tests completed\n";
}

void test_constant_synthesis() {
    std::cout << "Testing constant function synthesis...\n";
    
    // Create minimal AIG for constant testing
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1-2=PIs, 3=result
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.pos.push_back(AIG::var2lit(3));
    aig.num_pos = 1;
    
    // Test constant 0 synthesis
    std::cout << "  Testing constant 0 synthesis...\n";
    
    Window window;
    window.root = 3;
    window.inputs = {1, 2};
    window.divisors = {};
    
    TruthTable target_const0(2);
    // All bits remain 0 (constant 0)
    
    std::vector<int> selected_divisors = {};
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_const0);
    ASSERT(result >= 0);
    
    // Test constant 1 synthesis
    std::cout << "  Testing constant 1 synthesis...\n";
    
    TruthTable target_const1(2);
    target_const1.set_bit(0, true);
    target_const1.set_bit(1, true);
    target_const1.set_bit(2, true);
    target_const1.set_bit(3, true);
    
    result = synthesize_with_divisors(aig, window, selected_divisors, target_const1);
    ASSERT(result >= 0);
    
    std::cout << "  Constant synthesis tests completed\n";
}

void test_complex_synthesis() {
    std::cout << "Testing complex synthesis scenarios...\n";
    
    // Create AIG with many potential divisors
    AIG aig;
    aig.num_pis = 4;
    aig.num_nodes = 10; // 0=const, 1-4=PIs, 5-8=intermediates, 9=result
    aig.nodes.resize(10);
    
    for (int i = 0; i < 10; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Create various intermediate functions as potential divisors
    aig.nodes[5].fanin0 = AIG::var2lit(1); // a & b
    aig.nodes[5].fanin1 = AIG::var2lit(2);
    
    aig.nodes[6].fanin0 = AIG::var2lit(3); // c & d  
    aig.nodes[6].fanin1 = AIG::var2lit(4);
    
    aig.nodes[7].fanin0 = AIG::var2lit(1, true); // !a & !b
    aig.nodes[7].fanin1 = AIG::var2lit(2, true);
    
    aig.nodes[8].fanin0 = AIG::var2lit(5, true); // !(a & b) 
    aig.nodes[8].fanin1 = AIG::var2lit(0);
    
    aig.pos.push_back(AIG::var2lit(9));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test synthesis with multiple available divisors
    std::cout << "  Testing multi-divisor synthesis...\n";
    
    Window window;
    window.root = 9;
    window.inputs = {1, 2, 3, 4};
    window.divisors = {5, 6, 7, 8}; // All intermediate functions
    
    // Target: (a&b) | (c&d)
    TruthTable target_complex(4);
    // Set bits where (a&b)=1 or (c&d)=1
    for (int i = 0; i < 16; i++) {
        bool a = (i & 8) != 0;
        bool b = (i & 4) != 0;  
        bool c = (i & 2) != 0;
        bool d = (i & 1) != 0;
        
        if ((a && b) || (c && d)) {
            target_complex.set_bit(i, true);
        }
    }
    
    std::vector<int> selected_divisors = {0, 1}; // Use (a&b) and (c&d)
    
    int result = synthesize_with_divisors(aig, window, selected_divisors, target_complex);
    ASSERT(result >= 0);
    
    std::cout << "  Complex synthesis tests completed\n";
}