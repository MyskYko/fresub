#include <iostream>
#include <vector>
#include "fresub_aig.hpp"
#include "synthesis_bridge.hpp"
#include <aig.hpp>

using namespace fresub;

// Pure conversion: exopt aigman -> fresub AIG (same as before)
AIG* convert_exopt_to_fresub(aigman* exopt_aig) {
    if (!exopt_aig) return nullptr;
    
    std::cout << "Converting exopt AIG:\n";
    std::cout << "  PIs: " << exopt_aig->nPis << "\n";
    std::cout << "  Gates: " << exopt_aig->nGates << "\n";
    std::cout << "  vObjs size: " << exopt_aig->vObjs.size() << "\n";
    
    // Create new fresub AIG
    AIG* result = new AIG();
    
    // Set up the AIG with correct number of PIs
    result->num_pis = exopt_aig->nPis;
    result->num_pos = 1;  // Assume single output
    result->nodes.resize(exopt_aig->nPis + exopt_aig->nGates + 1);
    
    // Initialize PI nodes (nodes 1 to nPis are PIs)
    for (int i = 1; i <= exopt_aig->nPis; i++) {
        result->nodes[i].fanin0 = 0;
        result->nodes[i].fanin1 = 0;
        result->nodes[i].is_dead = false;
        std::cout << "  Created PI node " << i << "\n";
    }
    
    // Keep track of node mapping: exopt var -> fresub node
    std::vector<int> var_to_node(exopt_aig->nPis + exopt_aig->nGates + 1);
    
    // Map PIs directly
    for (int i = 1; i <= exopt_aig->nPis; i++) {
        var_to_node[i] = i;  // PI i maps to node i
    }
    
    // Convert gates
    int next_node_id = exopt_aig->nPis + 1;
    
    for (int i = 0; i < exopt_aig->nGates; i++) {
        int gate_var = exopt_aig->nPis + 1 + i;
        
        // Check bounds first
        int fanin0_idx = gate_var * 2;
        int fanin1_idx = gate_var * 2 + 1;
        
        if (fanin0_idx >= exopt_aig->vObjs.size() || fanin1_idx >= exopt_aig->vObjs.size()) {
            std::cout << "    ERROR: vObjs bounds error for gate " << i << "\n";
            delete result;
            return nullptr;
        }
        
        // Get fanins from vObjs
        int fanin0_lit = exopt_aig->vObjs[fanin0_idx];
        int fanin1_lit = exopt_aig->vObjs[fanin1_idx];
        
        int fanin0_var = fanin0_lit >> 1;
        int fanin1_var = fanin1_lit >> 1;
        bool fanin0_comp = fanin0_lit & 1;
        bool fanin1_comp = fanin1_lit & 1;
        
        std::cout << "  Gate " << i << " (var " << gate_var << "): "
                  << fanin0_var << (fanin0_comp ? "'" : "") << " & "
                  << fanin1_var << (fanin1_comp ? "'" : "") << "\n";
        
        if (fanin0_var >= var_to_node.size() || fanin1_var >= var_to_node.size()) {
            std::cout << "    ERROR: fanin variable out of bounds\n";
            delete result;
            return nullptr;
        }
        
        // Map fanin vars to fresub nodes
        int fanin0_node = var_to_node[fanin0_var];
        int fanin1_node = var_to_node[fanin1_var];
        
        if (fanin0_node == 0 || fanin1_node == 0) {
            std::cout << "    ERROR: unmapped fanin variable\n";
            delete result;
            return nullptr;
        }
        
        // Store in fresub AIG structure
        result->nodes[next_node_id].fanin0 = AIG::var2lit(fanin0_node, fanin0_comp);
        result->nodes[next_node_id].fanin1 = AIG::var2lit(fanin1_node, fanin1_comp);
        result->nodes[next_node_id].is_dead = false;
        
        std::cout << "    Created node " << next_node_id << " = "
                  << (fanin0_comp ? "!" : "") << fanin0_node << " & "
                  << (fanin1_comp ? "!" : "") << fanin1_node << "\n";
        
        // Map this gate var to the new node
        var_to_node[gate_var] = next_node_id;
        next_node_id++;
    }
    
    result->num_nodes = next_node_id - 1;
    
    // Set the output (last gate)
    if (exopt_aig->nGates > 0) {
        int last_node = next_node_id - 1;
        result->pos.push_back(AIG::var2lit(last_node, false));
        std::cout << "  Output: node " << last_node << "\n";
    }
    
    return result;
}

int main() {
    std::cout << "=== TEST REAL SYNTHESIS + CONVERSION ===\n";
    
    // Create simple 2-input AND synthesis problem
    std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
    std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
    
    // Set up AND gate: out = in0 & in1
    br[0][0] = true; sim[0][0] = false; sim[0][1] = false; // 00 -> 0
    br[1][0] = true; sim[1][0] = false; sim[1][1] = true;  // 01 -> 0
    br[2][0] = true; sim[2][0] = true;  sim[2][1] = false; // 10 -> 0
    br[3][1] = true; sim[3][0] = true;  sim[3][1] = true;  // 11 -> 1
    
    std::cout << "Synthesizing 2-input AND...\n";
    SynthesisResult synthesis = synthesize_circuit(br, sim, 2);
    
    if (!synthesis.success) {
        std::cout << "Synthesis failed!\n";
        return 1;
    }
    
    std::cout << "Synthesis successful: " << synthesis.description << "\n";
    
    aigman* exopt_aig = get_synthesis_aigman(synthesis);
    if (!exopt_aig) {
        std::cout << "Could not get exopt AIG\n";
        return 1;
    }
    
    std::cout << "\nTesting conversion...\n";
    AIG* converted = convert_exopt_to_fresub(exopt_aig);
    
    if (converted) {
        std::cout << "\n✓ Conversion successful!\n";
        std::cout << "Converted AIG: " << converted->num_pis << " PIs, " 
                  << converted->num_nodes << " total nodes, "
                  << (converted->num_nodes - converted->num_pis) << " gates\n";
        
        delete converted;
    } else {
        std::cout << "\n✗ Conversion failed!\n";
    }
    
    delete exopt_aig;
    return 0;
}