#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <algorithm>

namespace fresub {

class AIG {
public:
    struct Node {
        int fanin0;  // first fanin (can be complemented)
        int fanin1;  // second fanin (can be complemented)
        int level;   // level in the AIG
        std::vector<int> fanouts;  // list of fanout nodes
        bool is_dead;  // marked for removal
    };

    int num_pis;
    int num_pos;
    int num_nodes;
    std::vector<int> pos;  // primary outputs
    std::vector<Node> nodes;  // all nodes including PIs
    
    // Simulation data
    std::vector<uint64_t> sim_values;
    
    AIG();
    AIG(const std::string& filename);
    
    // File I/O
    void read_aiger(const std::string& filename);
    void write_aiger(const std::string& filename, bool binary = false) const;
    
    // Basic operations
    int create_and(int fanin0, int fanin1);
    void remove_node(int node_id);
    void replace_node(int old_node, int new_node);
    void remove_mffc(int node_id);  // Remove Maximum Fanout-Free Cone
    
    // Simulation
    void simulate(const std::vector<uint64_t>& pi_values);
    uint64_t get_sim_value(int node_id) const;
    
    
    // Multi-word truth table computation for windows
    // Returns vector<vector<word>> where:
    // - results[0..n-1] = divisors[0..n-1] truth tables
    // - results[n] = target truth table (at the end)
    std::vector<std::vector<uint64_t>> compute_truth_tables_for_window(
        int target_node,
        const std::vector<int>& window_inputs,
        const std::vector<int>& window_nodes, 
        const std::vector<int>& divisors,
        bool verbose = false) const;
    
    // Structural operations
    void build_fanouts();
    void compute_levels();
    int get_level(int node_id) const;
    
    // Utility functions
    static inline int lit2var(int lit) { return lit >> 1; }
    static inline int var2lit(int var, bool comp = false) { return (var << 1) | (comp ? 1 : 0); }
    static inline bool is_complemented(int lit) { return lit & 1; }
    static inline int complement(int lit) { return lit ^ 1; }
    
private:
};

} // namespace fresub