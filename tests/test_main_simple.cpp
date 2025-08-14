#include <iostream>
#include "aig.hpp"

using namespace fresub;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <input.aig> <output.aig>\n";
        return 1;
    }
    
    try {
        std::cout << "Loading AIG from " << argv[1] << "...\n";
        AIG aig(argv[1]);
        
        std::cout << "AIG loaded successfully:\n";
        std::cout << "  PIs: " << aig.num_pis << "\n";
        std::cout << "  POs: " << aig.num_pos << "\n";
        std::cout << "  Nodes: " << aig.num_nodes << "\n";
        
        std::cout << "Writing AIG to " << argv[2] << "...\n";
        aig.write_aiger(argv[2], true);
        
        std::cout << "Done!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}