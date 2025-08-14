#include "fresub_aig.hpp"
#include <fstream>
#include <iostream>
#include <queue>
#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace fresub {

// Helper functions for binary AIGER reading (copied from exopt)
static inline unsigned char getnoneofch(std::ifstream& file) {
    int ch = file.get();
    assert(ch != EOF);
    return ch;
}

static inline unsigned decode(std::ifstream& file) {
    unsigned x = 0, i = 0;
    unsigned char ch;
    while ((ch = getnoneofch(file)) & 0x80) {
        x |= (ch & 0x7f) << (7 * i++);
    }
    return x | (ch << (7 * i));
}

static inline void encode(std::ofstream& file, unsigned x) {
    unsigned char ch;
    while (x & ~0x7f) {
        ch = (x & 0x7f) | 0x80;
        file.put(ch);
        x >>= 7;
    }
    ch = x;
    file.put(ch);
}

AIG::AIG() : num_pis(0), num_pos(0), num_nodes(1) {
    // Reserve node 0 as constant
    nodes.push_back(Node{0, 0, 0, {}, false});
}

AIG::AIG(const std::string& filename) : AIG() {
    read_aiger(filename);
}

void AIG::read_aiger(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string header;
    file >> header;
    if (header != "aig" && header != "aag") {
        throw std::runtime_error("Invalid AIGER format");
    }
    
    bool is_binary = (header == "aig");
    
    int M, I, L, O, A;
    file >> M >> I >> L >> O >> A;
    
    if (L != 0) {
        throw std::runtime_error("Sequential circuits not supported");
    }
    
    num_pis = I;
    num_pos = O;
    num_nodes = M + 1;  // +1 for constant node
    
    nodes.clear();
    nodes.resize(num_nodes);
    pos.clear();
    pos.resize(num_pos);
    
    // Initialize all nodes
    for (int i = 0; i < num_nodes; i++) {
        nodes[i] = Node{0, 0, 0, {}, false};
    }
    
    // Read outputs
    for (int i = 0; i < num_pos; i++) {
        file >> pos[i];
    }
    
    // Skip newline after outputs before binary section
    if (is_binary) {
        file.ignore(); // Skip the newline after last PO
    }
    
    // Read AND gates
    if (is_binary) {
        // Binary format - delta encoding
        for (int i = num_pis + 1; i < num_nodes; i++) {
            unsigned delta0, delta1;
            // Read delta-encoded values
            unsigned char c;
            delta0 = delta1 = 0;
            
            // Use exopt's decode approach - read deltas directly
            delta0 = decode(file);
            delta1 = decode(file);
            // Exopt approach: vObjs[i+i] = i+i - decode(f); vObjs[i+i+1] = vObjs[i+i] - decode(f);
            nodes[i].fanin0 = i + i - delta0;
            nodes[i].fanin1 = nodes[i].fanin0 - delta1;
        }
    } else {
        // ASCII format
        for (int i = num_pis + 1; i < num_nodes; i++) {
            int lhs, rhs0, rhs1;
            file >> lhs >> rhs0 >> rhs1;
            assert(lit2var(lhs) == i);
            nodes[i].fanin0 = rhs0;
            nodes[i].fanin1 = rhs1;
        }
    }
    
    build_fanouts();
    compute_levels();
}

void AIG::write_aiger(const std::string& filename, bool binary) const {
    if (binary) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot create file: " + filename);
        }
        
        // Write header in ASCII (same for both formats)
        file << "aig " << (num_nodes - 1) << " " << num_pis << " 0 " 
             << num_pos << " " << (num_nodes - num_pis - 1) << "\n";
        
        // Write outputs in ASCII
        for (int po : pos) {
            file << po << "\n";
        }
        
        // Write AND gates in binary format (delta-encoded)
        for (int i = num_pis + 1; i < num_nodes; i++) {
            if (!nodes[i].is_dead) {
                unsigned lhs = 2 * i;
                unsigned delta0 = lhs - static_cast<unsigned>(nodes[i].fanin0);
                unsigned delta1 = static_cast<unsigned>(nodes[i].fanin0) - static_cast<unsigned>(nodes[i].fanin1);
                
                encode(file, delta0);
                encode(file, delta1);
            }
        }
    } else {
        // ASCII format
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Cannot create file: " + filename);
        }
        
        file << "aag " << (num_nodes - 1) << " " << num_pis << " 0 " 
             << num_pos << " " << (num_nodes - num_pis - 1) << "\n";
        
        // Write outputs
        for (int po : pos) {
            file << po << "\n";
        }
        
        // Write AND gates
        for (int i = num_pis + 1; i < num_nodes; i++) {
            if (!nodes[i].is_dead) {
                file << var2lit(i) << " " << nodes[i].fanin0 << " " 
                     << nodes[i].fanin1 << "\n";
            }
        }
    }
}

