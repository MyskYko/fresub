#include "../include/resub.hpp"
#include "../include/aig.hpp"
#include "../include/window.hpp"
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

void test_resub_simple() {
    std::cout << "Testing resubstitution...\n";
    
    // Create a simple redundant AIG
    // f = (a & b) & (a & b) - redundant, can be simplified to (a & b)
    AIG aig;
    aig.num_pis = 2;
    aig.num_nodes = 5;
    aig.nodes.resize(5);
    
    // Node 3 = AND(1, 2)
    aig.nodes[3].fanin0 = AIG::var2lit(1);
    aig.nodes[3].fanin1 = AIG::var2lit(2);
    
    // Node 4 = AND(3, 3) - redundant
    aig.nodes[4].fanin0 = AIG::var2lit(3);
    aig.nodes[4].fanin1 = AIG::var2lit(3);
    
    aig.pos.push_back(AIG::var2lit(4));
    aig.num_pos = 1;
    
    aig.build_fanouts();
    aig.compute_levels();
    
    // Extract window for node 4
    WindowExtractor extractor(aig, 4, 6);
    Window window;
    bool success = extractor.extract_window(4, window);
    ASSERT(success == true);
    
    // Perform resubstitution
    ResubEngine engine(aig, false);  // Use CPU
    ResubResult result = engine.resubstitute(window);
    
    // Check that some optimization was possible
    // (exact result depends on implementation details)
    ASSERT(window.target_node == 4);
    ASSERT(!window.divisors.empty());
    
    // Test truth table computation
    TruthTable truth;
    std::vector<int> inputs = {1, 2};
    engine.compute_truth_table(3, inputs, truth);
    ASSERT(!truth.empty());
    
    // Test batch processing
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    std::vector<ResubResult> results = engine.resubstitute_batch(windows);
    ASSERT(results.size() == windows.size());
    
    std::cout << "  Resubstitution tests completed\n";
}