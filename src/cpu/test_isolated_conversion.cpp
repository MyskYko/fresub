#include <iostream>
#include <vector>
#include "fresub_aig.hpp"
#include <aig.hpp>

using namespace fresub;

// Pure conversion: exopt aigman -> fresub AIG
// No mapping, no window knowledge, just structure conversion
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
        
        // Get fanins from vObjs
        int fanin0_lit = exopt_aig->vObjs[gate_var * 2];
        int fanin1_lit = exopt_aig->vObjs[gate_var * 2 + 1];
        
        int fanin0_var = fanin0_lit >> 1;
        int fanin1_var = fanin1_lit >> 1;
        bool fanin0_comp = fanin0_lit & 1;
        bool fanin1_comp = fanin1_lit & 1;
        
        std::cout << "  Gate " << i << " (var " << gate_var << "): "
                  << fanin0_var << (fanin0_comp ? "'" : "") << " & "
                  << fanin1_var << (fanin1_comp ? "'" : "") << "\n";
        
        // Map fanin vars to fresub nodes
        int fanin0_node = var_to_node[fanin0_var];
        int fanin1_node = var_to_node[fanin1_var];
        
        // Create literals
        int fanin0_fresub_lit = AIG::var2lit(fanin0_node, fanin0_comp);
        int fanin1_fresub_lit = AIG::var2lit(fanin1_node, fanin1_comp);
        
        // Store in fresub AIG structure
        result->nodes[next_node_id].fanin0 = fanin0_fresub_lit;
        result->nodes[next_node_id].fanin1 = fanin1_fresub_lit;
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
    // Create a simple test exopt AIG manually
    aigman* test_aig = new aigman(4, 1);  // 4 PIs, 1 PO
    
    // Add a simple gate: node 5 = 4 & 2
    test_aig->vObjs.resize(12);
    test_aig->vObjs[10] = 8;  // node 5 fanin0 = literal 8 (var 4, not complemented)
    test_aig->vObjs[11] = 4;  // node 5 fanin1 = literal 4 (var 2, not complemented)
    test_aig->nGates = 1;
    
    std::cout << "=== TEST ISOLATED CONVERSION ===\n";
    
    AIG* converted = convert_exopt_to_fresub(test_aig);
    
    if (converted) {
        std::cout << "\nConversion successful!\n";
        std::cout << "Fresub AIG: " << converted->num_pis << " PIs, " 
                  << converted->num_nodes << " nodes\n";
        
        // Verify structure
        std::cout << "\nVerifying structure:\n";
        for (int i = converted->num_pis + 1; i <= converted->num_nodes; i++) {
            int fanin0_lit = converted->nodes[i].fanin0;
            int fanin1_lit = converted->nodes[i].fanin1;
            int fanin0 = AIG::lit2var(fanin0_lit);
            int fanin1 = AIG::lit2var(fanin1_lit);
            bool comp0 = AIG::is_complemented(fanin0_lit);
            bool comp1 = AIG::is_complemented(fanin1_lit);
            
            std::cout << "  Node " << i << " = " 
                      << (comp0 ? "!" : "") << fanin0 << " & "
                      << (comp1 ? "!" : "") << fanin1 << "\n";
        }
        
        delete converted;
    } else {
        std::cout << "Conversion failed!\n";
    }
    
    delete test_aig;
    return 0;
}