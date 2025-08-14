#include <iostream>
#include <signal.h>
#include <execinfo.h>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "synthesis_bridge.hpp"
#include "aig_insertion.hpp"
#include <aig.hpp>

using namespace fresub;

void crash_handler(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    std::cout << "CRASH! Signal " << sig << "\n";
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// Simple test with controlled conditions
int main() {
    signal(SIGSEGV, crash_handler);
    
    std::cout << "=== DEBUG INSERTION TEST ===\n";
    
    try {
        // Create minimal AIG
        AIG aig;
        aig.num_pis = 4;
        aig.num_pos = 1;
        aig.num_nodes = 10;
        aig.nodes.resize(15);  // Extra space
        
        // Initialize nodes 1-10
        for (int i = 1; i <= 10; i++) {
            aig.nodes[i].fanin0 = 0;
            aig.nodes[i].fanin1 = 0;
            aig.nodes[i].is_dead = false;
            std::cout << "Initialized node " << i << "\n";
        }
        
        std::cout << "AIG initialized successfully\n";
        
        // Create simple synthesis
        std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
        std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
        
        // AND gate
        br[0][0] = true; sim[0][0] = false; sim[0][1] = false;
        br[1][0] = true; sim[1][0] = false; sim[1][1] = true;
        br[2][0] = true; sim[2][0] = true;  sim[2][1] = false;
        br[3][1] = true; sim[3][0] = true;  sim[3][1] = true;
        
        std::cout << "Starting synthesis...\n";
        SynthesisResult synthesis = synthesize_circuit(br, sim, 2);
        
        if (!synthesis.success) {
            std::cout << "Synthesis failed\n";
            return 1;
        }
        
        std::cout << "✓ Synthesis successful\n";
        
        // Test insertion
        std::cout << "Testing insertion...\n";
        AIGInsertion inserter(aig);
        
        std::vector<int> input_mapping = {1, 2, 3, 4};  // Map to nodes 1,2,3,4
        aigman* exopt_aig = get_synthesis_aigman(synthesis);
        
        std::cout << "Before conversion - AIG nodes: " << aig.num_nodes << "\n";
        
        InsertionResult result = inserter.convert_and_insert_aigman(exopt_aig, input_mapping);
        
        std::cout << "After conversion - insertion success: " << result.success << "\n";
        
        if (result.success) {
            std::cout << "✓ Insertion successful! Output node: " << result.new_output_node << "\n";
            
            // Test replacement
            std::cout << "Testing replacement...\n";
            std::cout << "Before replace_node call\n";
            
            bool replace_success = inserter.replace_target_with_circuit(8, result.new_output_node);
            
            std::cout << "After replace_node call: " << replace_success << "\n";
            
            if (replace_success) {
                std::cout << "✓ Replacement successful!\n";
            } else {
                std::cout << "✗ Replacement failed\n";
            }
        } else {
            std::cout << "✗ Insertion failed: " << result.description << "\n";
        }
        
        delete exopt_aig;
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "Unknown exception caught\n";
        return 1;
    }
    
    std::cout << "✓ Test completed without crash\n";
    return 0;
}