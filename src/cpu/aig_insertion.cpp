#include "aig_insertion.hpp"
#include "aig_converter.hpp"
#include <aig.hpp>
#include <iostream>
#include <set>

using namespace fresub;

InsertionResult AIGInsertion::convert_and_insert_aigman(aigman* exopt_aig, 
                                                       const std::vector<int>& input_nodes) {
    InsertionResult result;
    result.success = false;
    
    if (!exopt_aig) {
        result.description = "Null exopt AIG provided";
        return result;
    }
    
    std::cout << "Using two-stage conversion process:\n";
    
    // Stage 1: Convert exopt AIG to clean fresub AIG
    AIG* converted = convert_exopt_to_fresub(exopt_aig);
    if (!converted) {
        result.description = "Failed to convert exopt AIG";
        return result;
    }
    
    // Stage 2: Map converted AIG to target nodes and insert
    MappingResult mapping = map_and_insert_aig(aig, *converted, input_nodes);
    
    if (mapping.success) {
        result.success = true;
        result.new_output_node = mapping.output_node;
        result.new_nodes = mapping.new_nodes;
        result.description = mapping.description;
    } else {
        result.description = "Mapping failed: " + mapping.description;
    }
    
    delete converted;
    return result;
}

InsertionResult AIGInsertion::insert_synthesized_circuit(const Window& window,
                                                        const std::vector<int>& selected_divisors,
                                                        aigman* synthesized_circuit) {
    InsertionResult result;
    result.success = false;
    
    if (!synthesized_circuit) {
        result.description = "Null synthesized circuit";
        return result;
    }
    
    std::cout << "  Converting exopt AIG to fresub AIG and inserting...\n";
    
    // Filter divisors to exclude window inputs (avoid double-counting)
    std::vector<int> non_input_divisors;
    std::set<int> window_input_set(window.inputs.begin(), window.inputs.end());
    
    for (int divisor : window.divisors) {
        if (window_input_set.find(divisor) == window_input_set.end()) {
            non_input_divisors.push_back(divisor);
        }
    }
    
    // Create combined input mapping: window inputs + selected non-input divisors
    std::vector<int> all_input_nodes;
    
    // Add window inputs first
    for (int input : window.inputs) {
        all_input_nodes.push_back(input);
    }
    
    // Add selected non-input divisor nodes
    for (int idx : selected_divisors) {
        if (idx >= 0 && idx < window.divisors.size()) {
            int divisor_node = window.divisors[idx];
            // Only add if it's not a window input (to avoid double-counting)
            if (window_input_set.find(divisor_node) == window_input_set.end()) {
                all_input_nodes.push_back(divisor_node);
            }
        } else {
            result.description = "Invalid divisor index: " + std::to_string(idx);
            return result;
        }
    }
    
    
    // Convert and insert exopt circuit with combined inputs
    result = convert_and_insert_aigman(synthesized_circuit, all_input_nodes);
    
    if (result.success) {
        std::cout << "  ✓ Circuit inserted successfully\n";
        std::cout << "  New circuit root: " << result.new_output_node << "\n";
    } else {
        std::cout << "  ✗ Circuit insertion failed: " << result.description << "\n";
    }
    
    return result;
}

bool AIGInsertion::replace_target_with_circuit(int target_node, int new_circuit_root) {
    std::cout << "\nReplacing target node " << target_node 
              << " with circuit root " << new_circuit_root << "\n";
    
    if (target_node >= aig.nodes.size() || new_circuit_root >= aig.nodes.size()) {
        std::cout << "  ✗ Invalid node IDs\n";
        return false;
    }
    
    if (aig.nodes[target_node].is_dead || aig.nodes[new_circuit_root].is_dead) {
        std::cout << "  ✗ Dead node encountered\n";
        return false;
    }
    
    // Use AIG's built-in replace_node method
    aig.replace_node(target_node, new_circuit_root);
    
    std::cout << "  ✓ Target node replaced successfully\n";
    return true;
}

void AIGInsertion::update_fanouts(int old_node, int new_node) {
    // This is handled by AIG::replace_node, but we can add custom logic if needed
    std::cout << "  Updating fanouts: " << old_node << " -> " << new_node << "\n";
}