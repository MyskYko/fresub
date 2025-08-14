#include "fresub_aig.hpp"
#include <iostream>

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

void test_aig_basic() {
    std::cout << "Testing AIG basic operations...\n";
    
    // Create empty AIG
    AIG aig;
    ASSERT(aig.num_pis == 0);
    ASSERT(aig.num_pos == 0);
    ASSERT(aig.num_nodes == 1);  // Just constant node
    
    // Manually create a simple AIG
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
    
    // Test utility functions
    ASSERT(AIG::lit2var(6) == 3);
    ASSERT(AIG::lit2var(7) == 3);
    ASSERT(AIG::is_complemented(7) == true);
    ASSERT(AIG::is_complemented(6) == false);
    ASSERT(AIG::complement(6) == 7);
    ASSERT(AIG::complement(7) == 6);
    
    // Test create_and
    int new_node = aig.create_and(AIG::var2lit(1), AIG::var2lit(2));
    ASSERT(AIG::lit2var(new_node) == 4);
    ASSERT(aig.num_nodes == 5);
    
    // Test trivial cases
    ASSERT(aig.create_and(0, AIG::var2lit(1)) == 0);  // 0 AND x = 0
    ASSERT(aig.create_and(1, AIG::var2lit(1)) == AIG::var2lit(1));  // 1 AND x = x
    
    std::cout << "  AIG basic tests completed\n";
}

int main() {
    std::cout << "=== AIG Basic Operations Test ===\n\n";
    
    test_aig_basic();
    
    std::cout << "\nTest Results: ";
    if (passed_tests == total_tests) {
        std::cout << "PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}