int AIG::create_and(int fanin0, int fanin1) {
    // Normalize order
    if (fanin0 > fanin1) {
        std::swap(fanin0, fanin1);
    }
    
    // Check for trivial cases
    if (fanin0 == 0) return 0;  // 0 AND x = 0
    if (fanin0 == 1) return fanin1;  // 1 AND x = x
    if (fanin0 == fanin1) return fanin0;  // x AND x = x
    if (fanin0 == complement(fanin1)) return 0;  // x AND !x = 0
    
    // Create new AND node
    Node new_node;
    new_node.fanin0 = fanin0;
    new_node.fanin1 = fanin1;
    new_node.is_dead = false;
    new_node.level = std::max(get_level(lit2var(fanin0)), 
                              get_level(lit2var(fanin1))) + 1;
    
    nodes.push_back(new_node);
    int new_id = nodes.size() - 1;
    
    // Update fanouts
    nodes[lit2var(fanin0)].fanouts.push_back(new_id);
    nodes[lit2var(fanin1)].fanouts.push_back(new_id);
    
    num_nodes++;
    return var2lit(new_id);
}

void AIG::remove_node(int node_id) {
    if (node_id <= num_pis || node_id >= num_nodes) return;
    
    nodes[node_id].is_dead = true;
    
    // Remove from fanouts of fanins
    int fanin0_var = lit2var(nodes[node_id].fanin0);
    int fanin1_var = lit2var(nodes[node_id].fanin1);
    
    auto& fo0 = nodes[fanin0_var].fanouts;
    fo0.erase(std::remove(fo0.begin(), fo0.end(), node_id), fo0.end());
    
    auto& fo1 = nodes[fanin1_var].fanouts;
    fo1.erase(std::remove(fo1.begin(), fo1.end(), node_id), fo1.end());
}

void AIG::remove_mffc(int node_id) {
    if (node_id <= num_pis || node_id >= num_nodes) return;
    
    std::vector<int> mffc;
    std::unordered_set<int> visited;
    std::queue<int> to_process;
    
    // Start with the target node
    to_process.push(node_id);
    visited.insert(node_id);
    
    while (!to_process.empty()) {
        int current = to_process.front();
        to_process.pop();
        
        if (current <= num_pis) {
            continue; // Don't include PIs in MFFC
        }
        
        mffc.push_back(current);
        
        // Check if this node has fanout > 1 (not in MFFC except for original target)
        if (current != node_id && nodes[current].fanouts.size() > 1) {
            continue;
        }
        
        // Add fanins to processing queue if they're in the fanout-free cone
        int fanin0_node = lit2var(nodes[current].fanin0);
        int fanin1_node = lit2var(nodes[current].fanin1);
        
        if (fanin0_node > num_pis && visited.find(fanin0_node) == visited.end()) {
            // Check if fanin0 has only this node as fanout
            bool single_fanout = (nodes[fanin0_node].fanouts.size() == 1);
            if (single_fanout) {
                to_process.push(fanin0_node);
                visited.insert(fanin0_node);
            }
        }
        
        if (fanin1_node > num_pis && visited.find(fanin1_node) == visited.end()) {
            // Check if fanin1 has only this node as fanout
            bool single_fanout = (nodes[fanin1_node].fanouts.size() == 1);
            if (single_fanout) {
                to_process.push(fanin1_node);
                visited.insert(fanin1_node);
            }
        }
    }
    
    // Remove all nodes in MFFC
    std::cout << "  Removing MFFC for node " << node_id << ": {";
    for (size_t i = 0; i < mffc.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << mffc[i];
        remove_node(mffc[i]);
    }
    std::cout << "}\n";
}

