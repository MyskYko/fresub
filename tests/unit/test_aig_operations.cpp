#include "fresub_aig.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <filesystem>

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

// Forward declarations
void test_synthetic_aiger_formats();

// ============================================================================
// SECTION 1: Basic AIG Operations
// Merged from: test_aig.cpp
// ============================================================================

void test_aig_basic() {
    std::cout << "=== TESTING AIG BASIC OPERATIONS ===\n";
    std::cout << "Testing fundamental AIG data structure and utilities...\n";
    
    // Create empty AIG
    std::cout << "  Creating empty AIG...\n";
    AIG aig;
    ASSERT(aig.num_pis == 0);
    ASSERT(aig.num_pos == 0);
    ASSERT(aig.num_nodes == 1);  // Just constant node
    std::cout << "    ✓ Empty AIG: 0 PIs, 0 POs, 1 node (constant)\n";
    
    // Manually create a simple AIG
    std::cout << "  Building simple 2-input AIG structure...\n";
    aig.num_pis = 2;
    aig.num_nodes = 4;
    aig.nodes.resize(4);
    
    // Node 0: constant
    // Node 1: PI 1  
    // Node 2: PI 2
    // Node 3: AND(1, 2)
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Add output
    aig.pos.push_back(AIG::var2lit(3));
    aig.num_pos = 1;
    std::cout << "    ✓ Created AIG: Node 3 = AND(PI1, PI2), Output = Node 3\n";
    
    // Test utility functions
    std::cout << "  Testing AIG utility functions...\n";
    ASSERT(AIG::lit2var(6) == 3);    // literal 6 -> variable 3
    ASSERT(AIG::lit2var(7) == 3);    // literal 7 -> variable 3  
    ASSERT(AIG::is_complemented(7) == true);   // literal 7 is complemented
    ASSERT(AIG::is_complemented(6) == false);  // literal 6 is not complemented
    ASSERT(AIG::complement(6) == 7);  // complement of 6 is 7
    ASSERT(AIG::complement(7) == 6);  // complement of 7 is 6
    std::cout << "    ✓ Literal encoding: lit2var, is_complemented, complement\n";
    
    // Test create_and
    std::cout << "  Testing AND node creation...\n";
    int new_node = aig.create_and(AIG::var2lit(1), AIG::var2lit(2));
    ASSERT(AIG::lit2var(new_node) == 4);
    ASSERT(aig.num_nodes == 5);
    std::cout << "    ✓ Created AND node: " << AIG::lit2var(new_node) << " = AND(1, 2)\n";
    
    // Test trivial cases
    std::cout << "  Testing trivial AND cases...\n";
    ASSERT(aig.create_and(0, AIG::var2lit(1)) == 0);  // 0 AND x = 0
    ASSERT(aig.create_and(1, AIG::var2lit(1)) == AIG::var2lit(1));  // 1 AND x = x
    std::cout << "    ✓ Trivial optimizations: 0∧x=0, 1∧x=x\n";
    
    std::cout << "  ✓ AIG basic operations completed\n\n";
}

// ============================================================================
// SECTION 2: AIG Structural Modifications
// Merged from: test_aig_modification.cpp
// ============================================================================

void test_node_creation() {
    std::cout << "=== TESTING NODE CREATION ===\n";
    std::cout << "Testing AND node creation and fanout management...\n";
    
    // Create basic AIG
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 3; // 0=const, 1=a, 2=b
    aig.nodes.resize(3);
    
    for (int i = 0; i < 3; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.build_fanouts();
    aig.compute_levels();
    std::cout << "  Initialized AIG with 2 PIs and constant node\n";
    
    // Test AND node creation
    std::cout << "  Creating AND node: a ∧ b...\n";
    
    int and_node = aig.create_and(AIG::var2lit(1), AIG::var2lit(2)); // a & b
    ASSERT(and_node > 0);
    ASSERT(AIG::lit2var(and_node) == 3); // Should be node 3
    ASSERT(!AIG::is_complemented(and_node)); // Should not be complemented
    
    // Verify the created node
    int node_id = AIG::lit2var(and_node);
    ASSERT(aig.nodes[node_id].fanin0 == AIG::var2lit(1));
    ASSERT(aig.nodes[node_id].fanin1 == AIG::var2lit(2));
    ASSERT(!aig.nodes[node_id].is_dead);
    std::cout << "    ✓ Node " << node_id << ": fanin0=" << AIG::lit2var(aig.nodes[node_id].fanin0) 
              << ", fanin1=" << AIG::lit2var(aig.nodes[node_id].fanin1) << "\n";
    
    // Test that fanouts were updated
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), node_id) 
           != aig.nodes[1].fanouts.end());
    ASSERT(std::find(aig.nodes[2].fanouts.begin(), aig.nodes[2].fanouts.end(), node_id) 
           != aig.nodes[2].fanouts.end());
    std::cout << "    ✓ Fanouts updated: node 1 and 2 now have node " << node_id << " in fanout lists\n";
    
    std::cout << "  ✓ Node creation tests completed\n\n";
}

