#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <unordered_set>
#include <queue>
#include <bitset>
#include <iomanip>
#include "fresub_aig.hpp"
#include "window.hpp"

using namespace fresub;

void print_cut(const Cut& cut, const AIG& aig) {
    std::cout << "{";
    for (size_t i = 0; i < cut.leaves.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << cut.leaves[i];
        if (cut.leaves[i] <= aig.num_pis) {
            std::cout << "(PI)";
        }
    }
    std::cout << "}";
}

void print_node_vector(const std::vector<int>& nodes) {
    std::cout << "[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

// Print truth table in detailed format with input patterns
void print_truth_table_detailed(uint64_t tt, const std::vector<int>& inputs, const std::string& label) {
    int num_inputs = inputs.size();
    int num_patterns = 1 << num_inputs;
    
    std::cout << label << ":\n";
    
    // Print header
    std::cout << "  ";
    for (int input : inputs) {
        std::cout << std::setw(4) << input;
    }
    std::cout << " | Output\n";
    std::cout << "  " << std::string(num_inputs * 4, '-') << "-+-------\n";
    
    // Print each pattern
    for (int p = 0; p < num_patterns; p++) {
        std::cout << "  ";
        for (int i = num_inputs - 1; i >= 0; i--) {
            std::cout << std::setw(4) << ((p >> i) & 1);
        }
        std::cout << " |   " << ((tt >> p) & 1) << "\n";
    }
    
    std::cout << "  Binary: ";
    for (int i = num_patterns - 1; i >= 0; i--) {
        std::cout << ((tt >> i) & 1);
    }
    std::cout << " (0x" << std::hex << tt << std::dec << ")";
    std::cout << " [" << __builtin_popcountll(tt) << " ones]\n";
}

// Compute truth table for a node within a window
uint64_t compute_truth_table_for_node(const AIG& aig, int node, 
                                      const std::vector<int>& window_inputs,
                                      const std::vector<int>& window_nodes,
                                      std::map<int, uint64_t>& all_tts) {
    
    int num_inputs = window_inputs.size();
    if (num_inputs > 6) return 0;
    
    int num_patterns = 1 << num_inputs;
    
    // Initialize primary input truth tables
    for (int i = 0; i < num_inputs; i++) {
        int pi = window_inputs[i];
        uint64_t pattern = 0;
        for (int p = 0; p < num_patterns; p++) {
            if ((p >> i) & 1) {
                pattern |= (1ULL << p);
            }
        }
        all_tts[pi] = pattern;
    }
    
    // Simulate internal nodes in topological order
    for (int current_node : window_nodes) {
        if (current_node <= aig.num_pis) continue;
        if (current_node >= static_cast<int>(aig.nodes.size()) || aig.nodes[current_node].is_dead) continue;
        
        uint32_t fanin0_lit = aig.nodes[current_node].fanin0;
        uint32_t fanin1_lit = aig.nodes[current_node].fanin1;
        
        int fanin0 = aig.lit2var(fanin0_lit);
        int fanin1 = aig.lit2var(fanin1_lit);
        
        bool fanin0_compl = aig.is_complemented(fanin0_lit);
        bool fanin1_compl = aig.is_complemented(fanin1_lit);
        
        if (all_tts.find(fanin0) == all_tts.end() || all_tts.find(fanin1) == all_tts.end()) {
            continue;
        }
        
        uint64_t tt0 = all_tts[fanin0];
        uint64_t tt1 = all_tts[fanin1];
        
        if (fanin0_compl) tt0 = ~tt0 & ((1ULL << num_patterns) - 1);
        if (fanin1_compl) tt1 = ~tt1 & ((1ULL << num_patterns) - 1);
        
        uint64_t result_tt = tt0 & tt1;
        all_tts[current_node] = result_tt;
    }
    
    return all_tts.find(node) != all_tts.end() ? all_tts[node] : 0;
}

void analyze_complex_window(const AIG& aig, const Window& window, int window_id) {
    std::cout << std::string(70, '=') << "\n";
    std::cout << "COMPLEX WINDOW ANALYSIS: WINDOW " << window_id << " - TARGET " << window.target_node << "\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    // Basic window info
    std::cout << "WINDOW STRUCTURE:\n";
    std::cout << "  Cut: ";
    Cut cut;
    cut.leaves = window.inputs;
    print_cut(cut, aig);
    std::cout << " (" << window.inputs.size() << " inputs)\n";
    
    std::cout << "  Window nodes: ";
    print_node_vector(window.nodes);
    std::cout << " (" << window.nodes.size() << " total)\n";
    
    std::cout << "  Divisors: ";
    print_node_vector(window.divisors);
    std::cout << " (" << window.divisors.size() << " available)\n\n";
    
    // Show AIG structure within window
    std::cout << "AIG STRUCTURE WITHIN WINDOW:\n";
    for (int node : window.nodes) {
        if (node <= aig.num_pis) {
            std::cout << "  Node " << node << " = PI\n";
        } else if (node < static_cast<int>(aig.nodes.size()) && !aig.nodes[node].is_dead) {
            int fanin0 = aig.lit2var(aig.nodes[node].fanin0);
            int fanin1 = aig.lit2var(aig.nodes[node].fanin1);
            
            std::cout << "  Node " << node << " = AND(";
            std::cout << fanin0;
            if (aig.is_complemented(aig.nodes[node].fanin0)) std::cout << "'";
            std::cout << ", " << fanin1;
            if (aig.is_complemented(aig.nodes[node].fanin1)) std::cout << "'";
            std::cout << ")";
            
            // Mark if it's a divisor or in TFO
            if (std::find(window.divisors.begin(), window.divisors.end(), node) != window.divisors.end()) {
                std::cout << " [DIVISOR]";
            } else if (node == window.target_node) {
                std::cout << " [TARGET]";
            } else {
                std::cout << " [TFO/MFFC]";
            }
            std::cout << "\n";
        }
    }
    
    // Compute all truth tables
    std::cout << "\nTRUTH TABLE COMPUTATION:\n";
    std::map<int, uint64_t> all_tts;
    uint64_t target_tt = compute_truth_table_for_node(aig, window.target_node, 
                                                     window.inputs, window.nodes, all_tts);
    
    // Show target truth table in detail
    print_truth_table_detailed(target_tt, window.inputs, "TARGET " + std::to_string(window.target_node));
    std::cout << "\n";
    
    // Show divisor truth tables
    std::cout << "DIVISOR TRUTH TABLES:\n";
    for (int divisor : window.divisors) {
        if (all_tts.find(divisor) != all_tts.end()) {
            std::cout << "Divisor " << divisor << ": ";
            int num_patterns = 1 << window.inputs.size();
            for (int i = num_patterns - 1; i >= 0; i--) {
                std::cout << ((all_tts[divisor] >> i) & 1);
            }
            std::cout << " (0x" << std::hex << all_tts[divisor] << std::dec << ")";
            std::cout << " [" << __builtin_popcountll(all_tts[divisor]) << " ones]";
            
            // Show what this divisor represents
            if (divisor <= aig.num_pis) {
                std::cout << " (Primary Input)";
            } else if (divisor < static_cast<int>(aig.nodes.size()) && !aig.nodes[divisor].is_dead) {
                int fanin0 = aig.lit2var(aig.nodes[divisor].fanin0);
                int fanin1 = aig.lit2var(aig.nodes[divisor].fanin1);
                std::cout << " (";
                std::cout << fanin0;
                if (aig.is_complemented(aig.nodes[divisor].fanin0)) std::cout << "'";
                std::cout << " AND ";
                std::cout << fanin1;
                if (aig.is_complemented(aig.nodes[divisor].fanin1)) std::cout << "'";
                std::cout << ")";
            }
            std::cout << "\n";
        }
    }
    
    // Detailed resubstitution analysis
    std::cout << "\nCOMPREHENSIVE RESUBSTITUTION ANALYSIS:\n";
    
    // 0-resub
    std::cout << "\n0-RESUB (target = divisor or NOT divisor):\n";
    bool found_0resub = false;
    for (int divisor : window.divisors) {
        if (all_tts.find(divisor) != all_tts.end()) {
            uint64_t div_tt = all_tts[divisor];
            uint64_t mask = (1ULL << (1 << window.inputs.size())) - 1;
            
            if (div_tt == target_tt) {
                std::cout << "  ✓ FOUND: " << window.target_node << " = " << divisor << " (exact match)\n";
                found_0resub = true;
            }
            
            uint64_t div_tt_compl = (~div_tt) & mask;
            if (div_tt_compl == target_tt) {
                std::cout << "  ✓ FOUND: " << window.target_node << " = NOT(" << divisor << ")\n";
                found_0resub = true;
            }
        }
    }
    if (!found_0resub) {
        std::cout << "  No 0-resub opportunities\n";
    }
    
    // 1-resub comprehensive
    std::cout << "\n1-RESUB (target = div1 OP div2) - comprehensive search:\n";
    int resub_count = 0;
    for (size_t i = 0; i < window.divisors.size(); i++) {
        for (size_t j = i + 1; j < window.divisors.size(); j++) {
            int div1 = window.divisors[i];
            int div2 = window.divisors[j];
            
            if (all_tts.find(div1) == all_tts.end() || all_tts.find(div2) == all_tts.end()) continue;
            
            uint64_t tt1 = all_tts[div1];
            uint64_t tt2 = all_tts[div2];
            uint64_t mask = (1ULL << (1 << window.inputs.size())) - 1;
            
            struct {
                uint64_t result;
                const char* op_name;
                const char* description;
            } combinations[] = {
                {tt1 & tt2, "AND", "both must be 1"},
                {tt1 | tt2, "OR", "either can be 1"},
                {tt1 ^ tt2, "XOR", "exactly one is 1"},
                {~(tt1 ^ tt2) & mask, "XNOR", "both same value"},
                {(tt1 & ~tt2) & mask, "AND NOT", "first=1, second=0"},
                {(~tt1 & tt2) & mask, "NOT AND", "first=0, second=1"},
                {(~tt1 & ~tt2) & mask, "NOR", "both must be 0"},
                {~(tt1 & tt2) & mask, "NAND", "not both 1"},
                {(~tt1 | tt2) & mask, "IMPLIES", "if first then second"},
                {(tt1 | ~tt2) & mask, "REVERSE IMPLIES", "if second then first"}
            };
            
            for (auto& combo : combinations) {
                if (combo.result == target_tt) {
                    std::cout << "  ✓ FOUND: " << window.target_node << " = " 
                             << div1 << " " << combo.op_name << " " << div2 
                             << " (" << combo.description << ")\n";
                    resub_count++;
                    
                    if (resub_count >= 3) break; // Limit for readability
                }
            }
            if (resub_count >= 3) break;
        }
        if (resub_count >= 3) break;
    }
    
    if (resub_count == 0) {
        std::cout << "  No 1-resub opportunities found\n";
    }
    
    // Show optimization impact
    std::cout << "\nOPTIMIZATION IMPACT ANALYSIS:\n";
    std::cout << "  Search space: " << window.divisors.size() << " divisors\n";
    std::cout << "  0-resub combinations: " << (window.divisors.size() * 2) << " (direct + complement)\n";
    std::cout << "  1-resub combinations: " << (window.divisors.size() * (window.divisors.size() - 1) / 2 * 10) << " (pairs × 10 operations)\n";
    
    // Function analysis
    int target_ones = __builtin_popcountll(target_tt);
    int total_patterns = 1 << window.inputs.size();
    std::cout << "  Function complexity: " << target_ones << "/" << total_patterns 
             << " patterns are 1 (" << (100.0 * target_ones / total_patterns) << "%)\n";
    
    if (target_ones == 1) {
        std::cout << "  → Very sparse function (only 1 minterm)\n";
    } else if (target_ones <= total_patterns / 4) {
        std::cout << "  → Sparse function (AND-like logic)\n";
    } else if (target_ones >= 3 * total_patterns / 4) {
        std::cout << "  → Dense function (OR-like logic)\n";
    } else {
        std::cout << "  → Moderate complexity function\n";
    }
    
    std::cout << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.aig> [max_cut_size]\n";
        return 1;
    }
    
    std::string input_file = argv[1];
    int max_cut_size = (argc > 2) ? std::atoi(argv[2]) : 4;
    
    // Load AIG
    std::cout << "Loading AIG from " << input_file << "...\n";
    AIG aig;
    aig.read_aiger(input_file);
    
    std::cout << "AIG: " << aig.num_pis << " PIs, " << aig.num_pos << " POs, " 
              << aig.num_nodes << " nodes\n\n";
    
    // Extract windows
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Total windows: " << windows.size() << "\n\n";
    
    // Find and analyze complex windows (3+ inputs)
    std::cout << "=== SEARCHING FOR COMPLEX WINDOWS (3+ inputs) ===\n\n";
    
    int complex_windows_found = 0;
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4) {
            analyze_complex_window(aig, windows[i], i);
            complex_windows_found++;
            
            if (complex_windows_found >= 3) {
                std::cout << "... (analyzed first 3 complex windows)\n";
                break;
            }
        }
    }
    
    if (complex_windows_found == 0) {
        std::cout << "No complex windows found with 3+ inputs. Circuit may be very simple.\n";
        
        // Show a few windows anyway for reference
        std::cout << "\nShowing simpler windows for reference:\n";
        for (size_t i = 0; i < std::min((size_t)2, windows.size()); i++) {
            analyze_complex_window(aig, windows[i], i);
        }
    }
    
    return 0;
}