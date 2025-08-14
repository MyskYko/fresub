#include <iostream>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "feasibility.hpp"

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
        WindowExtractor extractor(aig, 6);
        
        std::cout << "Extracting windows...\n";
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        std::cout << "Extracted " << windows.size() << " windows\n";
        
        std::cout << "Testing window feasibility...\n";
        int feasible_count = 0;
        for (size_t i = 0; i < windows.size() && i < 5; i++) {
            const Window& window = windows[i];
            if (window.inputs.size() >= 3 && window.inputs.size() <= 4 && 
                window.divisors.size() >= 4) {
                
                FeasibilityResult result = find_feasible_4resub(aig, window);
                if (result.found) {
                    feasible_count++;
                    std::cout << "  Window " << i << ": feasible with " 
                              << result.divisor_nodes.size() << " divisors\n";
                } else {
                    std::cout << "  Window " << i << ": not feasible\n";
                }
            } else {
                std::cout << "  Window " << i << ": skipped (wrong size)\n";
            }
        }
        
        std::cout << "Found " << feasible_count << " feasible windows out of " 
                  << std::min(windows.size(), size_t(5)) << " tested\n";
        
        std::cout << "Done!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}