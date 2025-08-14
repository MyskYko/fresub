#include <iostream>
#include "synthesis_bridge.hpp"
#include "aig_converter.hpp"
#include <aig.hpp>

using namespace fresub;

int main() {
    std::cout << "=== TEST TWO-STAGE CONVERSION + MAPPING ===\n";
    
    // Create synthesis problem
    std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
    std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
    
    // AND gate: out = div0 & div1
    br[0][0] = true; sim[0][0] = false; sim[0][1] = false; // 00 -> 0
    br[1][0] = true; sim[1][0] = false; sim[1][1] = true;  // 01 -> 0  
    br[2][0] = true; sim[2][0] = true;  sim[2][1] = false; // 10 -> 0
    br[3][1] = true; sim[3][0] = true;  sim[3][1] = true;  // 11 -> 1
    
    // Step 1: Synthesize
    std::cout << "STEP 1: Synthesis\n";
    SynthesisResult synthesis = synthesize_circuit(br, sim, 2);
    if (!synthesis.success) {
        std::cout << "Synthesis failed!\n";
        return 1;
    }
    std::cout << "✓ " << synthesis.description << "\n\n";
    
    // Step 2: Convert exopt -> fresub
    std::cout << "STEP 2: Convert exopt AIG to fresub AIG\n";
    aigman* exopt_aig = get_synthesis_aigman(synthesis);
    AIG* converted = convert_exopt_to_fresub(exopt_aig);
    if (!converted) {
        std::cout << "✗ Conversion failed!\n";
        return 1;
    }
    std::cout << "✓ Conversion successful\n\n";
    
    // Step 3: Create target AIG and map
    std::cout << "STEP 3: Map to target AIG\n";
    AIG target_aig;
    
    // Initialize target with some nodes
    target_aig.num_pis = 4;
    target_aig.num_nodes = 10; // Assume we have nodes 1-10
    target_aig.nodes.resize(11);
    
    for (int i = 1; i <= 10; i++) {
        target_aig.nodes[i].is_dead = false;
    }
    
    // Create input mapping: converted PIs 1,2,3,4 -> target nodes 7,8,9,10
    std::vector<int> input_mapping = {7, 8, 9, 10};
    
    MappingResult mapping = map_and_insert_aig(target_aig, *converted, input_mapping);
    
    if (mapping.success) {
        std::cout << "✓ " << mapping.description << "\n";
        std::cout << "Final output node: " << mapping.output_node << "\n";
        std::cout << "\n🎯 TWO-STAGE PROCESS SUCCESSFUL! 🎯\n";
    } else {
        std::cout << "✗ Mapping failed: " << mapping.description << "\n";
    }
    
    delete converted;
    delete exopt_aig;
    return 0;
}