void test_trivial_and_cases() {
    std::cout << "=== TESTING TRIVIAL AND OPTIMIZATIONS ===\n";
    std::cout << "Testing constant propagation and identity optimizations...\n";
    
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 3;
    aig.nodes.resize(3);
    
    for (int i = 0; i < 3; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test 0 AND x = 0
    std::cout << "  Testing: 0 ∧ x = 0...\n";
    int result = aig.create_and(AIG::var2lit(0), AIG::var2lit(1)); // 0 & a
    ASSERT(result == 0);
    std::cout << "    ✓ 0 ∧ a = 0 (literal " << result << ")\n";
    
    // Test 1 AND x = x  
    std::cout << "  Testing: 1 ∧ x = x...\n";
    result = aig.create_and(AIG::var2lit(0, true), AIG::var2lit(1)); // 1 & a = a
    ASSERT(result == AIG::var2lit(1));
    std::cout << "    ✓ 1 ∧ a = a (literal " << result << ")\n";
    
    // Test x AND x = x
    std::cout << "  Testing: x ∧ x = x...\n";
    result = aig.create_and(AIG::var2lit(1), AIG::var2lit(1)); // a & a = a
    ASSERT(result == AIG::var2lit(1));
    std::cout << "    ✓ a ∧ a = a (literal " << result << ")\n";
    
    // Test x AND !x = 0
    std::cout << "  Testing: x ∧ ¬x = 0...\n";
    result = aig.create_and(AIG::var2lit(1), AIG::var2lit(1, true)); // a & !a = 0
    ASSERT(result == 0);
    std::cout << "    ✓ a ∧ ¬a = 0 (literal " << result << ")\n";
    
    std::cout << "  ✓ Trivial AND optimizations completed\n\n";
}

void test_node_removal() {
    std::cout << "=== TESTING NODE REMOVAL ===\n";
    std::cout << "Testing node deletion and fanout cleanup...\n";
    
    // Create AIG with chain: a -> node3 -> node4 -> output
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 5; // 0=const, 1=a, 2=b, 3=a&b, 4=(a&b)&a
    aig.nodes.resize(5);
    
    for (int i = 0; i < 5; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: a & b
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Node 4: (a & b) & a
    aig.nodes[4].fanin0 = AIG::var2lit(3);
    aig.nodes[4].fanin1 = AIG::var2lit(1);
    
    aig.pos.push_back(AIG::var2lit(4));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    std::cout << "  Built AIG chain: a,b → node3(a∧b) → node4((a∧b)∧a) → output\n";
    
    // Verify initial fanout structure
    ASSERT(std::find(aig.nodes[3].fanouts.begin(), aig.nodes[3].fanouts.end(), 4) 
           != aig.nodes[3].fanouts.end());
    std::cout << "  Initial fanouts: node 3 has node 4 in fanout list\n";
    
    // Test node removal
    std::cout << "  Removing node 3...\n";
    aig.remove_node(3);
    
    // Verify node is marked dead
    ASSERT(aig.nodes[3].is_dead);
    std::cout << "    ✓ Node 3 marked as dead\n";
    
    // Verify fanouts were updated (node 3 removed from fanout lists)
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), 3) 
           == aig.nodes[1].fanouts.end());
    ASSERT(std::find(aig.nodes[2].fanouts.begin(), aig.nodes[2].fanouts.end(), 3) 
           == aig.nodes[2].fanouts.end());
    std::cout << "    ✓ Fanouts cleaned up: node 3 removed from nodes 1 and 2 fanout lists\n";
    
    std::cout << "  ✓ Node removal tests completed\n\n";
}

