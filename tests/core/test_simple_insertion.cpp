#include <iostream>
#include "synthesis_bridge.hpp"
#include "fresub_aig.hpp"
#include <aig.hpp>

using namespace fresub;

int main() {
    std::cout << "=== SIMPLE INSERTION TEST ===\n";
    
    // Create simple test case for 2-input AND
    std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
    std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
    
    // Set up AND gate: out = in0 & in1
    br[0][0] = true; sim[0][0] = false; sim[0][1] = false; // 00 -> 0
    br[1][0] = true; sim[1][0] = false; sim[1][1] = true;  // 01 -> 0
    br[2][0] = true; sim[2][0] = true;  sim[2][1] = false; // 10 -> 0
    br[3][1] = true; sim[3][0] = true;  sim[3][1] = true;  // 11 -> 1
    
    std::cout << "Synthesizing 2-input AND function...\n";
    SynthesisResult result = synthesize_circuit(br, sim, 2);
    
    if (!result.success) {
        std::cout << "Synthesis failed: " << result.description << "\n";
        return 1;
    }
    
    std::cout << "Synthesis successful: " << result.description << "\n";
    
    aigman* exopt_aig = get_synthesis_aigman(result);
    if (!exopt_aig) {
        std::cout << "Could not get aigman\n";
        return 1;
    }
    
    std::cout << "Exopt AIG: " << exopt_aig->nPis << " PIs, " << exopt_aig->nGates << " gates\n";
    std::cout << "vObjs size: " << exopt_aig->vObjs.size() << "\n";
    
    // Create simple fresub AIG to insert into
    AIG fresub_aig;
    
    // Create two input nodes
    int node1 = 1; // Assume node 1 exists
    int node2 = 2; // Assume node 2 exists
    
    std::cout << "Converting exopt circuit...\n";
    
    // Simple conversion: map all exopt PIs to our nodes
    std::vector<int> node_mapping(exopt_aig->nPis + exopt_aig->nGates + 1, 0);
    
    // Map ALL exopt PIs - in this case we have 4 PIs for a 2-input function
    // The first 2 are pattern inputs, the last 2 are divisor inputs
    if (exopt_aig->nPis == 4) {
        // Pattern inputs don't map to real nodes
        node_mapping[1] = 1000;  // dummy node for pattern input 0
        node_mapping[2] = 1001;  // dummy node for pattern input 1
        // Divisor inputs map to real nodes
        node_mapping[3] = node1;  // divisor 0 -> node 1
        node_mapping[4] = node2;  // divisor 1 -> node 2
        
        std::cout << "  Map exopt var 1 -> dummy 1000 (pattern input)\n";
        std::cout << "  Map exopt var 2 -> dummy 1001 (pattern input)\n";
        std::cout << "  Map exopt var 3 -> node 1 (divisor)\n";
        std::cout << "  Map exopt var 4 -> node 2 (divisor)\n";
    }
    
    // Convert each gate
    for (int i = 0; i < exopt_aig->nGates; i++) {
        int gate_var = exopt_aig->nPis + 1 + i;
        
        std::cout << "  Converting gate " << i << " (var " << gate_var << ")\n";
        
        // Check if we can access the gate data safely
        int fanin0_idx = gate_var * 2;
        int fanin1_idx = gate_var * 2 + 1;
        
        if (fanin0_idx >= exopt_aig->vObjs.size() || fanin1_idx >= exopt_aig->vObjs.size()) {
            std::cout << "  ERROR: vObjs bounds exceeded\n";
            std::cout << "    Need indices " << fanin0_idx << "," << fanin1_idx << " but size is " << exopt_aig->vObjs.size() << "\n";
            delete exopt_aig;
            return 1;
        }
        
        int fanin0_lit = exopt_aig->vObjs[fanin0_idx];
        int fanin1_lit = exopt_aig->vObjs[fanin1_idx];
        
        std::cout << "    Fanins: " << fanin0_lit << ", " << fanin1_lit << "\n";
        
        // Extract variables and polarities
        int fanin0_var = fanin0_lit >> 1;
        int fanin1_var = fanin1_lit >> 1;
        bool fanin0_comp = fanin0_lit & 1;
        bool fanin1_comp = fanin1_lit & 1;
        
        std::cout << "    Variables: " << fanin0_var << (fanin0_comp ? "'" : "") 
                  << ", " << fanin1_var << (fanin1_comp ? "'" : "") << "\n";
        
        // Check if variables are mapped
        if (fanin0_var >= node_mapping.size() || fanin1_var >= node_mapping.size() ||
            node_mapping[fanin0_var] == 0 || node_mapping[fanin1_var] == 0) {
            std::cout << "  ERROR: Unmapped fanin variables\n";
            delete exopt_aig;
            return 1;
        }
        
        std::cout << "  SUCCESS: All variables mapped correctly\n";
        
        // Store the mapping for this gate (don't actually create it)
        node_mapping[gate_var] = 999 + i;  // Dummy node ID
    }
    
    std::cout << "Conversion completed successfully!\n";
    
    delete exopt_aig;
    return 0;
}