void AIG::replace_node(int old_node, int new_node) {
    // Update all fanouts of old_node to point to new_node
    for (int fanout : nodes[old_node].fanouts) {
        if (lit2var(nodes[fanout].fanin0) == old_node) {
            bool comp = is_complemented(nodes[fanout].fanin0);
            nodes[fanout].fanin0 = var2lit(new_node, comp);
        }
        if (lit2var(nodes[fanout].fanin1) == old_node) {
            bool comp = is_complemented(nodes[fanout].fanin1);
            nodes[fanout].fanin1 = var2lit(new_node, comp);
        }
        nodes[new_node].fanouts.push_back(fanout);
    }
    
    // Update POs
    for (int& po : pos) {
        if (lit2var(po) == old_node) {
            bool comp = is_complemented(po);
            po = var2lit(new_node, comp);
        }
    }
    
    remove_mffc(old_node);
}

void AIG::simulate(const std::vector<uint64_t>& pi_values) {
    sim_values.resize(num_nodes);
    
    // Set constant and PI values
    sim_values[0] = 0;
    for (int i = 1; i <= num_pis && i - 1 < static_cast<int>(pi_values.size()); i++) {
        sim_values[i] = pi_values[i - 1];
    }
    
    // Simulate AND gates in topological order
    for (int i = num_pis + 1; i < num_nodes; i++) {
        if (i >= static_cast<int>(nodes.size()) || nodes[i].is_dead) continue;
        
        int var0 = lit2var(nodes[i].fanin0);
        int var1 = lit2var(nodes[i].fanin1);
        
        // Bounds check to prevent crashes
        if (var0 < 0 || var0 >= num_nodes || var1 < 0 || var1 >= num_nodes) {
            sim_values[i] = 0;
            continue;
        }
        
        uint64_t val0 = sim_values[var0];
        if (is_complemented(nodes[i].fanin0)) val0 = ~val0;
        
        uint64_t val1 = sim_values[var1];
        if (is_complemented(nodes[i].fanin1)) val1 = ~val1;
        
        sim_values[i] = val0 & val1;
    }
}

std::vector<uint64_t> AIG::simulate_threadsafe(const std::vector<uint64_t>& pi_values) const {
    // Thread-safe simulation that returns results without modifying shared state
    std::vector<uint64_t> local_sim_values(num_nodes);
    
    // Set constant and PI values
    local_sim_values[0] = 0;
    for (int i = 1; i <= num_pis && i - 1 < static_cast<int>(pi_values.size()); i++) {
        local_sim_values[i] = pi_values[i - 1];
    }
    
    // Simulate AND gates in topological order
    for (int i = num_pis + 1; i < num_nodes; i++) {
        if (i >= static_cast<int>(nodes.size()) || nodes[i].is_dead) continue;
        
        int var0 = lit2var(nodes[i].fanin0);
        int var1 = lit2var(nodes[i].fanin1);
        
        // Bounds check to prevent crashes
        if (var0 < 0 || var0 >= num_nodes || var1 < 0 || var1 >= num_nodes) {
            local_sim_values[i] = 0;
            continue;
        }
        
        uint64_t val0 = local_sim_values[var0];
        if (is_complemented(nodes[i].fanin0)) val0 = ~val0;
        
        uint64_t val1 = local_sim_values[var1];
        if (is_complemented(nodes[i].fanin1)) val1 = ~val1;
        
        local_sim_values[i] = val0 & val1;
    }
    
    return local_sim_values;
}

uint64_t AIG::get_sim_value(int node_id) const {
    if (node_id < 0 || node_id >= static_cast<int>(sim_values.size())) return 0;
    return sim_values[node_id];
}

