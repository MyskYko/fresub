#include "feasibility.hpp"
#include "fresub_aig.hpp"
#include "window.hpp"
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
    
    // Extract windows
    WindowExtractor extractor(aig, 4);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    ASSERT(!windows.empty());
    
    // Test feasibility on first suitable window
    Window* target_window = nullptr;
    for (auto& window : windows) {
        if (window.inputs.size() >= 3 && window.inputs.size() <= 4 && 
            window.divisors.size() >= 4) {
            target_window = &window;
            break;
        }
    }
    
    if (target_window) {
        // Test feasibility check
        FeasibilityResult feasible = find_feasible_4resub(aig, *target_window);
        
        // Test truth table computation
        uint64_t truth = compute_truth_table_for_node(aig, target_window->target_node, 
                                                     target_window->inputs, target_window->nodes);
        ASSERT(truth != 0); // Should have some truth table value
    }
    
    // Test batch processing (windows already extracted above)
    std::cout << "  Processed " << windows.size() << " windows\\n";
    
    std::cout << "  Resubstitution tests completed\n";
}

int main() {
    std::cout << "=== Resubstitution Test ===\n\n";
    
    test_resub_simple();
    
    std::cout << "\nTest Results: ";
    if (passed_tests == total_tests) {
        std::cout << "PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}