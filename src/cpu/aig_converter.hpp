#pragma once

#include "fresub_aig.hpp"

// Forward declaration
class aigman;

namespace fresub {

// Stage 1: Pure conversion exopt -> fresub (no mapping)
AIG* convert_exopt_to_fresub(aigman* exopt_aig);

// Stage 2: Map converted AIG inputs to actual nodes and insert into target AIG
struct MappingResult {
    bool success;
    int output_node;           // The final output node in target AIG
    std::vector<int> new_nodes; // All newly created nodes
    std::string description;
};

MappingResult map_and_insert_aig(AIG& target_aig, const AIG& converted_aig, 
                                const std::vector<int>& input_mapping);

} // namespace fresub