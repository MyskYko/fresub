#include "../include/window.hpp"
#include "../include/aig.hpp"
#include <iostream>

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

void test_window_extraction() {
    std::cout << "Testing window extraction...\n";
    
    // Create a simple AIG
    AIG aig;
    aig.num_pis = 3;
    aig.num_nodes = 6;
    aig.nodes.resize(6);
    
    // Build structure:
    // Node 4 = AND(1, 2)
    // Node 5 = AND(4, 3)
    aig.nodes[4].fanin0 = AIG::var2lit(1);
    aig.nodes[4].fanin1 = AIG::var2lit(2);
    aig.nodes[5].fanin0 = AIG::var2lit(4);
    aig.nodes[5].fanin1 = AIG::var2lit(3);
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Test window extraction
    WindowExtractor extractor(aig, 4, 6);
    Window window;
    
    bool success = extractor.extract_window(5, window);
    ASSERT(success == true);
    ASSERT(window.target_node == 5);
    ASSERT(!window.inputs.empty());
    ASSERT(!window.nodes.empty());
    
    // Test cut enumeration
    CutEnumerator cut_enum(aig, 4, 10);
    cut_enum.enumerate_cuts();
    
    const auto& cuts = cut_enum.get_cuts(5);
    ASSERT(!cuts.empty());
    
    // Test cut operations
    Cut cut1(1);
    Cut cut2(2);
    ASSERT(cut1.leaves.size() == 1);
    ASSERT(cut1.leaves[0] == 1);
    
    bool merged = cut1.merge_with(cut2, 4);
    ASSERT(merged == true);
    ASSERT(cut1.leaves.size() == 2);
    
    std::cout << "  Window extraction tests completed\n";
}