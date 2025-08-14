#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <map>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "resub.hpp"
#include "conflict.hpp"

using namespace fresub;
using namespace std::chrono;

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options] <input.aig> <output.aig>\n";
    std::cout << "Options:\n";
    std::cout << "  -w <size>    Window size (default: 6)\n";
    std::cout << "  -c <size>    Cut size (default: 8)\n";
    std::cout << "  -r <rounds>  Number of rounds (default: 1)\n";
    std::cout << "  -t <threads> Number of threads (default: 8)\n";
    std::cout << "  -g           Use GPU acceleration\n";
    std::cout << "  -v           Verbose output\n";
    std::cout << "  -s           Print statistics\n";
    std::cout << "  -h           Show this help\n";
}

struct Config {
    std::string input_file;
    std::string output_file;
    int window_size = 6;
    int cut_size = 8;
    int num_rounds = 1;
    int num_threads = 8;
    bool use_gpu = false;
    bool verbose = false;
    bool show_stats = false;
};

Config parse_args(int argc, char* argv[]) {
    Config config;
    
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.window_size = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config.cut_size = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            config.num_rounds = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.num_threads = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-g") == 0) {
            config.use_gpu = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            config.show_stats = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (argv[i][0] != '-') {
            if (config.input_file.empty()) {
                config.input_file = argv[i];
            } else if (config.output_file.empty()) {
                config.output_file = argv[i];
            }
        }
        i++;
    }
    
    if (config.input_file.empty() || config.output_file.empty()) {
        print_usage(argv[0]);
        exit(1);
    }
    
    return config;
}

void print_aig_stats(const AIG& aig, const std::string& label) {
    std::cout << label << ":\n";
    std::cout << "  PIs: " << aig.num_pis << "\n";
    std::cout << "  POs: " << aig.num_pos << "\n";
    std::cout << "  Nodes: " << aig.num_nodes << "\n";
    
    // Count actual gates (non-dead nodes)
    int num_gates = 0;
    int max_level = 0;
    for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
        if (!aig.nodes[i].is_dead) {
            num_gates++;
            max_level = std::max(max_level, aig.nodes[i].level);
        }
    }
    std::cout << "  Gates: " << num_gates << "\n";
    std::cout << "  Levels: " << max_level << "\n";
}

