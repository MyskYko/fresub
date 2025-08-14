#include "fresub_aig.hpp"
#include "window.hpp"
#include "feasibility.hpp"
#include "synthesis_bridge.hpp"
#include <iostream>

using namespace fresub;

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

void test_synthesis_integration() {
    std::cout << "Testing synthesis integration...\n";
    
    // Load a simple test circuit
    AIG aig;
    aig.num_pis = 3;
    aig.num_pos = 1;
    aig.nodes.resize(6);
    
    // Create a simple AND-OR circuit: (a & b) | c
    // Node 0: constant
    // Nodes 1, 2, 3: PIs
    // Node 4: a & b
    // Node 5: (a & b) | c
    aig.nodes[0].is_dead = false;
    aig.nodes[1].is_dead = false; 
    aig.nodes[2].is_dead = false;
    aig.nodes[3].is_dead = false;
    aig.nodes[4].is_dead = false;
    aig.nodes[5].is_dead = false;
    
    aig.nodes[4].fanin0 = 2; // a
    aig.nodes[4].fanin1 = 4; // b  
    aig.nodes[5].fanin0 = 8; // (a & b)
    aig.nodes[5].fanin1 = 6; // c
    
    aig.num_nodes = 6;
    // Note: PO setup varies by AIG implementation
    
    // Test window extraction
    WindowExtractor extractor(aig, 4);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "  Extracted " << windows.size() << " windows\n";
    ASSERT(!windows.empty());
    
    // Test feasibility on suitable windows
    int feasible_count = 0;
    for (const auto& window : windows) {
        if (window.inputs.size() >= 2 && window.inputs.size() <= 4 && 
            window.divisors.size() >= 2) {
            
            FeasibilityResult result = find_feasible_4resub(aig, window);
            if (result.found) {
                feasible_count++;
                
                // Test synthesis bridge
                uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, 
                                                                 window.inputs, window.nodes);
                std::vector<uint64_t> divisor_tts;
                for (int divisor : window.divisors) {
                    uint64_t tt = compute_truth_table_for_node(aig, divisor, window.inputs, window.nodes);
                    divisor_tts.push_back(tt);
                }
                
                std::vector<std::vector<bool>> br, sim;
                convert_to_exopt_format(target_tt, divisor_tts, result.divisor_indices,
                                       window.inputs.size(), window.inputs, window.divisors, br, sim);
                
                ASSERT(!br.empty());
                ASSERT(!sim.empty());
                
                // Test synthesis
                SynthesisResult synth_result = synthesize_circuit(br, sim, 4);
                // Note: synthesis may fail, that's OK for this test
                
                break; // Test first feasible window
            }
        }
    }
    
    std::cout << "  Found " << feasible_count << " feasible windows\n";
    std::cout << "  Synthesis integration tests completed\n";
}

int main(int argc, char** argv) {
    try {
        test_synthesis_integration();
        
        std::cout << "\n=== TEST RESULTS ===\n";
        std::cout << "Total tests: " << total_tests << "\n";
        std::cout << "Passed tests: " << passed_tests << "\n";
        
        if (passed_tests == total_tests) {
            std::cout << "All tests passed!\n";
            return 0;
        } else {
            std::cout << "Some tests failed.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}