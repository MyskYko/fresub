#include "window.hpp"
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
    WindowExtractor extractor(aig, 4);
    std::vector<Window> windows;
    
    extractor.extract_all_windows(windows);
    ASSERT(!windows.empty());
    
    // Test first window
    Window& window = windows[0];
    ASSERT(!window.inputs.empty());
    ASSERT(!window.nodes.empty());
    
    // Test cut enumeration
    CutEnumerator cut_enum(aig, 4);
    cut_enum.enumerate_cuts();
    
    const auto& cuts = cut_enum.get_cuts(5);
    ASSERT(!cuts.empty());
    
    // Test cut operations
    Cut cut1(1);
    Cut cut2(2);
    ASSERT(cut1.leaves.size() == 1);
    ASSERT(cut1.leaves[0] == 1);
    
    // Test basic cut properties
    ASSERT(cut2.leaves.size() == 1);
    ASSERT(cut2.leaves[0] == 2);
    
    std::cout << "  Window extraction tests completed\n";
}

int main() {
    std::cout << "=== Window Extraction Test ===\n\n";
    
    test_window_extraction();
    
    std::cout << "\nTest Results: ";
    if (passed_tests == total_tests) {
        std::cout << "PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}