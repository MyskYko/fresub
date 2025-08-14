#include <iostream>
#include "synthesis_bridge.hpp"
#include <aig.hpp>

int main() {
    // Create simple test case: 2 inputs, target = input0 & input1
    std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
    std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
    
    // Pattern 00: inputs=00, target=0, divisors=00
    br[0][0] = true;  // target outputs 0
    sim[0][0] = false; sim[0][1] = false;  // divisors output 00
    
    // Pattern 01: inputs=01, target=0, divisors=01  
    br[1][0] = true;  // target outputs 0
    sim[1][0] = false; sim[1][1] = true;   // divisors output 01
    
    // Pattern 10: inputs=10, target=0, divisors=10
    br[2][0] = true;  // target outputs 0 
    sim[2][0] = true;  sim[2][1] = false;  // divisors output 10
    
    // Pattern 11: inputs=11, target=1, divisors=11
    br[3][1] = true;  // target outputs 1
    sim[3][0] = true;  sim[3][1] = true;   // divisors output 11
    
    std::cout << "Testing simple 2-input AND synthesis...\n";
    
    SynthesisResult result = synthesize_circuit(br, sim, 2);
    
    if (result.success) {
        std::cout << "Synthesis successful: " << result.description << "\n";
        
        aigman* aig = get_synthesis_aigman(result);
        if (aig) {
            std::cout << "Exopt AIG structure:\n";
            std::cout << "  nPis: " << aig->nPis << "\n";
            std::cout << "  nGates: " << aig->nGates << "\n";
            std::cout << "  vObjs.size(): " << aig->vObjs.size() << "\n";
            
            std::cout << "  vObjs contents: ";
            for (size_t i = 0; i < aig->vObjs.size() && i < 20; i++) {
                std::cout << aig->vObjs[i] << " ";
            }
            std::cout << "\n";
            
            delete aig;
        } else {
            std::cout << "Could not get aigman from result\n";
        }
    } else {
        std::cout << "Synthesis failed: " << result.description << "\n";
    }
    
    return 0;
}