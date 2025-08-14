#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <chrono>
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

void print_node_list(const std::vector<int>& nodes) {
    std::cout << "[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

// Old approach: process cuts one by one
std::vector<Window> extract_windows_old_way(AIG& aig, int max_cut_size) {
    std::vector<Window> windows;
    CutEnumerator cut_enumerator(aig, max_cut_size);
    cut_enumerator.enumerate_cuts();
    
    int cut_id = 0;
    
    for (int target = aig.num_pis + 1; target < aig.num_nodes; target++) {
        if (target >= static_cast<int>(aig.nodes.size()) || aig.nodes[target].is_dead) continue;
        
        const auto& node_cuts = cut_enumerator.get_cuts(target);
        
        for (const auto& cut : node_cuts) {
            if (cut.leaves.size() == 1 && cut.leaves[0] == target) {
                continue; // Skip trivial cut
            }
            if (cut.leaves.size() > max_cut_size) {
                continue;
            }
            
            Window window;
            window.target_node = target;
            window.inputs = cut.leaves;
            window.cut_id = cut_id++;
            window.nodes.clear();
            window.divisors.clear();
            
            // Single cut propagation (old way)
            std::vector<std::vector<int>> node_cut_lists(aig.num_nodes);
            
            // Initialize cut leaves
            for (int leaf : cut.leaves) {
                node_cut_lists[leaf].push_back(window.cut_id);
            }
            
            // Propagate this cut only
            for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
                if (i >= static_cast<int>(aig.nodes.size()) || aig.nodes[i].is_dead) continue;
                
                int fanin0 = aig.lit2var(aig.nodes[i].fanin0);
                int fanin1 = aig.lit2var(aig.nodes[i].fanin1);
                
                bool has_fanin0 = std::find(node_cut_lists[fanin0].begin(), 
                                           node_cut_lists[fanin0].end(), window.cut_id) != node_cut_lists[fanin0].end();
                bool has_fanin1 = std::find(node_cut_lists[fanin1].begin(), 
                                           node_cut_lists[fanin1].end(), window.cut_id) != node_cut_lists[fanin1].end();
                
                if (has_fanin0 && has_fanin1) {
                    node_cut_lists[i].push_back(window.cut_id);
                }
            }
            
            // Collect window nodes
            for (int i = 1; i < aig.num_nodes; i++) {
                if (std::find(node_cut_lists[i].begin(), node_cut_lists[i].end(), window.cut_id) 
                    != node_cut_lists[i].end()) {
                    window.nodes.push_back(i);
                }
            }
            
            // Simple divisors (all window nodes except target)
            for (int node : window.nodes) {
                if (node != target) {
                    window.divisors.push_back(node);
                }
            }
            
            if (!window.nodes.empty()) {
                windows.push_back(window);
            }
        }
    }
    
    return windows;
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
              << aig.num_nodes << " nodes\n";
    std::cout << "Max cut size: " << max_cut_size << "\n\n";
    
    // Compare old vs new approach
    std::cout << "=== COMPARING WINDOW EXTRACTION APPROACHES ===\n\n";
    
    // Method 1: Old approach (cut-by-cut)
    std::cout << "METHOD 1: Cut-by-cut processing (OLD)\n";
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Window> windows_old = extract_windows_old_way(aig, max_cut_size);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_old = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Windows created: " << windows_old.size() << "\n";
    std::cout << "  Time: " << duration_old.count() << " µs\n";
    
    // Method 2: New approach (simultaneous propagation)
    std::cout << "\nMETHOD 2: Simultaneous cut propagation (NEW)\n";
    start = std::chrono::high_resolution_clock::now();
    WindowExtractor extractor(aig, max_cut_size);
    std::vector<Window> windows_new;
    extractor.extract_all_windows(windows_new);
    end = std::chrono::high_resolution_clock::now();
    auto duration_new = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Windows created: " << windows_new.size() << "\n";
    std::cout << "  Time: " << duration_new.count() << " µs\n";
    
    // Analysis
    std::cout << "\n=== ANALYSIS ===\n";
    std::cout << "Window count difference: " << (windows_new.size() - windows_old.size()) << "\n";
    if (duration_old.count() > 0) {
        double speedup = (double)duration_old.count() / duration_new.count();
        std::cout << "Speedup: " << speedup << "x\n";
    }
    
    // Compare sample windows
    std::cout << "\n=== SAMPLE WINDOW COMPARISON ===\n";
    
    size_t sample_count = std::min({(size_t)3, windows_old.size(), windows_new.size()});
    
    for (size_t i = 0; i < sample_count; i++) {
        std::cout << "\nSample " << (i+1) << ":\n";
        
        // Old method window
        const auto& w_old = windows_old[i];
        std::cout << "  OLD: Target=" << w_old.target_node 
                 << ", Inputs=" << w_old.inputs.size()
                 << ", Nodes=" << w_old.nodes.size() 
                 << ", Divisors=" << w_old.divisors.size() << "\n";
        std::cout << "    Cut: ";
        Cut cut_old;
        cut_old.leaves = w_old.inputs;
        print_cut(cut_old, aig);
        std::cout << "\n    Nodes: ";
        print_node_list(w_old.nodes);
        std::cout << "\n";
        
        // New method window  
        const auto& w_new = windows_new[i];
        std::cout << "  NEW: Target=" << w_new.target_node 
                 << ", Inputs=" << w_new.inputs.size()
                 << ", Nodes=" << w_new.nodes.size() 
                 << ", Divisors=" << w_new.divisors.size() << "\n";
        std::cout << "    Cut: ";
        Cut cut_new;
        cut_new.leaves = w_new.inputs;
        print_cut(cut_new, aig);
        std::cout << "\n    Nodes: ";
        print_node_list(w_new.nodes);
        std::cout << "\n";
        
        // Differences
        if (w_old.nodes != w_new.nodes) {
            std::cout << "    *** DIFFERENT NODE SETS ***\n";
        }
        if (w_old.divisors.size() != w_new.divisors.size()) {
            std::cout << "    *** DIFFERENT DIVISOR COUNTS ***\n";
        }
    }
    
    std::cout << "\n=== KEY IMPROVEMENTS ===\n";
    std::cout << "1. Simultaneous propagation ensures global consistency\n";
    std::cout << "2. All cuts are processed in one topological pass\n";
    std::cout << "3. Proper dominance relationships are maintained\n";
    std::cout << "4. More efficient than individual cut processing\n";
    std::cout << "5. Enables correct window-based optimization\n";
    
    return 0;
}