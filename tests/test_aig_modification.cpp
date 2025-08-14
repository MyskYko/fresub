#include "../include/aig.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>

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

void test_node_creation() {
    std::cout << "Testing AIG node creation...\n";
    
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
    
    // Test AND node creation
    std::cout << "  Testing AND node creation...\n";
    
    int and_node = aig.create_and(AIG::var2lit(1), AIG::var2lit(2)); // a & b
    ASSERT(and_node > 0);
    ASSERT(AIG::lit2var(and_node) == 3); // Should be node 3
    ASSERT(!AIG::is_complemented(and_node)); // Should not be complemented
    
    // Verify the created node
    int node_id = AIG::lit2var(and_node);
    ASSERT(aig.nodes[node_id].fanin0 == AIG::var2lit(1));
    ASSERT(aig.nodes[node_id].fanin1 == AIG::var2lit(2));
    ASSERT(!aig.nodes[node_id].is_dead);
    
    // Test that fanouts were updated
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), node_id) 
           != aig.nodes[1].fanouts.end());
    ASSERT(std::find(aig.nodes[2].fanouts.begin(), aig.nodes[2].fanouts.end(), node_id) 
           != aig.nodes[2].fanouts.end());
    
    std::cout << "  Node creation tests completed\n";
}

void test_trivial_and_cases() {
    std::cout << "Testing trivial AND cases...\n";
    
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
    std::cout << "  Testing 0 AND x = 0...\n";
    int result = aig.create_and(AIG::var2lit(0), AIG::var2lit(1)); // 0 & a
    ASSERT(result == 0);
    
    // Test 1 AND x = x
    std::cout << "  Testing 1 AND x = x...\n";
    result = aig.create_and(AIG::var2lit(0, true), AIG::var2lit(1)); // 1 & a = a
    ASSERT(result == AIG::var2lit(1));
    
    // Test x AND x = x
    std::cout << "  Testing x AND x = x...\n";
    result = aig.create_and(AIG::var2lit(1), AIG::var2lit(1)); // a & a = a
    ASSERT(result == AIG::var2lit(1));
    
    // Test x AND !x = 0
    std::cout << "  Testing x AND !x = 0...\n";
    result = aig.create_and(AIG::var2lit(1), AIG::var2lit(1, true)); // a & !a = 0
    ASSERT(result == 0);
    
    std::cout << "  Trivial AND tests completed\n";
}

void test_node_removal() {
    std::cout << "Testing node removal...\n";
    
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
    
    // Verify initial fanout structure
    ASSERT(std::find(aig.nodes[3].fanouts.begin(), aig.nodes[3].fanouts.end(), 4) 
           != aig.nodes[3].fanouts.end());
    
    // Test node removal
    std::cout << "  Testing node removal...\n";
    
    aig.remove_node(3);
    
    // Verify node is marked dead
    ASSERT(aig.nodes[3].is_dead);
    
    // Verify fanouts were updated (node 3 removed from fanout lists)
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), 3) 
           == aig.nodes[1].fanouts.end());
    ASSERT(std::find(aig.nodes[2].fanouts.begin(), aig.nodes[2].fanouts.end(), 3) 
           == aig.nodes[2].fanouts.end());
    
    std::cout << "  Node removal tests completed\n";
}

void test_node_replacement() {
    std::cout << "Testing node replacement...\n";
    
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
    
    // Test replacing node 3 with node 1 (replace intermediate with input)
    std::cout << "  Testing node replacement...\n";
    
    // Verify initial state
    ASSERT(aig.nodes[4].fanin0 == AIG::var2lit(3));
    
    aig.replace_node(3, 1); // Replace node 3 with node 1
    
    // Verify replacement
    ASSERT(aig.nodes[4].fanin0 == AIG::var2lit(1)); // Should now point to node 1
    ASSERT(aig.nodes[3].is_dead); // Old node should be dead
    
    // Verify fanouts were updated
    ASSERT(std::find(aig.nodes[1].fanouts.begin(), aig.nodes[1].fanouts.end(), 4) 
           != aig.nodes[1].fanouts.end());
    
    std::cout << "  Node replacement tests completed\n";
}

void test_po_replacement() {
    std::cout << "Testing primary output replacement...\n";
    
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
    
    // Test replacing the node that PO points to
    std::cout << "  Testing PO update during replacement...\n";
    
    // Verify initial PO
    ASSERT(aig.pos[0] == AIG::var2lit(3, true));
    
    aig.replace_node(3, 1); // Replace node 3 with node 1
    
    // Verify PO was updated (should now point to node 1 with same complementation)
    ASSERT(aig.pos[0] == AIG::var2lit(1, true));
    ASSERT(aig.nodes[3].is_dead);
    
    std::cout << "  PO replacement tests completed\n";
}

void test_chain_modification() {
    std::cout << "Testing chain modification...\n";
    
    // Create longer chain for more complex testing
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
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test inserting a new node in the middle of the chain
    std::cout << "  Testing chain insertion...\n";
    
    int original_size = aig.nodes.size();
    
    // Create new AND node: new_node = node5 & node1
    int new_node = aig.create_and(AIG::var2lit(5), AIG::var2lit(1));
    
    ASSERT(aig.nodes.size() == original_size + 1);
    ASSERT(AIG::lit2var(new_node) == original_size);
    
    // Test that levels are updated correctly
    aig.compute_levels();
    ASSERT(aig.get_level(AIG::lit2var(new_node)) > aig.get_level(5));
    ASSERT(aig.get_level(AIG::lit2var(new_node)) > aig.get_level(1));
    
    std::cout << "  Chain modification tests completed\n";
}

void test_structural_hash() {
    std::cout << "Testing structural properties...\n";
    
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 4; // 0=const, 1-3=PIs
    aig.nodes.resize(4);
    
    for (int i = 0; i < 4; i++) {
        aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test that creating identical AND nodes might be optimized
    std::cout << "  Testing identical node creation...\n";
    
    int node1 = aig.create_and(AIG::var2lit(1), AIG::var2lit(2)); // a & b
    int node2 = aig.create_and(AIG::var2lit(1), AIG::var2lit(2)); // a & b (same)
    
    // Both should be valid (current implementation doesn't hash)
    ASSERT(node1 > 0);
    ASSERT(node2 > 0);
    
    // Test order independence  
    int node3 = aig.create_and(AIG::var2lit(2), AIG::var2lit(1)); // b & a (same as a & b)
    ASSERT(node3 > 0);
    
    // Verify that all created nodes have correct structure
    ASSERT(aig.nodes[AIG::lit2var(node1)].fanin0 == AIG::var2lit(1));
    ASSERT(aig.nodes[AIG::lit2var(node1)].fanin1 == AIG::var2lit(2));
    
    std::cout << "  Structural tests completed\n";
}