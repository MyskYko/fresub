#include "fresub_aig.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
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

void test_ascii_format_comprehensive() {
    std::cout << "Testing comprehensive ASCII AIGER format...\n";
    
    // Test various AIG structures in ASCII format
    
    // Test 1: Simple 2-input AND
    std::cout << "  Testing simple AND in ASCII...\n";
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
        
        // Write ASCII
        aig.write_aiger("test_simple_and_ascii.aig", false);
        
        // Read back
        AIG aig2("test_simple_and_ascii.aig");
        
        ASSERT(aig2.num_pis == aig.num_pis);
        ASSERT(aig2.num_pos == aig.num_pos);
        ASSERT(aig2.nodes.size() == aig.nodes.size());
        ASSERT(aig2.pos[0] == aig.pos[0]);
    }
    
    // Test 2: Multiple outputs
    std::cout << "  Testing multiple outputs in ASCII...\n";
    {
        AIG aig;
        aig.num_pis = 2;
        aig.num_nodes = 5; // 0=const, 1=a, 2=b, 3=a&b, 4=a|b
        aig.nodes.resize(5);
        
        for (int i = 0; i < 5; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.nodes[3].fanin0 = AIG::var2lit(1);     // a & b
        aig.nodes[3].fanin1 = AIG::var2lit(2);
        
        aig.nodes[4].fanin0 = AIG::var2lit(1, true); // !(~a & ~b) = a | b
        aig.nodes[4].fanin1 = AIG::var2lit(2, true);
        
        aig.pos.push_back(AIG::var2lit(3));        // AND output
        aig.pos.push_back(AIG::var2lit(4, true));  // OR output (inverted)
        aig.num_pos = 2;
        
        // Write and read back
        aig.write_aiger("test_multi_out_ascii.aig", false);
        AIG aig2("test_multi_out_ascii.aig");
        
        ASSERT(aig2.num_pis == 2);
        ASSERT(aig2.num_pos == 2);
        ASSERT(aig2.pos.size() == 2);
    }
    
    // Test 3: Complemented inputs/outputs
    std::cout << "  Testing complemented signals in ASCII...\n";
    {
        AIG aig;
        aig.num_pis = 2;
        aig.num_nodes = 4;
        aig.nodes.resize(4);
        
        for (int i = 0; i < 4; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        // NAND gate: !(a & b)
        aig.nodes[3].fanin0 = AIG::var2lit(1);
        aig.nodes[3].fanin1 = AIG::var2lit(2);
        
        aig.pos.push_back(AIG::var2lit(3, true)); // Complemented output
        aig.num_pos = 1;
        
        aig.write_aiger("test_nand_ascii.aig", false);
        AIG aig2("test_nand_ascii.aig");
        
        ASSERT(aig2.num_pis == 2);
        ASSERT(aig2.num_pos == 1);
        ASSERT(AIG::is_complemented(aig2.pos[0])); // Should preserve complementation
    }
    
    std::cout << "  ASCII format comprehensive tests completed\n";
}

void test_binary_format_comprehensive() {
    std::cout << "Testing comprehensive binary AIGER format...\n";
    
    // Test 1: Binary encoding/decoding
    std::cout << "  Testing binary encoding accuracy...\n";
    {
        AIG aig;
        aig.num_pis = 3;
        aig.num_nodes = 6; // Chain: ((a&b)&c)
        aig.nodes.resize(6);
        
        for (int i = 0; i < 6; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.nodes[4].fanin0 = AIG::var2lit(1); // a & b
        aig.nodes[4].fanin1 = AIG::var2lit(2);
        
        aig.nodes[5].fanin0 = AIG::var2lit(4); // (a&b) & c
        aig.nodes[5].fanin1 = AIG::var2lit(3);
        
        aig.pos.push_back(AIG::var2lit(5));
        aig.num_pos = 1;
        
        // Write binary
        aig.write_aiger("test_chain_binary.aig", true);
        
        // Read back
        AIG aig2("test_chain_binary.aig");
        
        ASSERT(aig2.num_pis == aig.num_pis);
        ASSERT(aig2.num_pos == aig.num_pos);
        ASSERT(aig2.nodes.size() == aig.nodes.size());
        
        // Verify structure is preserved
        ASSERT(aig2.nodes[4].fanin0 == AIG::var2lit(1));
        ASSERT(aig2.nodes[4].fanin1 == AIG::var2lit(2));
        ASSERT(aig2.nodes[5].fanin0 == AIG::var2lit(4));
        ASSERT(aig2.nodes[5].fanin1 == AIG::var2lit(3));
    }
    
    // Test 2: Large binary circuit
    std::cout << "  Testing larger binary circuit...\n";
    {
        AIG aig;
        aig.num_pis = 4;
        aig.num_nodes = 10; // More complex structure
        aig.nodes.resize(10);
        
        for (int i = 0; i < 10; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        // Build tree structure
        aig.nodes[5].fanin0 = AIG::var2lit(1); // a & b
        aig.nodes[5].fanin1 = AIG::var2lit(2);
        
        aig.nodes[6].fanin0 = AIG::var2lit(3); // c & d
        aig.nodes[6].fanin1 = AIG::var2lit(4);
        
        aig.nodes[7].fanin0 = AIG::var2lit(5, true); // !(a&b)
        aig.nodes[7].fanin1 = AIG::var2lit(0);
        
        aig.nodes[8].fanin0 = AIG::var2lit(6, true); // !(c&d)  
        aig.nodes[8].fanin1 = AIG::var2lit(0);
        
        aig.nodes[9].fanin0 = AIG::var2lit(7, true); // !!(a&b) & !!(c&d) = (a&b) | (c&d)
        aig.nodes[9].fanin1 = AIG::var2lit(8, true);
        
        aig.pos.push_back(AIG::var2lit(9));
        aig.num_pos = 1;
        
        aig.write_aiger("test_large_binary.aig", true);
        AIG aig2("test_large_binary.aig");
        
        ASSERT(aig2.num_pis == 4);
        ASSERT(aig2.num_pos == 1);
        ASSERT(aig2.nodes.size() >= 9); // At least this many nodes
    }
    
    std::cout << "  Binary format comprehensive tests completed\n";
}

void test_roundtrip_consistency() {
    std::cout << "Testing ASCII/binary roundtrip consistency...\n";
    
    // Create moderately complex AIG
    AIG original;
    original.num_pis = 3;
    original.num_nodes = 7;
    original.nodes.resize(7);
    
    for (int i = 0; i < 7; i++) {
        original.nodes[i] = AIG::Node{0, 0, 0, {}, false};
    }
    
    // XOR-like structure: (a&b) | (!a&!b) - equivalence
    original.nodes[4].fanin0 = AIG::var2lit(1);     // a & b
    original.nodes[4].fanin1 = AIG::var2lit(2);
    
    original.nodes[5].fanin0 = AIG::var2lit(1, true); // !a & !b
    original.nodes[5].fanin1 = AIG::var2lit(2, true);
    
    original.nodes[6].fanin0 = AIG::var2lit(4, true); // !(a&b) & !(!a&!b)
    original.nodes[6].fanin1 = AIG::var2lit(5, true);
    
    original.pos.push_back(AIG::var2lit(6, true)); // Final inversion
    original.num_pos = 1;
    
    // Test ASCII -> Binary -> ASCII
    std::cout << "  Testing ASCII -> Binary -> ASCII...\n";
    
    original.write_aiger("test_orig_ascii.aig", false);
    AIG from_ascii("test_orig_ascii.aig");
    
    from_ascii.write_aiger("test_ascii_to_binary.aig", true);
    AIG from_binary("test_ascii_to_binary.aig");
    
    from_binary.write_aiger("test_back_to_ascii.aig", false);
    AIG final_ascii("test_back_to_ascii.aig");
    
    // Verify consistency
    ASSERT(final_ascii.num_pis == original.num_pis);
    ASSERT(final_ascii.num_pos == original.num_pos);
    
    // Test Binary -> ASCII -> Binary
    std::cout << "  Testing Binary -> ASCII -> Binary...\n";
    
    original.write_aiger("test_orig_binary.aig", true);
    AIG from_binary2("test_orig_binary.aig");
    
    from_binary2.write_aiger("test_binary_to_ascii.aig", false);
    AIG from_ascii2("test_binary_to_ascii.aig");
    
    from_ascii2.write_aiger("test_back_to_binary.aig", true);
    AIG final_binary("test_back_to_binary.aig");
    
    ASSERT(final_binary.num_pis == original.num_pis);
    ASSERT(final_binary.num_pos == original.num_pos);
    
    std::cout << "  Roundtrip consistency tests completed\n";
}

void test_edge_cases_aiger() {
    std::cout << "Testing AIGER edge cases...\n";
    
    // Test 1: Single input, single output (identity)
    std::cout << "  Testing identity function...\n";
    {
        AIG aig;
        aig.num_pis = 1;
        aig.num_nodes = 2; // 0=const, 1=input
        aig.nodes.resize(2);
        
        for (int i = 0; i < 2; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.pos.push_back(AIG::var2lit(1)); // Output = input
        aig.num_pos = 1;
        
        // Test both formats
        aig.write_aiger("test_identity_ascii.aig", false);
        aig.write_aiger("test_identity_binary.aig", true);
        
        AIG aig_ascii("test_identity_ascii.aig");
        AIG aig_binary("test_identity_binary.aig");
        
        ASSERT(aig_ascii.num_pis == 1);
        ASSERT(aig_ascii.num_pos == 1);
        ASSERT(aig_binary.num_pis == 1);
        ASSERT(aig_binary.num_pos == 1);
    }
    
    // Test 2: Constant outputs
    std::cout << "  Testing constant outputs...\n";
    {
        AIG aig;
        aig.num_pis = 2;
        aig.num_nodes = 3;
        aig.nodes.resize(3);
        
        for (int i = 0; i < 3; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        aig.pos.push_back(AIG::var2lit(0));      // Constant 0
        aig.pos.push_back(AIG::var2lit(0, true)); // Constant 1
        aig.num_pos = 2;
        
        aig.write_aiger("test_constants_ascii.aig", false);
        aig.write_aiger("test_constants_binary.aig", true);
        
        AIG aig_ascii("test_constants_ascii.aig");
        AIG aig_binary("test_constants_binary.aig");
        
        ASSERT(aig_ascii.num_pos == 2);
        ASSERT(aig_binary.num_pos == 2);
        ASSERT(aig_ascii.pos[0] == AIG::var2lit(0));
        ASSERT(aig_ascii.pos[1] == AIG::var2lit(0, true));
    }
    
    // Test 3: Many primary inputs
    std::cout << "  Testing many primary inputs...\n";
    {
        AIG aig;
        aig.num_pis = 8; // 8 inputs
        aig.num_nodes = 12; // Tree structure
        aig.nodes.resize(12);
        
        for (int i = 0; i < 12; i++) {
            aig.nodes[i] = AIG::Node{0, 0, 0, {}, false};
        }
        
        // Build binary tree of ANDs
        aig.nodes[9].fanin0 = AIG::var2lit(1);  // AND level 1
        aig.nodes[9].fanin1 = AIG::var2lit(2);
        
        aig.nodes[10].fanin0 = AIG::var2lit(3);
        aig.nodes[10].fanin1 = AIG::var2lit(4);
        
        aig.nodes[11].fanin0 = AIG::var2lit(9); // AND level 2
        aig.nodes[11].fanin1 = AIG::var2lit(10);
        
        aig.pos.push_back(AIG::var2lit(11));
        aig.num_pos = 1;
        
        aig.write_aiger("test_many_inputs_binary.aig", true);
        AIG aig2("test_many_inputs_binary.aig");
        
        ASSERT(aig2.num_pis == 8);
        ASSERT(aig2.num_pos == 1);
    }
    
    std::cout << "  Edge case tests completed\n";
}

void test_file_validation() {
    std::cout << "Testing file validation and error handling...\n";
    
    // Test 1: Invalid file format
    std::cout << "  Testing invalid file detection...\n";
    {
        // Create invalid AIGER file
        std::ofstream bad_file("test_invalid.aig");
        bad_file << "invalid header\n";
        bad_file << "1 2 3 4 5\n";
        bad_file.close();
        
        try {
            AIG aig("test_invalid.aig");
            ASSERT(false); // Should have thrown exception
        } catch (const std::exception& e) {
            // Expected to throw
            ASSERT(true);
        }
    }
    
    // Test 2: File size consistency
    std::cout << "  Testing file size consistency...\n";
    {
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
        
        // Write both formats
        aig.write_aiger("test_size_ascii.aig", false);
        aig.write_aiger("test_size_binary.aig", true);
        
        // Check that files exist and have reasonable sizes
        ASSERT(std::filesystem::exists("test_size_ascii.aig"));
        ASSERT(std::filesystem::exists("test_size_binary.aig"));
        
        auto ascii_size = std::filesystem::file_size("test_size_ascii.aig");
        auto binary_size = std::filesystem::file_size("test_size_binary.aig");
        
        ASSERT(ascii_size > 0);
        ASSERT(binary_size > 0);
        // Binary might be smaller for small circuits, but both should be reasonable
    }
    
    std::cout << "  File validation tests completed\n";
}

int main() {
    std::cout << "=== AIGER Comprehensive Test ===\n\n";
    
    test_ascii_format_comprehensive();
    test_binary_format_comprehensive();
    test_roundtrip_consistency();
    test_edge_cases_aiger();
    test_file_validation();
    
    std::cout << "\nTest Results: ";
    if (passed_tests == total_tests) {
        std::cout << "PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}