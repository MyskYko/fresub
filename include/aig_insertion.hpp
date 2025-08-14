#pragma once

#include "fresub_aig.hpp"
#include "window.hpp"
#include <vector>

// Forward declarations for exopt types
class aigman;

namespace fresub {

struct InsertionResult {
    bool success;
    int new_output_node;        // The root of inserted circuit
    std::vector<int> new_nodes; // All newly created nodes
    std::string description;
};

class AIGInsertion {
private:
    AIG& aig;
    
    // Update fanouts after node replacement
    void update_fanouts(int old_node, int new_node);

public:
    explicit AIGInsertion(AIG& aig) : aig(aig) {}

    // Convert exopt aigman to fresub nodes
    InsertionResult convert_and_insert_aigman(aigman* exopt_aig, 
                                            const std::vector<int>& divisor_nodes);
    
    // Main insertion interface
    InsertionResult insert_synthesized_circuit(const Window& window,
                                             const std::vector<int>& selected_divisors,
                                             aigman* synthesized_circuit);
    
    // Replace target node with synthesized circuit
    bool replace_target_with_circuit(int target_node, int new_circuit_root);
};

} // namespace fresub