std::unordered_map<int, uint64_t> AIG::simulate_window_bitparallel(
    const std::vector<int>& window_inputs,
    const std::vector<int>& window_nodes, 
    const std::vector<int>& target_nodes) const {
    
    std::unordered_map<int, uint64_t> sim_values;
    
    // Handle up to 6 inputs (64 patterns fit in uint64_t)
    int num_inputs = std::min(static_cast<int>(window_inputs.size()), 6);
    if (num_inputs == 0) return sim_values;
    
    // Generate standard bit-parallel patterns
    // input0: 01010101... (alternating)
    // input1: 00110011... (2-bit groups)  
    // input2: 00001111... (4-bit groups)
    // input3: 00000000111111111... (8-bit groups)
    // etc.
    
    for (int i = 0; i < num_inputs; i++) {
        uint64_t pattern = 0;
        int group_size = 1 << i;  // 1, 2, 4, 8, 16, 32
        
        for (int bit = 0; bit < 64; bit++) {
            if ((bit / group_size) & 1) {
                pattern |= (1ULL << bit);
            }
        }
        
        sim_values[window_inputs[i]] = pattern;
    }
    
    // Set constant node
    sim_values[0] = 0;
    
    // Create a set of all nodes we need to compute for quick lookup
    std::unordered_set<int> nodes_to_compute;
    for (int node : window_nodes) nodes_to_compute.insert(node);
    for (int node : target_nodes) nodes_to_compute.insert(node);
    
    // For now, let's build a proper dependency closure
    // Add all nodes in the transitive fanin of target nodes
    std::unordered_set<int> all_needed_nodes;
    
    // Add target nodes
    for (int node : target_nodes) {
        if (node >= 0 && node < num_nodes) {
            all_needed_nodes.insert(node);
        }
    }
    
    // Add window nodes
    for (int node : window_nodes) {
        if (node >= 0 && node < num_nodes) {
            all_needed_nodes.insert(node);
        }
    }
    
    // Iteratively add dependencies until we reach window inputs or PIs
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<int> current_nodes(all_needed_nodes.begin(), all_needed_nodes.end());
        
        for (int node : current_nodes) {
            if (node <= num_pis || node >= static_cast<int>(nodes.size())) continue;
            if (nodes[node].is_dead) continue;
            
            int var0 = lit2var(nodes[node].fanin0);
            int var1 = lit2var(nodes[node].fanin1);
            
            // Add fanins if they're not window inputs and not already included
            if (var0 >= 0 && var0 < num_nodes && 
                std::find(window_inputs.begin(), window_inputs.end(), var0) == window_inputs.end()) {
                if (all_needed_nodes.insert(var0).second) {
                    changed = true;
                }
            }
            
            if (var1 >= 0 && var1 < num_nodes && 
                std::find(window_inputs.begin(), window_inputs.end(), var1) == window_inputs.end()) {
                if (all_needed_nodes.insert(var1).second) {
                    changed = true;
                }
            }
        }
    }
    
    // Simulate nodes in topological order
    for (int i = 1; i < num_nodes; i++) {
        if (i >= static_cast<int>(nodes.size()) || nodes[i].is_dead) continue;
        
        // Skip if this node is not needed
        if (all_needed_nodes.find(i) == all_needed_nodes.end()) continue;
            
        // If it's a PI/input, skip (already set above or not in window)
        if (i <= num_pis) continue;
        
        // Get fanin values
        int var0 = lit2var(nodes[i].fanin0);
        int var1 = lit2var(nodes[i].fanin1);
        
        // Bounds check
        if (var0 < 0 || var0 >= num_nodes || var1 < 0 || var1 >= num_nodes) continue;
        
        // Skip if fanins are not available
        if (sim_values.find(var0) == sim_values.end() || 
            sim_values.find(var1) == sim_values.end()) continue;
            
        uint64_t val0 = sim_values[var0];
        if (is_complemented(nodes[i].fanin0)) val0 = ~val0;
        
        uint64_t val1 = sim_values[var1];  
        if (is_complemented(nodes[i].fanin1)) val1 = ~val1;
        
        sim_values[i] = val0 & val1;
    }
    
    return sim_values;
}

