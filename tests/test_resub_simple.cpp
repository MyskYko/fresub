#include <iostream>
#include "aig.hpp"
#include "window.hpp"
#include "resub.hpp"

using namespace fresub;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <input.aig>\n";
        return 1;
    }
    
    try {
        std::cout << "Loading AIG from " << argv[1] << "...\n";
        AIG aig(argv[1]);
        
        std::cout << "AIG loaded successfully:\n";
        std::cout << "  PIs: " << aig.num_pis << "\n";
        std::cout << "  POs: " << aig.num_pos << "\n";
        std::cout << "  Nodes: " << aig.num_nodes << "\n";
        
        std::cout << "Creating WindowExtractor...\n";
        WindowExtractor extractor(aig, 6, 8);
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        std::cout << "Creating ResubEngine (single-threaded)...\n";
        ResubEngine engine(aig, false); // false = no GPU
        
        std::cout << "Testing single window resubstitution...\n";
        if (!windows.empty()) {
            ResubResult result = engine.resubstitute(windows[0]);
            std::cout << "First window result: success=" << result.success 
                      << " gain=" << result.actual_gain << "\n";
        }
        
        std::cout << "Testing batch resubstitution (first 5 windows)...\n";
        if (windows.size() > 5) {
            std::vector<Window> small_batch(windows.begin(), windows.begin() + 5);
            std::vector<ResubResult> results = engine.resubstitute_batch(small_batch);
            std::cout << "Batch results: " << results.size() << " results\n";
            for (size_t i = 0; i < results.size(); i++) {
                std::cout << "  Window " << i << ": success=" << results[i].success 
                          << " gain=" << results[i].actual_gain << "\n";
            }
        }
        
        std::cout << "Done!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}