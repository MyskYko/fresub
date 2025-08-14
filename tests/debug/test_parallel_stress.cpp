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
        WindowExtractor extractor(aig, 6, 8); // Use normal sizes
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        // Limit to max_windows for testing
        if (windows.size() > static_cast<size_t>(max_windows)) {
            windows.resize(max_windows);
            std::cout << "Limited to " << windows.size() << " windows for testing\n";
        }
        
        std::cout << "Creating ParallelResubManager with " << num_threads << " threads...\n";
        ParallelResubManager manager(aig, num_threads);
        
        std::cout << "Starting parallel resubstitution on " << windows.size() << " windows...\n";
        std::cout << "  (This is where the crash might occur)\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        manager.parallel_resubstitute(windows);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Parallel resubstitution completed successfully in " 
                  << duration.count() << "ms!\n";
        
        auto stats = manager.get_stats();
        std::cout << "Statistics:\n";
        std::cout << "  Total windows: " << stats.total_windows << "\n";
        std::cout << "  Successful resubs: " << stats.successful_resubs << "\n";
        std::cout << "  Conflicts detected: " << stats.conflicts_detected << "\n";
        std::cout << "  Commits applied: " << stats.commits_applied << "\n";
        std::cout << "  Total gain: " << stats.total_gain << "\n";
        
        std::cout << "Test completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}