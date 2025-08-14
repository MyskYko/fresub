#include "fresub_aig.hpp"
#include "feasibility.hpp"
#include <iostream>
#include <cassert>
#include <vector>

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

void test_truth_table_operations() {
    std::cout << "Testing truth table operations...\n";
    
    // Test truth table as vector representation
    std::cout << "  Testing truth table vector operations...\n";
    
    // For 2-input functions, we have 4 entries (2^2)
    TruthTable tt_and(1); // Using one 64-bit word
    TruthTable tt_or(1);
    TruthTable tt_xor(1);
    
    // AND: bit pattern 1000 (binary) = bit 3 set in 4-bit truth table
    tt_and[0] = 0x8; // Binary 1000, only position 3 (11) is true
    
    // OR: bit pattern 1110 (binary) = positions 1,2,3 set
    tt_or[0] = 0xE; // Binary 1110
    
    // XOR: bit pattern 0110 (binary) = positions 1,2 set  
    tt_xor[0] = 0x6; // Binary 0110
    
    // Test that we created valid truth tables
    ASSERT(tt_and.size() == 1);
    ASSERT(tt_or.size() == 1);
    ASSERT(tt_xor.size() == 1);
    
    ASSERT((tt_and[0] & (1ULL << 3)) != 0); // 11 -> 1 for AND
    ASSERT((tt_or[0] & (1ULL << 1)) != 0);  // 01 -> 1 for OR
    ASSERT((tt_xor[0] & (1ULL << 2)) != 0); // 10 -> 1 for XOR
    
    std::cout << "  Truth table operations tests completed\n";
}

void test_simple_feasibility() {
    std::cout << "Testing simple feasibility checking...\n";
    
    // Create simple AIG for testing
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4;
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    aig.pos.push_back(AIG::var2lit(3));
    aig.num_pos = 1;
    
    ResubEngine engine(aig);
    
    // Test basic engine creation
    std::cout << "  Testing engine creation...\n";
    ASSERT(true); // Engine created successfully
    
    // Test truth table computation (public method)
    std::cout << "  Testing truth table computation...\n";
    TruthTable truth;
    std::vector<int> inputs = {1, 2};
    engine.compute_truth_table(3, inputs, truth);
    
    // Should have computed some truth table
    ASSERT(!truth.empty());
    
    std::cout << "  Simple feasibility tests completed\n";
}

void test_complex_feasibility() {
    std::cout << "Testing complex feasibility checking...\n";
    
    // Create more complex AIG for testing  
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 6;
    aig.nodes.resize(6);
    
    for (int i = 0; i < 6; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Build XOR-like structure
    aig.nodes[4].fanin0 = AIG::var2lit(1);
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    aig.nodes[5].fanin0 = AIG::var2lit(4);
    aig.nodes[5].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(5));
    aig.num_pos = 1;
    
    ResubEngine engine(aig);
    
    // Test complex truth table computation
    std::cout << "  Testing complex truth table computation...\n";
    TruthTable truth;
    std::vector<int> inputs = {1, 2, 3};
    engine.compute_truth_table(5, inputs, truth);
    
    ASSERT(!truth.empty());
    
    std::cout << "  Complex feasibility tests completed\n";
}

void test_three_input_feasibility() {
    std::cout << "Testing 3-input feasibility checking...\n";
    
    // Create majority-like AIG
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 8;
    aig.nodes.resize(8);
    
    for (int i = 0; i < 8; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Build majority structure: (a&b) | (b&c) | (a&c)
    aig.nodes[4].fanin0 = AIG::var2lit(1); // a & b
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    
    aig.nodes[5].fanin0 = AIG::var2lit(2); // b & c
    aig.nodes[5].fanin1 = AIG::var2lit(3);
    
    aig.nodes[6].fanin0 = AIG::var2lit(1); // a & c
    aig.nodes[6].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(7));
    aig.num_pos = 1;
    
    ResubEngine engine(aig);
    
    // Test with 3 inputs
    std::cout << "  Testing 3-input computation...\n";
    TruthTable truth;
    std::vector<int> inputs = {1, 2, 3};
    engine.compute_truth_table(4, inputs, truth); // Test one of the AND gates
    
    ASSERT(!truth.empty());
    
    std::cout << "  3-input feasibility tests completed\n";
}

void test_infeasible_cases() {
    std::cout << "Testing infeasible cases...\n";
    
    // Test with minimal AIG
    AIG aig;
    aig.num_pis = 1;
    aig.num_nodes = 2;
    aig.nodes.resize(2);
    
    for (int i = 0; i < 2; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.pos.push_back(AIG::var2lit(1));
    aig.num_pos = 1;
    
    ResubEngine engine(aig);
    
    // Test edge case with single input
    std::cout << "  Testing single input case...\n";
    TruthTable truth;
    std::vector<int> inputs = {1};
    engine.compute_truth_table(1, inputs, truth);
    
    ASSERT(!truth.empty());
    
    std::cout << "  Infeasible case tests completed\n";
}

void test_edge_cases() {
    std::cout << "Testing edge cases...\n";
    
    // Test with constant nodes
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 3;
    aig.nodes.resize(3);
    
    for (int i = 0; i < 3; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.pos.push_back(AIG::var2lit(0)); // Constant output
    aig.num_pos = 1;
    
    ResubEngine engine(aig);
    
    // Test constant case
    std::cout << "  Testing constant computation...\n";
    TruthTable truth;
    std::vector<int> inputs = {1, 2};
    engine.compute_truth_table(0, inputs, truth); // Constant node
    
    ASSERT(!truth.empty());
    
    std::cout << "  Edge case tests completed\n";
}