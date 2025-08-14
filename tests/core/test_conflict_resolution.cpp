#include "fresub_aig.hpp"
#include "window.hpp"
#include "conflict.hpp"
#include "feasibility.hpp"
#include <iostream>

using namespace fresub;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.aig>\n";
        return 1;
    }
    
    std::cout << "=== CONFLICT RESOLUTION DEMO ===\n\n";
    
    try {
        // Load AIG
        AIG aig(argv[1]);
        std::cout << "Loaded AIG: " << aig.num_pis << " PIs, " 
                  << aig.num_pos << " POs, " << aig.num_nodes << " nodes\n";
        
        // Build fanouts and levels for window extraction
        aig.build_fanouts();
        aig.compute_levels();
        
        // Extract windows
        WindowExtractor extractor(aig, 4);
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        
        std::cout << "Extracted " << windows.size() << " windows\n";
        if (windows.empty()) {
            std::cout << "No windows to process\n";
            return 0;
        }
        
        // Create conflict resolver
        ConflictResolver resolver(aig);
        
        // Show first few windows for demonstration
        int show_count = std::min(5, (int)windows.size());
        std::cout << "\nFirst " << show_count << " windows:\n";
        for (int i = 0; i < show_count; i++) {
            const Window& w = windows[i];
            std::cout << "  Window " << i << ": target=" << w.target_node 
                      << ", inputs={";
            for (size_t j = 0; j < w.inputs.size(); j++) {
                if (j > 0) std::cout << ",";
                std::cout << w.inputs[j];
            }
            std::cout << "}, divisors=" << w.divisors.size() << "\n";
        }
        
        // Define a mock resubstitution function for demo
        auto mock_resubstitution = [&](const Window& window) -> bool {
            std::cout << "    Mock resubstitution for target " << window.target_node;
            
            // Check feasibility first
            FeasibilityResult result = find_feasible_4resub(aig, window);
            if (!result.found) {
                std::cout << " -> Not feasible\n";
                return false;
            }
            
            std::cout << " -> Feasible with " << result.divisor_indices.size() 
                      << " divisors\n";
            return true; // Mock success
        };
        
        std::cout << "\n=== SEQUENTIAL PROCESSING ===\n";
        
        // Process windows sequentially with conflict resolution
        std::vector<bool> results = resolver.process_windows_sequentially(
            windows, mock_resubstitution);
        
        // Count results
        int successful = 0, failed = 0, skipped = 0;
        for (size_t i = 0; i < results.size(); i++) {
            if (results[i]) {
                successful++;
            } else {
                // Check if window was valid at time of processing
                bool was_valid = resolver.is_window_valid(windows[i]);
                if (was_valid) {
                    failed++;
                } else {
                    skipped++;
                }
            }
        }
        
        std::cout << "\n=== RESULTS ===\n";
        std::cout << "Total windows: " << windows.size() << "\n";
        std::cout << "Successful resubstitutions: " << successful << "\n";
        std::cout << "Failed resubstitutions: " << failed << "\n";
        std::cout << "Skipped due to conflicts: " << skipped << "\n";
        
        std::cout << "\n✅ Conflict resolution demo completed successfully!\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}