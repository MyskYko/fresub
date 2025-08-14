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
        WindowExtractor extractor(aig, 4, 6); // Smaller sizes for testing
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        // Limit to first 20 windows for debugging
        if (windows.size() > 20) {
            windows.resize(20);
            std::cout << "Limited to " << windows.size() << " windows for testing\n";
        }
        
        std::cout << "Creating ParallelResubManager with " << num_threads << " threads...\n";
        ParallelResubManager manager(aig, num_threads);
        
        std::cout << "Starting parallel resubstitution...\n";
        std::cout << "  (This is where the crash might occur)\n";
        
        manager.parallel_resubstitute(windows);
        
        std::cout << "Parallel resubstitution completed successfully!\n";
        
        auto stats = manager.get_stats();
        std::cout << "Statistics:\n";
        std::cout << "  Total windows: " << stats.total_windows << "\n";
        std::cout << "  Successful resubs: " << stats.successful_resubs << "\n";
        std::cout << "  Conflicts detected: " << stats.conflicts_detected << "\n";
        std::cout << "  Commits applied: " << stats.commits_applied << "\n";
        std::cout << "  Total gain: " << stats.total_gain << "\n";
        
        std::cout << "Done!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}