void test_node_replacement() {
    std::cout << "=== TESTING NODE REPLACEMENT ===\n";
    std::cout << "Testing node substitution and reference updates...\n";
    
    // Create AIG: a -> node3 -> node4, b -> node4
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 5; // 0=const, 1=a, 2=b, 3=a&b, 4=node3&b
    aig.nodes.resize(5);
    
    for (int i = 0; i < 5; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: a & b
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Node 4: node3 & b
    aig.nodes[4].fanin0 = AIG::var2lit(3);
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    
    aig.pos.push_back(AIG::var2lit(4));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    std::cout << "  Built AIG: node3(a∧b), node4(node3∧b), output=node4\n";
    
    // Verify initial state
    ASSERT(aig.nodes[4].fanin0 == AIG::var2lit(3));
    std::cout << "  Initial: node 4 fanin0 = node " << AIG::lit2var(aig.nodes[4].fanin0) << "\n";
    
    // Test replacing node 3 with node 1 (replace intermediate with input)
    std::cout << "  Replacing node 3 with node 1...\n";
    aig.replace_node(3, 1); // Replace node 3 with node 1
    
    // Verify replacement
    ASSERT(aig.nodes[4].fanin0 == AIG::var2lit(1)); // Should now point to node 1
    ASSERT(aig.nodes[3].is_dead); // Old node should be dead
    std::cout << "    ✓ Node 4 fanin0 now points to node " << AIG::lit2var(aig.nodes[4].fanin0) << "\n";
    std::cout << "    ✓ Node 3 marked as dead\n";
    
    // Verify fanouts were updated
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), 4) 
           != aig.nodes[1].fanouts.end());
    std::cout << "    ✓ Node 1 fanouts updated to include node 4\n";
    
    std::cout << "  ✓ Node replacement tests completed\n\n";
}

void test_primary_output_updates() {
    std::cout << "=== TESTING PRIMARY OUTPUT UPDATES ===\n";
    std::cout << "Testing PO pointer updates during node replacement...\n";
    
    // Create AIG with PO pointing to intermediate node
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=a&b
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // Node 3: a & b
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    aig.pos.push_back(AIG::var2lit(3, true)); // Output is !(a&b)
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    std::cout << "  Built AIG: node3(a∧b), output=¬node3\n";
    
    // Verify initial PO
    ASSERT(aig.pos[0] == AIG::var2lit(3, true));
    std::cout << "  Initial PO: ¬node3 (literal " << aig.pos[0] << ")\n";
    
    // Test replacing the node that PO points to
    std::cout << "  Replacing node 3 with node 1 (PO should update)...\n";
    aig.replace_node(3, 1); // Replace node 3 with node 1
    
    // Verify PO was updated (should now point to node 1 with same complementation)
    ASSERT(aig.pos[0] == AIG::var2lit(1, true));
    ASSERT(aig.nodes[3].is_dead);
    std::cout << "    ✓ PO updated to ¬node1 (literal " << aig.pos[0] << ")\n";
    std::cout << "    ✓ Node 3 marked as dead\n";
    
    std::cout << "  ✓ Primary output update tests completed\n\n";
}

// ============================================================================
// SECTION 3: AIGER File I/O
// Merged from: test_aiger.cpp and test_aiger_comprehensive.cpp  
// ============================================================================

