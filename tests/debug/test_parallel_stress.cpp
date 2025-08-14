#include <iostream>
#include <chrono>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "feasibility.hpp"
#include "conflict.hpp"

using namespace fresub;

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cout << "Usage: " << argv[0] << " <input.aig> [num_threads] [max_windows]\n";
        return 1;
    }
    
    int num_threads = (argc >= 3) ? std::stoi(argv[2]) : 2;
    int max_windows = (argc == 4) ? std::stoi(argv[3]) : 100;
    
    try {
        std::cout << "Loading AIG from " << argv[1] << "...\n";
        AIG aig(argv[1]);
        
        std::cout << "AIG loaded successfully:\n";
        std::cout << "  PIs: " << aig.num_pis << "\n";
        std::cout << "  POs: " << aig.num_pos << "\n";
        std::cout << "  Nodes: " << aig.num_nodes << "\n";
        
        std::cout << "Creating WindowExtractor...\n";
        WindowExtractor extractor(aig, 6); // Use current API: max_cut_size=6
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        // Limit to max_windows for testing
        if (windows.size() > static_cast<size_t>(max_windows)) {
            windows.resize(max_windows);
            std::cout << "Limited to " << windows.size() << " windows for testing\n";
        }
        
        std::cout << "Creating ConflictResolver (parallel processing not yet implemented)...\n";
        std::cout << "Using sequential processing instead of " << num_threads << " threads\n";
        ConflictResolver resolver(aig);
        
        // Mock resubstitution function for stress testing
        auto mock_resubstitution = [&](const Window& window) -> bool {
            // Simulate some work and return success for subset of windows
            return (window.inputs.size() <= 4 && window.target_node % 2 == 0);
        };
        
        std::cout << "Starting sequential processing on " << windows.size() << " windows...\n";
        std::cout << "  (Stress testing with mock resubstitution)\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<bool> results = resolver.process_windows_sequentially(windows, mock_resubstitution);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Sequential processing completed successfully in " 
                  << duration.count() << "ms!\n";
        
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
        std::cout << "  Processing time: " << duration.count() << "ms\n";
        std::cout << "  Note: Parallel processing not yet implemented, using sequential\n";
        
        std::cout << "Test completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}