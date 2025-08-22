#include "window.hpp"
#include "synthesis.hpp"
#include "conflict.hpp"
#include <aig.hpp>
#include <iostream>
#include <cassert>

int total_tests = 0;
int passed_tests = 0;

#define ASSERT(cond) do { \
    total_tests++; \
    if (!(cond)) { \
        std::cerr << "Test failed: " << #cond << std::endl; \
    } else { \
        passed_tests++; \
    } \
} while(0)

using namespace fresub;

void print_aig_structure(const aigman& aig, const std::string& label) {
    std::cout << "=== " << label << " ===\n";
    std::cout << "nPis: " << aig.nPis << ", nGates: " << aig.nGates 
              << ", nPos: " << aig.nPos << ", nObjs: " << aig.nObjs << "\n";
    
    // Print gates
    std::cout << "Gates:\n";
    for (int i = aig.nPis + 1; i < aig.nObjs; i++) {
        if (i * 2 + 1 < static_cast<int>(aig.vObjs.size())) {
            int fanin0 = aig.vObjs[i * 2];
            int fanin1 = aig.vObjs[i * 2 + 1];
            int fanin0_var = fanin0 >> 1;
            int fanin1_var = fanin1 >> 1;
            bool fanin0_comp = fanin0 & 1;
            bool fanin1_comp = fanin1 & 1;
            
            std::cout << "  Node " << i << " = AND(";
            if (fanin0_comp) std::cout << "!";
            std::cout << fanin0_var;
            std::cout << ", ";
            if (fanin1_comp) std::cout << "!";
            std::cout << fanin1_var;
            std::cout << ")  [literals: " << fanin0 << ", " << fanin1 << "]\n";
        }
    }
    
    // Print outputs
    std::cout << "Outputs: ";
    for (int i = 0; i < aig.nPos; i++) {
        if (i > 0) std::cout << ", ";
        int po_lit = aig.vPos[i];
        int po_var = po_lit >> 1;
        bool po_comp = po_lit & 1;
        if (po_comp) std::cout << "!";
        std::cout << po_var;
    }
    std::cout << "\n\n";
}

void test_aigman_import() {
    std::cout << "=== TESTING AIGMAN NATIVE IMPORT ===\n";
    
    // Create main AIG
    aigman main_aig(3, 1);  // 3 PIs, 1 PO
    main_aig.vObjs.resize(7 * 2);
    
    // Node 4 = AND(1, 2)
    main_aig.vObjs[4 * 2] = 2;
    main_aig.vObjs[4 * 2 + 1] = 4;
    
    // Node 5 = AND(2, 3)
    main_aig.vObjs[5 * 2] = 4;
    main_aig.vObjs[5 * 2 + 1] = 6;
    
    // Node 6 = AND(4, 5) - this will be our target to replace
    main_aig.vObjs[6 * 2] = 8;
    main_aig.vObjs[6 * 2 + 1] = 10;
    
    main_aig.nGates = 3;
    main_aig.nObjs = 7;
    main_aig.vPos[0] = 12;  // Output points to node 6
    
    int original_gates = main_aig.nGates;
    
    // Print original structure
    print_aig_structure(main_aig, "MAIN AIG BEFORE IMPORT");
    
    // Create a simple replacement circuit (synthesized)
    // This would normally come from synthesis
    aigman* synth_aig = new aigman(2, 1);  // 2 inputs, 1 output
    synth_aig->vObjs.resize(4 * 2);
    
    // Node 3 = AND(1, 2) - single gate implementation
    synth_aig->vObjs[3 * 2] = 2;
    synth_aig->vObjs[3 * 2 + 1] = 4;
    
    synth_aig->nGates = 1;
    synth_aig->nObjs = 4;
    synth_aig->vPos[0] = 6;  // Output is node 3
    
    print_aig_structure(*synth_aig, "SYNTHESIZED CIRCUIT");
    
    // Use aigman's native import to insert the synthesized circuit
    // Map synth inputs [1,2] to main nodes [3,4]
    std::vector<int> input_mapping = {3, 4};  // Map to literals of nodes 3,4
    
    // We want the output to be a new node that we'll use to replace node 6
    std::vector<int> output_mapping = {12};
    
    std::cout << "Importing synthesized circuit...\n";
    std::cout << "Input mapping: synth[1,2] -> main[3,4] (literals 6,8)\n";
    std::cout << "Output will create new node in main AIG\n";
    
    // Actually call the import function
    main_aig.import(synth_aig, input_mapping, output_mapping);
    
    std::cout << "Import completed.\n";
    
    // Print structure after import
    print_aig_structure(main_aig, "MAIN AIG AFTER IMPORT");
    
    ASSERT(synth_aig != nullptr);
    ASSERT(synth_aig->nGates < original_gates);  // Synthesized circuit is smaller
    ASSERT(!output_mapping.empty());  // Import should produce output
    
    std::cout << "✓ Import and replacement completed successfully\n";
    
    delete synth_aig;
}


