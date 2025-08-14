#include <iostream>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "feasibility.hpp"
#include "conflict.hpp"

using namespace fresub;

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cout << "Usage: " << argv[0] << " <input.aig> [num_threads]\n";
        return 1;
    }
    
    int num_threads = (argc == 3) ? std::stoi(argv[2]) : 2;
    
    try {
        std::cout << "Loading AIG from " << argv[1] << "...\n";
        AIG aig(argv[1]);
        
        std::cout << "AIG loaded successfully:\n";
        std::cout << "  PIs: " << aig.num_pis << "\n";
        std::cout << "  POs: " << aig.num_pos << "\n";
        std::cout << "  Nodes: " << aig.num_nodes << "\n";
        
        std::cout << "Creating WindowExtractor...\n";
        WindowExtractor extractor(aig, 4); // Use current API: max_cut_size=4
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        // Limit to first 20 windows for debugging
        if (windows.size() > 20) {
            windows.resize(20);
            std::cout << "Limited to " << windows.size() << " windows for testing\n";
        }
        
        std::cout << "Creating ConflictResolver (parallel processing not yet implemented)...\n";
        std::cout << "Using sequential processing with " << num_threads << " threads requested\n";
        ConflictResolver resolver(aig);
        
        // Mock resubstitution function for testing
        auto mock_resubstitution = [&](const Window& window) -> bool {
            // Simple mock - just return true for some windows to simulate work
            return (window.target_node % 3 == 0); // Mock: accept every 3rd window
        };
        
        std::cout << "Starting sequential processing (parallel not implemented)...\n";
        std::vector<bool> results = resolver.process_windows_sequentially(windows, mock_resubstitution);
        
        std::cout << "Processing completed successfully!\n";
        
        // Calculate stats
        int successful = 0, failed = 0;
        for (bool result : results) {
            if (result) successful++;
            else failed++;
        }
        std::cout << "Statistics:\n";
        std::cout << "  Total windows: " << results.size() << "\n";
        std::cout << "  Successful resubs: " << successful << "\n";
        std::cout << "  Failed resubs: " << failed << "\n";
        std::cout << "  Note: Parallel processing not yet implemented, using sequential\n";
        
        std::cout << "Done!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}