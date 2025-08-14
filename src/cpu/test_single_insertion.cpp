#include <iostream>
#include <signal.h>
#include <execinfo.h>
#include "fresub_aig.hpp"
#include "window.hpp"
#include "synthesis_bridge.hpp"
#include "aig_insertion.hpp"
#include <aig.hpp>

using namespace fresub;

void crash_handler(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    std::cout << "CRASH! Signal " << sig << std::endl;
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

int main(int argc, char** argv) {
    signal(SIGSEGV, crash_handler);
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.aig>" << std::endl;
        return 1;
    }
    
    std::cout << "=== SINGLE INSERTION DEBUG TEST ===" << std::endl;
    
    try {
        // Load AIG
        std::cout << "Loading AIG from " << argv[1] << "..." << std::endl;
        AIG aig;
        aig.read_aiger(argv[1]);
        
        std::cout << "AIG loaded: " << aig.num_pis << " PIs, " << aig.num_pos << " POs, " 
                  << aig.num_nodes << " nodes" << std::endl;
        std::cout << "nodes.size(): " << aig.nodes.size() << std::endl;
        
        // Print AIG structure before any modifications
        std::cout << "\n=== ORIGINAL AIG STRUCTURE ===" << std::endl;
        for (size_t i = 1; i <= std::min(size_t(15), aig.nodes.size()-1); i++) {
            if (!aig.nodes[i].is_dead) {
                std::cout << "Node " << i << ": ";
                if (i <= aig.num_pis) {
                    std::cout << "PI";
                } else {
                    std::cout << "fanin0=" << aig.nodes[i].fanin0 
                              << ", fanin1=" << aig.nodes[i].fanin1;
                }
                std::cout << std::endl;
            }
        }
        
        // Extract windows
        std::cout << "\n=== EXTRACTING WINDOWS ===" << std::endl;
        WindowExtractor extractor(aig, 4);
        std::vector<Window> windows;
        extractor.extract_all_windows(windows);
        
        std::cout << "Total windows extracted: " << windows.size() << std::endl;
        
        // Find first suitable window
        Window* target_window = nullptr;
        int window_idx = -1;
        for (size_t i = 0; i < windows.size(); i++) {
            if (windows[i].inputs.size() >= 3 && windows[i].inputs.size() <= 4 && 
                windows[i].divisors.size() >= 4) {
                target_window = &windows[i];
                window_idx = i;
                break;
            }
        }
        
        if (!target_window) {
            std::cout << "No suitable window found" << std::endl;
            return 1;
        }
        
        std::cout << "\n=== PROCESSING WINDOW " << window_idx << " ===" << std::endl;
        std::cout << "Target node: " << target_window->target_node << std::endl;
        std::cout << "Window inputs: ";
        for (int inp : target_window->inputs) {
            std::cout << inp << " ";
        }
        std::cout << "\nWindow divisors: ";
        for (int div : target_window->divisors) {
            std::cout << div << " ";
        }
        std::cout << std::endl;
        
        // Validate all nodes exist
        std::cout << "\n=== NODE VALIDATION ===" << std::endl;
        bool all_valid = true;
        
        if (target_window->target_node >= aig.nodes.size() || aig.nodes[target_window->target_node].is_dead) {
            std::cout << "ERROR: Target node " << target_window->target_node << " is invalid!" << std::endl;
            all_valid = false;
        }
        
        for (int inp : target_window->inputs) {
            if (inp >= aig.nodes.size() || aig.nodes[inp].is_dead) {
                std::cout << "ERROR: Input node " << inp << " is invalid!" << std::endl;
                all_valid = false;
            }
        }
        
        for (int div : target_window->divisors) {
            if (div >= aig.nodes.size() || aig.nodes[div].is_dead) {
                std::cout << "ERROR: Divisor node " << div << " is invalid!" << std::endl;
                all_valid = false;
            }
        }
        
        if (!all_valid) {
            std::cout << "ABORTING: Invalid nodes detected" << std::endl;
            return 1;
        }
        
        std::cout << "All nodes valid" << std::endl;
        
        // Create simple synthesis (AND gate)
        std::cout << "\n=== CREATING SIMPLE SYNTHESIS ===" << std::endl;
        std::vector<std::vector<bool>> br(4, std::vector<bool>(2, false));
        std::vector<std::vector<bool>> sim(4, std::vector<bool>(2, false));
        
        // AND gate: out = div0 & div1
        br[0][0] = true; sim[0][0] = false; sim[0][1] = false; // 00 -> 0
        br[1][0] = true; sim[1][0] = false; sim[1][1] = true;  // 01 -> 0  
        br[2][0] = true; sim[2][0] = true;  sim[2][1] = false; // 10 -> 0
        br[3][1] = true; sim[3][0] = true;  sim[3][1] = true;  // 11 -> 1
        
        SynthesisResult synthesis = synthesize_circuit(br, sim, 2);
        
        if (!synthesis.success) {
            std::cout << "Synthesis failed: " << synthesis.description << std::endl;
            return 1;
        }
        
        std::cout << "✓ Synthesis successful: " << synthesis.description << std::endl;
        
        // Try insertion
        std::cout << "\n=== ATTEMPTING INSERTION ===" << std::endl;
        
        // Show detailed current AIG state
        std::cout << "Current AIG state before insertion:" << std::endl;
        std::cout << "  num_pis: " << aig.num_pis << std::endl;
        std::cout << "  num_nodes: " << aig.num_nodes << std::endl;
        std::cout << "  nodes.size(): " << aig.nodes.size() << std::endl;
        std::cout << "  All nodes:" << std::endl;
        for (size_t i = 0; i < aig.nodes.size(); i++) {
            if (!aig.nodes[i].is_dead) {
                std::cout << "    Node " << i << ": ";
                if (i <= aig.num_pis) {
                    std::cout << "PI" << std::endl;
                } else {
                    std::cout << "AND(fanin0=" << aig.nodes[i].fanin0 
                              << ", fanin1=" << aig.nodes[i].fanin1 << ")" << std::endl;
                }
            }
        }
        
        // Show window details
        std::cout << "\nWindow details:" << std::endl;
        std::cout << "  Target node: " << target_window->target_node << std::endl;
        std::cout << "  Window inputs (nodes that are cut boundary): ";
        for (size_t i = 0; i < target_window->inputs.size(); i++) {
            std::cout << target_window->inputs[i] << " ";
        }
        std::cout << std::endl;
        std::cout << "  Window divisors (candidate replacement nodes): ";
        for (size_t i = 0; i < target_window->divisors.size(); i++) {
            std::cout << target_window->divisors[i] << " ";
        }
        std::cout << std::endl;
        
        AIGInsertion inserter(aig);
        
        std::vector<int> divisor_indices = {0, 1}; // Use first two divisors
        std::cout << "\nSelected divisor indices: " << divisor_indices[0] << ", " << divisor_indices[1] << std::endl;
        std::cout << "Actual divisor nodes: " << target_window->divisors[divisor_indices[0]] 
                  << ", " << target_window->divisors[divisor_indices[1]] << std::endl;
        
        // Show synthesis details
        aigman* synthesized_aigman = get_synthesis_aigman(synthesis);
        
        if (synthesized_aigman) {
            std::cout << "\nSynthesized circuit details:" << std::endl;
            std::cout << "  nPis: " << synthesized_aigman->nPis << std::endl;
            std::cout << "  nGates: " << synthesized_aigman->nGates << std::endl;
            std::cout << "  vObjs contents:" << std::endl;
            for (int i = 0; i < (synthesized_aigman->nPis + synthesized_aigman->nGates) * 2; i++) {
                std::cout << "    vObjs[" << i << "] = " << synthesized_aigman->vObjs[i] << std::endl;
            }
        }
        
        if (!synthesized_aigman) {
            std::cout << "ERROR: Could not get synthesized aigman" << std::endl;
            return 1;
        }
        
        std::cout << "About to call insert_synthesized_circuit..." << std::endl;
        
        InsertionResult insertion = inserter.insert_synthesized_circuit(
            *target_window, divisor_indices, synthesized_aigman);
        
        std::cout << "insert_synthesized_circuit returned" << std::endl;
        
        if (!insertion.success) {
            std::cout << "✗ Insertion failed: " << insertion.description << std::endl;
            delete synthesized_aigman;
            return 1;
        }
        
        std::cout << "✓ Insertion successful!" << std::endl;
        std::cout << "New circuit root: " << insertion.new_output_node << std::endl;
        
        // Check AIG state after insertion
        std::cout << "\n=== AIG STATE AFTER INSERTION ===" << std::endl;
        std::cout << "num_nodes: " << aig.num_nodes << std::endl;
        std::cout << "nodes.size(): " << aig.nodes.size() << std::endl;
        std::cout << "insertion.new_output_node: " << insertion.new_output_node << std::endl;
        
        if (insertion.new_output_node < aig.nodes.size()) {
            std::cout << "New output node exists: yes" << std::endl;
            std::cout << "New output node is_dead: " << aig.nodes[insertion.new_output_node].is_dead << std::endl;
        } else {
            std::cout << "ERROR: New output node " << insertion.new_output_node 
                      << " is beyond nodes.size() " << aig.nodes.size() << std::endl;
        }
        
        // Try replacement
        std::cout << "\n=== ATTEMPTING REPLACEMENT ===" << std::endl;
        std::cout << "About to call replace_target_with_circuit..." << std::endl;
        
        bool replacement_success = inserter.replace_target_with_circuit(
            target_window->target_node, insertion.new_output_node);
        
        std::cout << "replace_target_with_circuit returned: " << replacement_success << std::endl;
        
        delete synthesized_aigman;
        
        std::cout << "\n✓ SINGLE INSERTION TEST COMPLETED SUCCESSFULLY!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception caught" << std::endl;
        return 1;
    }
    
    return 0;
}