int main(int argc, char* argv[]) {
    Config config = parse_args(argc, argv);
    
    try {
        // Load input AIG
        if (config.verbose) {
            std::cout << "Loading AIG from " << config.input_file << "...\n";
        }
        
        AIG aig(config.input_file);
        
        if (config.show_stats) {
            print_aig_stats(aig, "Initial AIG");
        }
        
        int initial_gates = 0;
        for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
            if (!aig.nodes[i].is_dead) initial_gates++;
        }
        
        // Perform resubstitution rounds
        for (int round = 0; round < config.num_rounds; round++) {
            if (config.verbose) {
                std::cout << "\nRound " << (round + 1) << " of " << config.num_rounds << "\n";
            }
            
            auto start_time = high_resolution_clock::now();
            
            // Extract windows
            if (config.verbose) {
                std::cout << "Extracting windows (size=" << config.window_size 
                         << ", cut=" << config.cut_size << ")...\n";
            }
            WindowExtractor extractor(aig, config.window_size);
            std::vector<Window> windows;
            extractor.extract_all_windows(windows);
            
            if (config.verbose) {
                std::cout << "Extracted " << windows.size() << " windows\n";
                if (windows.size() > 0) {
                    // Show detailed stats about window distribution
                    std::map<int, int> input_size_dist, divisor_size_dist;
                    for (const auto& w : windows) {
                        input_size_dist[w.inputs.size()]++;
                        divisor_size_dist[w.divisors.size()]++;
                    }
                    
                    std::cout << "Window input size distribution:\n";
                    for (const auto& [size, count] : input_size_dist) {
                        std::cout << "  " << count << " windows with " << size << " inputs\n";
                    }
                    
                    std::cout << "Window divisor count distribution:\n";
                    for (const auto& [size, count] : divisor_size_dist) {
                        std::cout << "  " << count << " windows with " << size << " divisors\n";
                    }
                    
                    // Show detailed info for first few windows
                    int sample_size = std::min(3, (int)windows.size());
                    std::cout << "\nDetailed window analysis (first " << sample_size << "):\n";
                    for (int i = 0; i < sample_size; i++) {
                        const auto& w = windows[i];
                        std::cout << "  Window " << i << " (target=" << w.target_node << "):\n";
                        std::cout << "    Inputs: [";
                        for (size_t j = 0; j < w.inputs.size(); j++) {
                            if (j > 0) std::cout << ", ";
                            std::cout << w.inputs[j];
                        }
                        std::cout << "]\n";
                        std::cout << "    Window nodes: " << w.nodes.size() << "\n";
                        std::cout << "    Divisors: " << w.divisors.size() << "\n";
                        std::cout << "    Cut ID: " << w.cut_id << "\n";
                    }
                }
            }
            
            // EARLY TERMINATION: Let's examine windows before resubstitution
            if (config.verbose) {
                std::cout << "\n=== WINDOW ANALYSIS FOR UNDERSTANDING ===\n";
                
                // Analyze window distribution by depth
                std::map<int, int> depth_distribution;
                std::map<int, int> size_by_depth;
                int deep_windows = 0;
                int shallow_windows = 0;
                
                for (const auto& w : windows) {
                    int level = aig.get_level(w.target_node);
                    depth_distribution[level/5 * 5]++;  // Group by 5 levels
                    
                    if (level > 20) deep_windows++;
                    else shallow_windows++;
                    
                    size_by_depth[level/10 * 10] += w.nodes.size();
                }
                
                std::cout << "Window distribution by circuit depth:\n";
                for (const auto& [depth, count] : depth_distribution) {
                    std::cout << "  Levels " << depth << "-" << (depth+4) << ": " << count << " windows\n";
                    if (depth > 25) break; // Show first few groups
                }
                std::cout << "  Shallow (level ≤20): " << shallow_windows << " windows\n";
                std::cout << "  Deep (level >20): " << deep_windows << " windows\n\n";
                
                // Find interesting windows to examine
                std::cout << "=== EXAMINING SAMPLE WINDOWS ===\n\n";
                
                // Sample 1: A shallow window
                int sample1 = -1;
                for (int i = 0; i < std::min(100, (int)windows.size()); i++) {
                    if (windows[i].inputs.size() == 3 && windows[i].nodes.size() < 10) {
                        sample1 = i;
                        break;
                    }
                }
                
                // Sample 2: A medium depth window  
                int sample2 = -1;
                for (int i = 0; i < (int)windows.size(); i++) {
                    if (windows[i].target_node > 1000 && windows[i].inputs.size() == 4 && 
                        windows[i].nodes.size() > 20) {
                        sample2 = i;
                        break;
                    }
                }
                
                // Sample 3: A deep window
                int sample3 = -1;
                for (int i = 0; i < (int)windows.size(); i++) {
                    if (windows[i].target_node > 2000 && windows[i].inputs.size() >= 5) {
                        sample3 = i;
                        break;
                    }
                }
                
                std::vector<int> samples;
                if (sample1 >= 0) samples.push_back(sample1);
                if (sample2 >= 0) samples.push_back(sample2);
                if (sample3 >= 0) samples.push_back(sample3);
                
                for (int idx : samples) {
                    const auto& w = windows[idx];
                    std::cout << "WINDOW " << idx << " (Target node " << w.target_node << "):\n";
                    std::cout << "  Circuit context:\n";
                    std::cout << "    Target at level: " << aig.get_level(w.target_node) << " (max circuit depth: " << aig.compute_levels() << ")\n";
                    std::cout << "    Target fanouts: " << aig.nodes[w.target_node].fanouts.size() << " nodes depend on this\n";
                    
                    std::cout << "  Window structure:\n";
                    std::cout << "    Cut size: " << w.inputs.size() << " inputs [";
                    for (size_t j = 0; j < w.inputs.size(); j++) {
                        if (j > 0) std::cout << ", ";
                        std::cout << w.inputs[j];
                    }
                    std::cout << "]\n";
                    std::cout << "    Window contains: " << w.nodes.size() << " total nodes\n";
                    std::cout << "    Available divisors: " << w.divisors.size() << " nodes\n";
                    
                    // Analyze MFFC
                    int mffc_size = w.nodes.size() - w.divisors.size();
                    std::cout << "    MFFC size: " << mffc_size << " nodes (private to target)\n";
                    
                    std::cout << "\n  What resubstitution will try:\n";
                    std::cout << "    Goal: Express target node " << w.target_node << " using divisors\n";
                    std::cout << "    Current: " << w.inputs.size() << "-input function\n";
                    std::cout << "    Trying: Combinations of " << w.divisors.size() << " divisors\n";
                    
                    // Show potential savings
                    if (mffc_size > 1) {
                        std::cout << "    Potential: Save up to " << mffc_size << " gates if successful\n";
                    }
                    std::cout << "\n";
                }
                
                std::cout << "=== RESUBSTITUTION ALGORITHM OVERVIEW ===\n";
                std::cout << "For each of the " << windows.size() << " windows:\n";
                std::cout << "1. Compute truth table of target using bit-parallel simulation\n";
                std::cout << "2. Try single divisors first (direct wire replacement)\n";
                std::cout << "3. Try pairs of divisors with AND/OR/XOR operations\n";
                std::cout << "4. Accept if new implementation uses fewer gates\n";
                std::cout << "5. Handle conflicts between overlapping optimizations\n";
                
                std::cout << "\nEARLY TERMINATION: Stopping here to understand the algorithm.\n";
                return 0;
            }
            
                    for (size_t j = 0; j < w.inputs.size(); j++) {
                        if (j > 0) std::cout << ", ";
                        std::cout << w.inputs[j];
                        if (w.inputs[j] <= aig.num_pis) {
                            std::cout << "(PI)";
                        }
                    }
                    std::cout << "]\n";
                    
                    std::cout << "  Window nodes (all): [";
                    for (size_t j = 0; j < std::min((size_t)10, w.nodes.size()); j++) {
                        if (j > 0) std::cout << ", ";
                        std::cout << w.nodes[j];
                    }
                    if (w.nodes.size() > 10) {
                        std::cout << " ... +" << (w.nodes.size() - 10) << " more";
                    }
                    std::cout << "]\n";
                    
                    std::cout << "  Total divisors: " << w.divisors.size() << "\n";
                    std::cout << "  Divisor details:\n";
                    
                    // Show divisor breakdown - no complements
                    for (int j = 0; j < std::min((int)w.divisors.size(), 6); j++) {
                        int node = w.divisors[j];
                        std::cout << "    Divisor " << j << ": node " << node;
                        
                        if (node <= aig.num_pis) {
                            std::cout << " (Primary Input)";
                        } else if (std::find(w.inputs.begin(), w.inputs.end(), node) != w.inputs.end()) {
                            std::cout << " (Window Input)";
                        } else if (std::find(w.nodes.begin(), w.nodes.end(), node) != w.nodes.end()) {
                            std::cout << " (Internal Node)";
                        } else {
                            std::cout << " (External Node)";
                        }
                        
                        // Show the actual gate definition if it's an internal node
                        if (node > aig.num_pis && node < aig.num_nodes && !aig.nodes[node].is_dead) {
                            int fanin0 = aig.nodes[node].fanin0;
                            int fanin1 = aig.nodes[node].fanin1;
                            std::cout << " = AND(" << aig.lit2var(fanin0);
                            if (aig.is_complemented(fanin0)) std::cout << "'";
                            std::cout << ", " << aig.lit2var(fanin1);
                            if (aig.is_complemented(fanin1)) std::cout << "'";
                            std::cout << ")";
                        }
                        std::cout << "\n";
                    }
                    
                    if (w.divisors.size() > 6) {
                        std::cout << "    ... and " << (w.divisors.size() - 6) << " more divisors\n";
                    }
                    std::cout << "\n";
                }
                
                std::cout << "\nEARLY TERMINATION: Stopping here to examine window structure.\n";
                std::cout << "Do the windows and divisors look correct to you?\n";
                return 0;  // Exit early
            }
            
            // This code won't be reached due to early termination above
            ParallelResubManager manager(aig, config.num_threads);
            manager.parallel_resubstitute(windows);
            
            auto end_time = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(end_time - start_time);
            
            if (config.show_stats) {
                auto stats = manager.get_stats();
                std::cout << "Round " << (round + 1) << " statistics:\n";
                std::cout << "  Time: " << duration.count() << " ms\n";
                std::cout << "  Windows processed: " << stats.total_windows << "\n";
                std::cout << "  Successful resubs: " << stats.successful_resubs << "\n";
                std::cout << "  Conflicts detected: " << stats.conflicts_detected << "\n";
                std::cout << "  Commits applied: " << stats.commits_applied << "\n";
                std::cout << "  Total gain: " << stats.total_gain << "\n";
            }
            
            // Check if improvement was made
            int current_gates = 0;
            for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
                if (!aig.nodes[i].is_dead) current_gates++;
            }
            
            if (current_gates >= initial_gates) {
                if (config.verbose) {
                    std::cout << "No improvement in this round, stopping\n";
                }
                break;
            }
            
            initial_gates = current_gates;
        }
        
        // Final statistics
        if (config.show_stats) {
            print_aig_stats(aig, "\nFinal AIG");
        }
        
        // Write output
        if (config.verbose) {
            std::cout << "\nWriting optimized AIG to " << config.output_file << "...\n";
        }
        
        aig.write_aiger(config.output_file, true);
        
        // Print final gate count
        int final_gates = 0;
        for (int i = aig.num_pis + 1; i < aig.num_nodes; i++) {
            if (!aig.nodes[i].is_dead) final_gates++;
        }
        std::cout << "Final gate count: " << final_gates << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
