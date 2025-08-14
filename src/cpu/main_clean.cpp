#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <map>
#include <climits>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "resub.hpp"
#include "conflict.hpp"

using namespace fresub;
using namespace std::chrono;

int main(int argc, char** argv) {
    try {
        // Parse arguments (simplified)
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <input.aig> <output.aig>\n";
            return 1;
        }
        
        std::string input_file = argv[1];
        std::string output_file = argv[2];
        bool verbose = true;
        
        // Load AIG
        std::cout << "Loading AIG from " << input_file << "...\n";
        AIG aig;
        aig.read_aiger(input_file);
        
        // Print initial stats
        int initial_gates = 0;
        for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
            if (!aig.nodes[i].is_dead) initial_gates++;
        }
        
        std::cout << "Initial AIG:\n";
        std::cout << "  PIs: " << aig.num_pis << "\n";
        std::cout << "  POs: " << aig.num_pos << "\n";
        std::cout << "  Nodes: " << aig.num_nodes << "\n";
        std::cout << "  Gates: " << initial_gates << "\n";
        aig.compute_levels();
        std::cout << "  Levels: (computed)\n\n";
        
        // Extract windows
        std::cout << "Extracting windows...\n";
        WindowExtractor extractor(aig, 6);
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        
        // Analyze windows
        std::cout << "\n=== WINDOW ANALYSIS ===\n";
        std::cout << "Total windows: " << windows.size() << "\n";
        
        // Window distribution by depth
        std::map<int, int> depth_distribution;
        std::map<int, int> size_distribution;
        
        for (const auto& w : windows) {
            int level = aig.get_level(w.target_node);
            depth_distribution[level/10 * 10]++;
            size_distribution[w.nodes.size()/10 * 10]++;
        }
        
        std::cout << "\nWindow distribution by target depth:\n";
        int shown = 0;
        for (const auto& [depth, count] : depth_distribution) {
            std::cout << "  Levels " << depth << "-" << (depth+9) << ": " << count << " windows\n";
            if (++shown >= 5) {
                std::cout << "  ...\n";
                break;
            }
        }
        
        std::cout << "\nWindow size distribution:\n";
        shown = 0;
        for (const auto& [size, count] : size_distribution) {
            std::cout << "  " << size << "-" << (size+9) << " nodes: " << count << " windows\n";
            if (++shown >= 5) {
                std::cout << "  ...\n";
                break;
            }
        }
        
        // Find interesting sample windows
        std::cout << "\n=== SAMPLE WINDOW ANALYSIS ===\n";
        
        // Find a shallow, medium, and deep window
        int shallow_idx = -1, medium_idx = -1, deep_idx = -1;
        
        for (size_t i = 0; i < windows.size(); i++) {
            int level = aig.get_level(windows[i].target_node);
            
            if (shallow_idx < 0 && level < 10 && windows[i].nodes.size() > 5) {
                shallow_idx = i;
            }
            if (medium_idx < 0 && level >= 20 && level < 30 && windows[i].nodes.size() > 20) {
                medium_idx = i;
            }
            if (deep_idx < 0 && level >= 40 && windows[i].nodes.size() > 30) {
                deep_idx = i;
            }
            
            if (shallow_idx >= 0 && medium_idx >= 0 && deep_idx >= 0) break;
        }
        
        std::vector<int> samples;
        if (shallow_idx >= 0) samples.push_back(shallow_idx);
        if (medium_idx >= 0) samples.push_back(medium_idx);
        if (deep_idx >= 0) samples.push_back(deep_idx);
        
        for (int idx : samples) {
            const auto& w = windows[idx];
            
            std::cout << "\nWindow " << idx << " (Target: " << w.target_node << "):\n";
            
            // Context
            int level = aig.get_level(w.target_node);
            std::cout << "  Position: Level " << level << "\n";
            std::cout << "  Fanouts: " << aig.nodes[w.target_node].fanouts.size() 
                     << " nodes use this result\n";
            
            // Structure
            std::cout << "  Window structure:\n";
            std::cout << "    Cut: " << w.inputs.size() << " inputs\n";
            std::cout << "    Contains: " << w.nodes.size() << " total nodes\n";
            std::cout << "    Divisors: " << w.divisors.size() << " available\n";
            
            // MFFC analysis
            int mffc_size = w.nodes.size() - w.divisors.size();
            std::cout << "    MFFC: " << mffc_size << " nodes (removable if optimized)\n";
            
            // Optimization potential
            std::cout << "  Optimization strategy:\n";
            std::cout << "    Current: " << w.inputs.size() << "-input function\n";
            std::cout << "    Will try: Expressing using " << w.divisors.size() << " divisors\n";
            if (mffc_size > 0) {
                std::cout << "    Potential: Could save " << mffc_size << " gates\n";
            }
        }
        
        std::cout << "\n=== ALGORITHM OVERVIEW ===\n";
        std::cout << "The resubstitution algorithm will:\n";
        std::cout << "1. Process " << windows.size() << " windows in parallel\n";
        std::cout << "2. For each window, compute truth tables using bit-parallel simulation\n";
        std::cout << "3. Try to express target using combinations of divisors\n";
        std::cout << "4. Accept replacements that reduce gate count\n";
        std::cout << "5. Resolve conflicts between overlapping optimizations\n";
        std::cout << "\nThis provides a powerful local optimization framework.\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}