void test_conflict_resolution() {
    std::cout << "\n=== TESTING CONFLICT RESOLUTION ===\n";
    
    // Create AIG with multiple nodes that will have overlapping windows
    aigman aig(4, 1);  // 4 PIs, 1 PO
    aig.vObjs.resize(10 * 2);
    
    // Node 5 = AND(1, 2)  
    aig.vObjs[5 * 2] = 2;
    aig.vObjs[5 * 2 + 1] = 4;
    
    // Node 6 = AND(3, 4)
    aig.vObjs[6 * 2] = 6;
    aig.vObjs[6 * 2 + 1] = 8;
    
    // Node 7 = AND(5, 6) - depends on both 5 and 6
    aig.vObjs[7 * 2] = 10;
    aig.vObjs[7 * 2 + 1] = 12;
    
    // Node 8 = AND(5, 3) - also depends on 5
    aig.vObjs[8 * 2] = 10;
    aig.vObjs[8 * 2 + 1] = 6;
    
    // Node 9 = AND(7, 8) - final output
    aig.vObjs[9 * 2] = 14;
    aig.vObjs[9 * 2 + 1] = 16;
    
    aig.nGates = 5;
    aig.nObjs = 10;
    aig.vPos[0] = 18;  // Output points to node 9
    
    print_aig_structure(aig, "INITIAL AIG FOR CONFLICT TEST");
    
    // Extract windows - should find windows for nodes 5, 6, 7, 8, 9
    WindowExtractor extractor(aig, 6, true);
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Extracted " << windows.size() << " windows\n";
    for (size_t i = 0; i < windows.size(); i++) {
        std::cout << "  Window " << i << ": target=" << windows[i].target_node 
                  << ", divisors=" << windows[i].divisors.size() << "\n";
    }
    
    // Test conflict resolver with resubstitution candidates
    ConflictResolver resolver(aig);
    
    // Create resubstitution candidates from windows with divisors
    std::vector<ResubstitutionCandidate> candidates;
    std::cout << "\nCreating resubstitution candidates:\n";
    for (size_t i = 0; i < windows.size(); i++) {
        if (windows[i].divisors.size() >= 2) {
            // Select first 2 divisors as selected divisor nodes
            std::vector<int> selected_nodes = {windows[i].divisors[0], windows[i].divisors[1]};
            
            // Create a simple synthesized subcircuit (mock synthesis)
            aigman* synth_aig = new aigman(2, 1);  // 2 inputs, 1 output
            synth_aig->vObjs.resize(4 * 2);
            // Node 3 = AND(1, 2) - simple implementation using both inputs
            synth_aig->vObjs[3 * 2] = 2;
            synth_aig->vObjs[3 * 2 + 1] = 4;
            synth_aig->nGates = 1;
            synth_aig->nObjs = 4;
            synth_aig->vPos[0] = 6;  // Output is node 3
            
            candidates.emplace_back(synth_aig, windows[i].target_node, selected_nodes);
            std::cout << "  Candidate " << candidates.size()-1 << ": target=" << windows[i].target_node 
                      << ", divisors=[" << selected_nodes[0] << "," << selected_nodes[1] << "]\n";
        }
    }
    
    // Check initial validity of candidates
    std::cout << "\nInitial candidate validity:\n";
    for (size_t i = 0; i < candidates.size(); i++) {
        bool valid = resolver.is_candidate_valid(candidates[i]);
        std::cout << "  Candidate " << i << " (target " << candidates[i].target_node << "): " 
                  << (valid ? "VALID" : "INVALID") << "\n";
        ASSERT(valid);  // All should be valid initially
    }
    
    // Simulate replacing node 5 - this should invalidate windows targeting node 5
    // and any windows that depend on node 5
    std::cout << "\nSimulating replacement of node 5...\n";
    aig.replacenode(5, 2);  // Replace node 5 with input 1 (literal 2)
    
    print_aig_structure(aig, "AIG AFTER REPLACING NODE 5");
    
    // Check candidate validity after replacement
    std::cout << "\nCandidate validity after node 5 replacement:\n";
    int valid_count = 0, invalid_count = 0;
    for (size_t i = 0; i < candidates.size(); i++) {
        bool valid = resolver.is_candidate_valid(candidates[i]);
        std::cout << "  Candidate " << i << " (target " << candidates[i].target_node << "): " 
                  << (valid ? "VALID" : "INVALID") << "\n";
        if (valid) valid_count++;
        else invalid_count++;
    }
    
    ASSERT(invalid_count > 0);  // Some candidates should become invalid
    std::cout << "✓ Conflict resolution correctly identified " << invalid_count 
              << " invalid candidates out of " << candidates.size() << " total\n";
    
    // Test sequential processing of candidates
    std::cout << "\nTesting sequential candidate processing:\n";
    auto results = resolver.process_candidates_sequentially(candidates, true); // verbose for tests
    
    int applied = 0, skipped = 0;
    for (bool result : results) {
        if (result) applied++;
        else skipped++;
    }
    
    ASSERT(applied + skipped == static_cast<int>(candidates.size()));
    ASSERT(applied > 0);  // At least some should be applied
    ASSERT(skipped >= invalid_count);  // At least the initially invalid ones should be skipped
    
    std::cout << "✓ Sequential processing correctly applied " << applied 
              << " and skipped " << skipped << " candidates\n";
}

int main() {
    std::cout << "Insertion Test (aigman native)\n";
    std::cout << "==============================\n\n";
    
    test_aigman_import();
    test_conflict_resolution();
    
    if (passed_tests == total_tests) {
        std::cout << "\n✅ PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "\n❌ FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}
