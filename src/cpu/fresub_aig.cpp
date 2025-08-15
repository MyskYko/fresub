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


uint64_t AIG::get_sim_value(int node_id) const {
    if (node_id < 0 || node_id >= static_cast<int>(sim_values.size())) return 0;
    return sim_values[node_id];
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


std::vector<std::vector<uint64_t>> AIG::compute_truth_tables_for_window(
    int target_node,
    const std::vector<int>& window_inputs,
    const std::vector<int>& window_nodes,
    const std::vector<int>& divisors,
    bool verbose) const {
    
    if (verbose) {
        std::cout << "\n--- COMPUTING TRUTH TABLES FOR WINDOW ---\n";
        std::cout << "Target: " << target_node << "\n";
        std::cout << "Window inputs: [";
        for (size_t i = 0; i < window_inputs.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << window_inputs[i];
        }
        std::cout << "]\nWindow nodes: [";
        for (size_t i = 0; i < window_nodes.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << window_nodes[i];
        }
        std::cout << "]\nDivisors: [";
        for (size_t i = 0; i < divisors.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << divisors[i];
        }
        std::cout << "]\n";
    }
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 20) {  // Reasonable limit for memory usage
        std::cout << "ERROR: Too many inputs (" << num_inputs << ") for truth table computation\n";
        return {};
    }
    
    size_t num_patterns = 1ULL << num_inputs;
    size_t num_words = (num_patterns + 63) / 64;  // Ceiling division
    
    if (verbose) {
        std::cout << "Truth table size: " << num_patterns << " patterns = " 
                  << num_words << " words of 64 bits\n";
    }
    
    std::unordered_map<int, std::vector<uint64_t>> node_tt;
    
    // Initialize primary input truth tables
    if (verbose) std::cout << "Initializing primary input truth tables:\n";
    for (int i = 0; i < num_inputs; i++) {
        int pi = window_inputs[i];
        std::vector<uint64_t> pattern(num_words, 0);
        
        // Create bit pattern: input i toggles every 2^i positions
        for (size_t p = 0; p < num_patterns; p++) {
            if ((p >> i) & 1) {
                size_t word_idx = p / 64;
                size_t bit_idx = p % 64;
                pattern[word_idx] |= (1ULL << bit_idx);
            }
        }
        
        node_tt[pi] = pattern;
        if (verbose) {
            std::cout << "  Input " << pi << " (bit " << i << "): ";
            // Print first word for debugging (if reasonable size)
            if (num_patterns <= 64) {
                for (int b = num_patterns - 1; b >= 0; b--) {
                    std::cout << ((pattern[0] >> b) & 1);
                }
                std::cout << " (0x" << std::hex << pattern[0] << std::dec << ")";
            } else {
                std::cout << "[" << num_words << " words, " << num_patterns << " patterns]";
            }
            std::cout << "\n";
        }
    }
    
    // Process window nodes in topological order
    if (verbose) std::cout << "\nProcessing window nodes:\n";
    for (int current_node : window_nodes) {
        if (std::find(window_inputs.begin(), window_inputs.end(), current_node) != window_inputs.end()) {
            continue; // Skip inputs, already processed
        }
        
        if (current_node >= static_cast<int>(nodes.size()) || nodes[current_node].is_dead) {
            if (verbose) std::cout << "  Node " << current_node << ": SKIPPED (dead)\n";
            continue;
        }
        
        int fanin0 = lit2var(nodes[current_node].fanin0);
        int fanin1 = lit2var(nodes[current_node].fanin1);
        bool comp0 = is_complemented(nodes[current_node].fanin0);
        bool comp1 = is_complemented(nodes[current_node].fanin1);
        
        // Get fanin truth tables (multi-word)
        std::vector<uint64_t> tt0(num_words, 0);
        std::vector<uint64_t> tt1(num_words, 0);
        
        if (node_tt.find(fanin0) != node_tt.end()) {
            tt0 = node_tt[fanin0];
        }
        if (node_tt.find(fanin1) != node_tt.end()) {
            tt1 = node_tt[fanin1];
        }
        
        // Apply complementation and compute AND word by word
        std::vector<uint64_t> result_tt(num_words);
        for (size_t w = 0; w < num_words; w++) {
            uint64_t val0 = comp0 ? ~tt0[w] : tt0[w];
            uint64_t val1 = comp1 ? ~tt1[w] : tt1[w];
            result_tt[w] = val0 & val1;
        }
        
        node_tt[current_node] = result_tt;
        
        if (verbose) {
            std::cout << "  Node " << current_node << " = AND(";
            std::cout << fanin0 << (comp0 ? "'" : "") << ", ";
            std::cout << fanin1 << (comp1 ? "'" : "") << "):\n";
            if (num_patterns <= 64) {
                std::cout << "    ";
                for (int b = num_patterns - 1; b >= 0; b--) {
                    std::cout << ((result_tt[0] >> b) & 1);
                }
                std::cout << " (0x" << std::hex << result_tt[0] << std::dec << ")";
            } else {
                std::cout << "    [" << num_words << " words computed]";
            }
            std::cout << "\n";
        }
    }
    
    // Extract results as vector<vector<word>>
    // results[0..n-1] = divisors[0..n-1], results[n] = target
    std::vector<std::vector<uint64_t>> results;
    
    // Add all divisors first (indices 0..n-1)
    for (int divisor : divisors) {
        if (node_tt.find(divisor) != node_tt.end()) {
            results.push_back(node_tt[divisor]);
        } else {
            results.push_back(std::vector<uint64_t>(num_words, 0)); // Default to all zeros
        }
    }
    
    // Add target node at the end (index n)
    if (node_tt.find(target_node) != node_tt.end()) {
        results.push_back(node_tt[target_node]);
    } else {
        results.push_back(std::vector<uint64_t>(num_words, 0)); // Default to all zeros
    }
    
    if (verbose) {
        std::cout << "\nExtracted truth tables as vector<vector<word>>:\n";
        for (size_t i = 0; i < divisors.size(); i++) {
            std::cout << "  results[" << i << "] = divisor " << divisors[i] 
                      << " (" << results[i].size() << " words)\n";
        }
        std::cout << "  results[" << divisors.size() << "] = target " << target_node 
                  << " (" << results[divisors.size()].size() << " words)\n";
        std::cout << "  Total: " << results.size() << " truth tables\n";
    }
    
    return results;
}

} // namespace fresub