std::unordered_map<int, uint64_t> AIG::simulate_window_stateless(
    const std::vector<int>& window_inputs,
    const std::vector<int>& nodes_to_compute,
    const std::vector<std::pair<int, std::pair<int, int>>>& node_definitions) {
    
    std::unordered_map<int, uint64_t> sim_values;
    
    // Handle up to 6 inputs (64 patterns fit in uint64_t)
    int num_inputs = std::min(static_cast<int>(window_inputs.size()), 6);
    if (num_inputs == 0) return sim_values;
    
    // Generate standard bit-parallel patterns - NO SHARED STATE ACCESS
    for (int i = 0; i < num_inputs; i++) {
        uint64_t pattern = 0;
        int group_size = 1 << i;  // 1, 2, 4, 8, 16, 32
        
        for (int bit = 0; bit < 64; bit++) {
            if ((bit / group_size) & 1) {
                pattern |= (1ULL << bit);
            }
        }
        
        sim_values[window_inputs[i]] = pattern;
    }
    
    // Set constant node
    sim_values[0] = 0;
    
    // Simulate nodes using provided definitions - STATELESS
    for (const auto& node_def : node_definitions) {
        int node_id = node_def.first;
        int fanin0 = node_def.second.first;
        int fanin1 = node_def.second.second;
        
        // Extract variables and complements from literals
        int var0 = fanin0 >> 1;
        int var1 = fanin1 >> 1;
        bool comp0 = fanin0 & 1;
        bool comp1 = fanin1 & 1;
        
        // Skip if fanins are not available
        if (sim_values.find(var0) == sim_values.end() || 
            sim_values.find(var1) == sim_values.end()) continue;
            
        uint64_t val0 = sim_values[var0];
        if (comp0) val0 = ~val0;
        
        uint64_t val1 = sim_values[var1];  
        if (comp1) val1 = ~val1;
        
        sim_values[node_id] = val0 & val1;
    }
    
    return sim_values;
}

void AIG::build_fanouts() {
    // Clear existing fanouts
    for (auto& node : nodes) {
        node.fanouts.clear();
    }
    
    // Build fanout lists
    for (int i = num_pis + 1; i < num_nodes; i++) {
        if (i >= static_cast<int>(nodes.size())) {
            std::cerr << "Error: node index " << i << " out of bounds (size=" << nodes.size() << ")\n";
            break;
        }
        if (nodes[i].is_dead) continue;
        
        int fanin0_var = lit2var(nodes[i].fanin0);
        int fanin1_var = lit2var(nodes[i].fanin1);
        
        // Skip nodes with negative fanins for now (these are likely encoding issues)
        if (fanin0_var < 0 || fanin0_var >= static_cast<int>(nodes.size()) ||
            fanin1_var < 0 || fanin1_var >= static_cast<int>(nodes.size())) {
            continue;
        }
        
        nodes[fanin0_var].fanouts.push_back(i);
        if (fanin0_var != fanin1_var) {
            nodes[fanin1_var].fanouts.push_back(i);
        }
    }
}

void AIG::compute_levels() {
    // Initialize levels
    for (auto& node : nodes) {
        node.level = 0;
    }
    
    // Compute levels in topological order
    for (int i = num_pis + 1; i < num_nodes; i++) {
        if (nodes[i].is_dead) continue;
        
        int level0 = nodes[lit2var(nodes[i].fanin0)].level;
        int level1 = nodes[lit2var(nodes[i].fanin1)].level;
        nodes[i].level = std::max(level0, level1) + 1;
    }
}

int AIG::get_level(int node_id) const {
    if (node_id >= nodes.size()) return 0;
    return nodes[node_id].level;
}

void AIG::topological_sort(std::vector<int>& sorted_nodes) const {
    sorted_nodes.clear();
    std::vector<bool> visited(num_nodes, false);
    
    // DFS from each PO
    for (int po : pos) {
        dfs_mark(lit2var(po), visited, sorted_nodes);
    }
    
    // Reverse to get topological order
    std::reverse(sorted_nodes.begin(), sorted_nodes.end());
}

void AIG::dfs_mark(int node_id, std::vector<bool>& visited, 
                    std::vector<int>& result) const {
    if (visited[node_id] || node_id <= num_pis) return;
    visited[node_id] = true;
    
    if (!nodes[node_id].is_dead) {
        dfs_mark(lit2var(nodes[node_id].fanin0), visited, result);
        dfs_mark(lit2var(nodes[node_id].fanin1), visited, result);
        result.push_back(node_id);
    }
}

void AIG::get_cone(int root, std::vector<int>& cone_nodes) const {
    cone_nodes.clear();
    std::vector<bool> visited(num_nodes, false);
    dfs_mark(root, visited, cone_nodes);
}

} // namespace fresub