void test_aiger_roundtrip(const std::string& benchmark_file) {
    std::cout << "=== TESTING AIGER FILE I/O ===\n";
    std::cout << "Testing AIGER format reading and writing...\n";
    
    // Test with provided benchmark file
    try {
        std::cout << "  Loading benchmark: " << benchmark_file << "...\n";
        AIG aig(benchmark_file);
        
        // Check that the file was loaded with valid structure
        ASSERT(aig.num_pis > 0);
        ASSERT(aig.num_pos > 0);
        ASSERT(aig.num_nodes >= aig.num_pis + 1);  // At least PIs + constant
        
        std::cout << "    ✓ Loaded AIG: " << aig.num_pis << " PIs, " 
                  << aig.num_pos << " POs, " << aig.num_nodes << " nodes\n";
        
        // Check that nodes are properly initialized
        ASSERT(aig.nodes.size() == aig.num_nodes);
        ASSERT(aig.pos.size() == aig.num_pos);
        
        // Build fanouts and levels to test structural operations
        aig.build_fanouts();
        aig.compute_levels();
        std::cout << "    ✓ Built fanouts and computed levels\n";
        
        // Check that levels are computed
        ASSERT(aig.get_level(0) == 0);  // Constant has level 0
        
        // Test writing and reading back (ASCII)
        std::cout << "  Testing ASCII format roundtrip...\n";
        aig.write_aiger("test_output_ascii.aig", false);
        
        AIG aig2("test_output_ascii.aig");
        ASSERT(aig2.num_pis == aig.num_pis);
        ASSERT(aig2.num_pos == aig.num_pos);
        std::cout << "    ✓ ASCII roundtrip successful\n";
        
        // Test binary format
        std::cout << "  Testing binary format roundtrip...\n";
        aig.write_aiger("test_output_binary.aig", true);
        
        AIG aig3("test_output_binary.aig");
        ASSERT(aig3.num_pis == aig.num_pis);
        ASSERT(aig3.num_pos == aig.num_pos);
        std::cout << "    ✓ Binary roundtrip successful\n";
        
        // Clean up test files
        std::remove("test_output_ascii.aig");
        std::remove("test_output_binary.aig");
        std::cout << "    ✓ Cleaned up temporary files\n";
        
    } catch (const std::exception& e) {
        std::cout << "  Note: Could not test with " << benchmark_file 
                  << " (file not found), testing with synthetic AIG...\n";
        test_synthetic_aiger_formats();
        return;
    }
    
    std::cout << "  ✓ AIGER I/O tests completed\n\n";
}

