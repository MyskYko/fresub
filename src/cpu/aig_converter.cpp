#include "aig_converter.hpp"
#include <aig.hpp>
#include <iostream>
#include <unordered_map>

using namespace fresub;

// Stage 1: Pure conversion exopt -> fresub
AIG* fresub::convert_exopt_to_fresub(aigman* exopt_aig) {
    if (!exopt_aig) return nullptr;
    
    std::cout << "Converting exopt AIG: " << exopt_aig->nPis 
              << " PIs, " << exopt_aig->nGates << " gates\n";
    
    // Create new fresub AIG
    AIG* result = new AIG();
    
    // Set up the AIG structure
    result->num_pis = exopt_aig->nPis;
    result->num_pos = 1;
    result->nodes.resize(exopt_aig->nPis + exopt_aig->nGates + 1);
    
    // Initialize PI nodes
    for (int i = 1; i <= exopt_aig->nPis; i++) {
        result->nodes[i].fanin0 = 0;
        result->nodes[i].fanin1 = 0;
        result->nodes[i].is_dead = false;
    }
    
    // Node mapping: exopt var -> fresub node
    std::vector<int> var_to_node(exopt_aig->nPis + exopt_aig->nGates + 1);
    
    // Map PIs directly
    for (int i = 1; i <= exopt_aig->nPis; i++) {
        var_to_node[i] = i;
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
        
        // Map fanin vars to fresub nodes
        int fanin0_node = var_to_node[fanin0_var];
        int fanin1_node = var_to_node[fanin1_var];
        
        // Store in fresub AIG structure
        result->nodes[next_node_id].fanin0 = AIG::var2lit(fanin0_node, fanin0_comp);
        result->nodes[next_node_id].fanin1 = AIG::var2lit(fanin1_node, fanin1_comp);
        result->nodes[next_node_id].is_dead = false;
        
        // Map this gate var to the new node
        var_to_node[gate_var] = next_node_id;
        next_node_id++;
    }
    
    result->num_nodes = next_node_id - 1;
    
    // Set output to last gate
    if (exopt_aig->nGates > 0) {
        result->pos.push_back(AIG::var2lit(next_node_id - 1, false));
    }
    
    return result;
}

// Stage 2: Map converted AIG to actual nodes and insert
MappingResult fresub::map_and_insert_aig(AIG& target_aig, const AIG& converted_aig, 
                                const std::vector<int>& input_mapping) {
    MappingResult result;
    result.success = false;
    
    if (input_mapping.size() < converted_aig.num_pis) {
        result.description = "Not enough input mappings provided";
        return result;
    }
    
    std::cout << "Mapping and inserting converted AIG:\n";
    std::cout << "  Converted AIG: " << converted_aig.num_pis << " PIs, " 
              << (converted_aig.num_nodes - converted_aig.num_pis) << " gates\n";
    std::cout << "  Target AIG: " << target_aig.num_nodes << " nodes\n";
    
    // Create mapping from converted nodes to target nodes
    std::unordered_map<int, int> node_map;
    
    // Map converted PIs to target nodes
    for (int i = 1; i <= converted_aig.num_pis; i++) {
        int target_node = input_mapping[i - 1];  // input_mapping is 0-indexed
        node_map[i] = target_node;
        std::cout << "  Map converted PI " << i << " -> target node " << target_node << "\n";
    }
    
    // Convert and insert gates
    for (int i = converted_aig.num_pis + 1; i <= converted_aig.num_nodes; i++) {
        int fanin0_lit = converted_aig.nodes[i].fanin0;
        int fanin1_lit = converted_aig.nodes[i].fanin1;
        
        int fanin0_node = AIG::lit2var(fanin0_lit);
        int fanin1_node = AIG::lit2var(fanin1_lit);
        bool fanin0_comp = AIG::is_complemented(fanin0_lit);
        bool fanin1_comp = AIG::is_complemented(fanin1_lit);
        
        // Map to target nodes
        int target_fanin0 = node_map[fanin0_node];
        int target_fanin1 = node_map[fanin1_node];
        
        // Create literals
        int target_lit0 = AIG::var2lit(target_fanin0, fanin0_comp);
        int target_lit1 = AIG::var2lit(target_fanin1, fanin1_comp);
        
        // Create AND gate in target AIG
        int new_node = target_aig.create_and(target_lit0, target_lit1);
        node_map[i] = new_node;
        result.new_nodes.push_back(new_node);
        
        std::cout << "  Created gate " << new_node << " = " 
                  << (fanin0_comp ? "!" : "") << target_fanin0 << " & "
                  << (fanin1_comp ? "!" : "") << target_fanin1 << "\n";
    }
    
    // Set output node (last created gate)
    if (!result.new_nodes.empty()) {
        result.output_node = result.new_nodes.back();
        result.success = true;
        result.description = "Successfully mapped and inserted " + 
                           std::to_string(result.new_nodes.size()) + " gates";
        std::cout << "  Circuit output: node " << result.output_node << "\n";
    } else {
        result.description = "No gates to insert";
    }
    
    return result;
}