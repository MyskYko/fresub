#include "../include/aig.hpp"
#include <iostream>
#include <fstream>

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

void test_aiger_read() {
    std::cout << "Testing AIGER file reading...\n";
    
    // Test with i10.aig benchmark
    try {
        std::cout << "  Loading i10.aig...\n";
        AIG aig("../benchmarks/i10.aig");
        
        // Check expected values from the header: aig 2932 257 0 224 2675
        ASSERT(aig.num_pis == 257);
        ASSERT(aig.num_pos == 224);
        ASSERT(aig.num_nodes == 2933);  // 2932 + 1 for constant
        
        // Check that nodes are properly initialized
        ASSERT(aig.nodes.size() == 2933);
        ASSERT(aig.pos.size() == 224);
        
        // Check that we can access nodes without crashing
        for (int i = 0; i < aig.num_pis + 1; i++) {
            ASSERT(!aig.nodes[i].is_dead);
        }
        
        // Build fanouts and levels to test structural operations
        aig.build_fanouts();
        aig.compute_levels();
        
        // Check that levels are computed
        ASSERT(aig.get_level(0) == 0);  // Constant has level 0
        
        // Test writing and reading back (ASCII)
        std::cout << "  Writing to test_output_ascii.aig...\n";
        aig.write_aiger("test_output_ascii.aig", false);
        
        std::cout << "  Reading back test_output_ascii.aig...\n";
        AIG aig2("test_output_ascii.aig");
        ASSERT(aig2.num_pis == aig.num_pis);
        ASSERT(aig2.num_pos == aig.num_pos);
        
        // Test binary format
        std::cout << "  Writing to test_output_binary.aig (binary)...\n";
        aig.write_aiger("test_output_binary.aig", true);
        
        std::cout << "  Reading back test_output_binary.aig...\n";
        AIG aig3("test_output_binary.aig");
        ASSERT(aig3.num_pis == aig.num_pis);
        ASSERT(aig3.num_pos == aig.num_pos);
        // Note: num_nodes might differ if dead nodes are removed during write
        
        std::cout << "  AIGER read/write test completed\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during AIGER test: " << e.what() << std::endl;
        ASSERT(false);  // Mark test as failed
    }
}

// Also test creating a small AIG and writing it
void test_aiger_write() {
    std::cout << "Testing AIGER file writing...\n";
    
    try {
        // Create a simple AIG: f = (a AND b) OR c
        AIG aig;
        aig.num_pis = 3;
        aig.num_nodes = 6;  // 0=const, 1=a, 2=b, 3=c, 4=a&b, 5=(a&b)|c
        aig.nodes.resize(6);
        
        // Initialize nodes
        for (int i = 0; i < 6; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        // Node 4 = AND(1, 2)
        aig.nodes[4].fanin0 = AIG::var2lit(1);
        aig.nodes[4].fanin1 = AIG::var2lit(2);
        
        // Node 5 = OR(4, 3) = NOT(AND(NOT(4), NOT(3)))
        aig.nodes[5].fanin0 = AIG::var2lit(4, true);  // NOT(4)
        aig.nodes[5].fanin1 = AIG::var2lit(3, true);  // NOT(3)
        
        // Set output
        aig.pos.push_back(AIG::var2lit(5, true));  // Output is NOT of node 5
        aig.num_pos = 1;
        
        // Write to file (ASCII)
        std::cout << "  Writing simple AIG (ASCII)...\n";
        aig.write_aiger("simple_test_ascii.aig", false);
        
        // Read it back
        std::cout << "  Reading back simple AIG (ASCII)...\n";
        AIG aig2("simple_test_ascii.aig");
        
        ASSERT(aig2.num_pis == 3);
        ASSERT(aig2.num_pos == 1);
        ASSERT(aig2.nodes.size() >= 6);
        
        // Write to file (binary)
        std::cout << "  Writing simple AIG (binary)...\n";
        aig.write_aiger("simple_test_binary.aig", true);
        
        // Read it back
        std::cout << "  Reading back simple AIG (binary)...\n";
        AIG aig3("simple_test_binary.aig");
        
        ASSERT(aig3.num_pis == 3);
        ASSERT(aig3.num_pos == 1);
        ASSERT(aig3.nodes.size() >= 6);
        
        std::cout << "  AIGER write test completed\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during write test: " << e.what() << std::endl;
        ASSERT(false);
    }
}