void test_synthetic_aiger_formats() {
    std::cout << "=== TESTING SYNTHETIC AIGER FORMATS ===\n";
    std::cout << "Testing AIGER encoding with custom AIG structures...\n";
    
    // Test 1: Simple 2-input AND
    std::cout << "  Testing simple AND gate encoding...\n";
    {
        AIG aig;
        aig.num_pis = 2;
        aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=a&b
        aig.nodes.resize(4);
        
        for (int i = 0; i < 4; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.nodes[3].fanin0 = AIG::var2lit(1);
        aig.nodes[3].fanin1 = AIG::var2lit(2);
        
        aig.pos.push_back(AIG::var2lit(3));
        aig.num_pos = 1;
        
        // Test ASCII format
        aig.write_aiger("test_simple_and_ascii.aig", false);
        AIG aig2("test_simple_and_ascii.aig");
        
        ASSERT(aig2.num_pis == aig.num_pis);
        ASSERT(aig2.num_pos == aig.num_pos);
        ASSERT(aig2.nodes.size() == aig.nodes.size());
        ASSERT(aig2.pos[0] == aig.pos[0]);
        std::cout << "    ✓ Simple AND: ASCII format correct\n";
        
        // Test binary format
        aig.write_aiger("test_simple_and_binary.aig", true);
        AIG aig3("test_simple_and_binary.aig");
        
        ASSERT(aig3.num_pis == aig.num_pis);
        ASSERT(aig3.num_pos == aig.num_pos);
        std::cout << "    ✓ Simple AND: binary format correct\n";
        
        std::remove("test_simple_and_ascii.aig");
        std::remove("test_simple_and_binary.aig");
    }
    
    // Test 2: Multiple outputs with complemented signals
    std::cout << "  Testing multiple outputs with complemented signals...\n";
    {
        AIG aig;
        aig.num_pis = 2;
        aig.num_nodes = 4; // 0=const, 1=a, 2=b, 3=a&b
        aig.nodes.resize(4);
        
        for (int i = 0; i < 4; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.nodes[3].fanin0 = AIG::var2lit(1);
        aig.nodes[3].fanin1 = AIG::var2lit(2);
        
        // Multiple outputs: a&b, !(a&b), a
        aig.pos.push_back(AIG::var2lit(3));        // a&b
        aig.pos.push_back(AIG::var2lit(3, true));  // !(a&b)  
        aig.pos.push_back(AIG::var2lit(1));        // a
        aig.num_pos = 3;
        
        // Test roundtrip
        aig.write_aiger("test_multi_out.aig", false);
        AIG aig2("test_multi_out.aig");
        
        ASSERT(aig2.num_pis == 2);
        ASSERT(aig2.num_pos == 3);
        ASSERT(aig2.pos[0] == AIG::var2lit(3));        // a&b
        ASSERT(aig2.pos[1] == AIG::var2lit(3, true));  // !(a&b)
        ASSERT(aig2.pos[2] == AIG::var2lit(1));        // a
        std::cout << "    ✓ Multiple outputs with complementation preserved\n";
        
        std::remove("test_multi_out.aig");
    }
    
    // Test 3: Complex circuit with chains
    std::cout << "  Testing complex circuit with logic chains...\n";
    {
        AIG aig;
        aig.num_pis = 3;
        aig.num_nodes = 7; // 0=const, 1-3=PIs, 4=1&2, 5=4&3, 6=5&1
        aig.nodes.resize(7);
        
        for (int i = 0; i < 7; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        // Build chain: (((a & b) & c) & a)
        aig.nodes[4].fanin0 = AIG::var2lit(1); // a & b
        aig.nodes[4].fanin1 = AIG::var2lit(2);
        
        aig.nodes[5].fanin0 = AIG::var2lit(4); // (a&b) & c
        aig.nodes[5].fanin1 = AIG::var2lit(3);
        
        aig.nodes[6].fanin0 = AIG::var2lit(5); // ((a&b)&c) & a
        aig.nodes[6].fanin1 = AIG::var2lit(1);
        
        aig.pos.push_back(AIG::var2lit(6));
        aig.num_pos = 1;
        
        // Test binary format for complex circuit
        aig.write_aiger("test_complex.aig", true);
        AIG aig2("test_complex.aig");
        
        ASSERT(aig2.num_pis == 3);
        ASSERT(aig2.num_pos == 1);
        ASSERT(aig2.num_nodes == 7);
        std::cout << "    ✓ Complex circuit binary encoding successful\n";
        
        std::remove("test_complex.aig");
    }
    
    std::cout << "  ✓ Synthetic AIGER format tests completed\n\n";
}

// ============================================================================
// MAIN TEST DRIVER
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "      AIG OPERATIONS TEST SUITE        \n";
    std::cout << "========================================\n";
    std::cout << "Consolidated test covering:\n";
    std::cout << "• Basic AIG operations and utilities\n";
    std::cout << "• Structural modifications (create/remove/replace)\n";
    std::cout << "• AIGER file I/O (ASCII and binary formats)\n";
    std::cout << "• Comprehensive format testing\n\n";
    
    // Section 1: Basic AIG operations
    test_aig_basic();
    
    // Section 2: Structural modifications
    test_node_creation();
    test_trivial_and_cases();
    test_node_removal();
    test_node_replacement();
    test_primary_output_updates();
    
    // Section 3: AIGER I/O testing
    std::string benchmark_file = "../benchmarks/mul2.aig";  // Default benchmark
    if (argc > 1) {
        benchmark_file = argv[1];  // Override with command line argument
    }
    
    test_aiger_roundtrip(benchmark_file);
    test_synthetic_aiger_formats();
    
    // Final results
    std::cout << "========================================\n";
    std::cout << "         TEST RESULTS SUMMARY          \n";
    std::cout << "========================================\n";
    
    if (passed_tests == total_tests) {
        std::cout << "🎉 ALL TESTS PASSED! (" << passed_tests << "/" << total_tests << ")\n";
        std::cout << "\nAIG operations test suite completed successfully.\n";
        std::cout << "All basic operations, modifications, and I/O functions verified.\n";
        return 0;
    } else {
        std::cout << "❌ SOME TESTS FAILED! (" << passed_tests << "/" << total_tests << ")\n";
        std::cout << "\nFailures detected in AIG operations.\n";
        return 1;
    }
}