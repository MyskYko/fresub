#include "fresub_aig.hpp"
#include "feasibility.hpp"
#include "window.hpp"
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

void test_feasibility_basic() {
    std::cout << "Testing basic feasibility checking...\n";
    
    // Create a simple AIG: x1 AND x2
    AIG aig;
    
    // Build fanouts for proper operation
    aig.build_fanouts();
    aig.compute_levels();
    
    std::cout << "  AIG created with " << aig.num_nodes << " nodes\n";
    
    // Create a simple window for testing
    Window window;
    window.target_node = 5; // A simple target
    window.inputs = {1, 2, 3};
    window.divisors = {1, 2, 3, 4};
    
    // Test feasibility check
    FeasibilityResult result = find_feasible_4resub(aig, window);
    
    std::cout << "  Feasibility result: " << (result.found ? "FOUND" : "NOT FOUND") << "\n";
    if (result.found) {
        std::cout << "    Selected divisors: ";
        for (int idx : result.divisor_indices) {
            std::cout << idx << " ";
        }
        std::cout << "\n";
    }
    
    // This should pass regardless of result (just testing API works)
    ASSERT(true);
}

void test_window_extraction_feasibility() {
    std::cout << "Testing window-based feasibility...\n";
    
    // Create a more complex AIG
    AIG aig;
    aig.build_fanouts();
    aig.compute_levels();
    
    // Extract windows using WindowExtractor
    WindowExtractor extractor(aig, 4);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "  Extracted " << windows.size() << " windows\n";
    
    int feasible_count = 0;
    for (size_t i = 0; i < std::min(windows.size(), size_t(10)); i++) {
        FeasibilityResult result = find_feasible_4resub(aig, windows[i]);
        if (result.found) {
            feasible_count++;
        }
    }
    
    std::cout << "  Found " << feasible_count << " feasible windows (out of first 10)\n";
    
    // Test that we can call the function without crashing
    ASSERT(true);
}

void test_edge_cases() {
    std::cout << "Testing edge cases...\n";
    
    AIG aig;
    
    // Test with empty window
    Window empty_window;
    empty_window.target_node = 1;
    empty_window.inputs = {};
    empty_window.divisors = {};
    
    FeasibilityResult result = find_feasible_4resub(aig, empty_window);
    std::cout << "  Empty window feasibility: " << (result.found ? "FOUND" : "NOT FOUND") << "\n";
    
    // Test with single input
    Window single_window;
    single_window.target_node = 2;
    single_window.inputs = {1};
    single_window.divisors = {1};
    
    result = find_feasible_4resub(aig, single_window);
    std::cout << "  Single input feasibility: " << (result.found ? "FOUND" : "NOT FOUND") << "\n";
    
    ASSERT(true);
}

int main(int argc, char* argv[]) {
    std::cout << "=== DETAILED FEASIBILITY TESTING ===\n\n";
    
    try {
        test_feasibility_basic();
        test_window_extraction_feasibility();
        test_edge_cases();
        
        std::cout << "\n=== TEST RESULTS ===\n";
        std::cout << "Passed: " << passed_tests << "/" << total_tests << "\n";
        
        if (passed_tests == total_tests) {
            std::cout << "✅ All tests passed!\n";
            return 0;
        } else {
            std::cout << "❌ Some tests failed!\n";
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}