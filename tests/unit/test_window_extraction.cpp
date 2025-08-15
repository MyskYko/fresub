#include "window.hpp"
#include <aig.hpp>
#include <iostream>
#include <cassert>
#include <algorithm>

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

void print_cut(const Cut& cut) {
    std::cout << "{";
    for (size_t i = 0; i < cut.leaves.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << cut.leaves[i];
    }
    std::cout << "}";
}

void print_node_list(const std::vector<int>& nodes) {
    std::cout << "[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << nodes[i];
    }
    std::cout << "]";
}

void print_aig_structure(const aigman& aig) {
    std::cout << "=== AIG STRUCTURE ===\n";
    std::cout << "nPis: " << aig.nPis << ", nGates: " << aig.nGates 
              << ", nPos: " << aig.nPos << ", nObjs: " << aig.nObjs << "\n";
    
    // Print primary inputs
    std::cout << "Primary Inputs: ";
    for (int i = 1; i <= aig.nPis; i++) {
        if (i > 1) std::cout << ", ";
        std::cout << i;
    }
    std::cout << "\n";
    
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
            std::cout << ")";
            std::cout << "  [fanins: " << fanin0 << ", " << fanin1 << "]\n";
        }
    }
    
    // Print primary outputs
    std::cout << "Primary Outputs: ";
    for (int i = 0; i < aig.nPos; i++) {
        if (i > 0) std::cout << ", ";
        int po_lit = aig.vPos[i];
        int po_var = po_lit >> 1;
        bool po_comp = po_lit & 1;
        if (po_comp) std::cout << "!";
        std::cout << po_var;
        std::cout << " [literal: " << po_lit << "]";
    }
    std::cout << "\n\n";
}

void test_hardcoded_aig() {
    std::cout << "=== TESTING HARDCODED AIG FOR MFFC/TFO VERIFICATION ===\n";
    
    // Create a hardcoded AIG with known structure:
    // PIs: 1, 2, 3
    // Node 4 = AND(1, 2)     // depends on PIs 1,2
    // Node 5 = AND(2, 3)     // depends on PIs 2,3  
    // Node 6 = AND(4, 5)     // depends on nodes 4,5 -> PIs 1,2,3
    // Node 7 = AND(4, 3)     // depends on node 4 and PI 3 -> PIs 1,2,3
    // Node 8 = AND(6, 7)     // depends on nodes 6,7 -> PIs 1,2,3
    // PO: 8
    
    aigman aig(3, 1);  // 3 PIs, 1 PO
    
    // Resize vObjs to accommodate all nodes (up to node 8)
    aig.vObjs.resize(9 * 2); // nodes 0-8, each needs 2 entries
    
    // Node 4 = AND(1, 2)
    aig.vObjs[4 * 2] = 2;     // fanin0 = PI1 (literal 2)
    aig.vObjs[4 * 2 + 1] = 4; // fanin1 = PI2 (literal 4)
    
    // Node 5 = AND(2, 3)
    aig.vObjs[5 * 2] = 4;     // fanin0 = PI2 (literal 4)
    aig.vObjs[5 * 2 + 1] = 6; // fanin1 = PI3 (literal 6)
    
    // Node 6 = AND(4, 5)
    aig.vObjs[6 * 2] = 8;     // fanin0 = Node4 (literal 8)
    aig.vObjs[6 * 2 + 1] = 10; // fanin1 = Node5 (literal 10)
    
    // Node 7 = AND(4, 3)
    aig.vObjs[7 * 2] = 8;     // fanin0 = Node4 (literal 8)
    aig.vObjs[7 * 2 + 1] = 6; // fanin1 = PI3 (literal 6)
    
    // Node 8 = AND(6, 7)
    aig.vObjs[8 * 2] = 12;    // fanin0 = Node6 (literal 12)
    aig.vObjs[8 * 2 + 1] = 14; // fanin1 = Node7 (literal 14)
    
    aig.nGates = 5;           // 5 gates (nodes 4,5,6,7,8)
    aig.nObjs = 9;            // 9 objects total (0,1,2,3,4,5,6,7,8)
    aig.vPos[0] = 16;         // Output points to node 8 (literal 16)
    
    std::cout << "Created hardcoded AIG:\n";
    print_aig_structure(aig);
    
    // Test MFFC and TFO for specific targets
    WindowExtractor extractor(aig, 4);
    
    std::cout << "=== TESTING MFFC COMPUTATION ===\n";
    
    // Test MFFC for node 6 (should include 6,5 since 5 only feeds 6, but not 4 which feeds both 6&7)
    auto mffc_6 = extractor.compute_mffc(6);
    std::cout << "MFFC(6): {";
    bool first = true;
    for (int node : mffc_6) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}\n";
    ASSERT(mffc_6.size() == 2);
    ASSERT(mffc_6.find(5) != mffc_6.end());
    ASSERT(mffc_6.find(6) != mffc_6.end());
    ASSERT(mffc_6.find(4) == mffc_6.end()); // 4 feeds both 6 and 7
    std::cout << "✓ MFFC(6) correct: {5, 6}\n";
    
    // Test MFFC for node 8 (should include all nodes since 8 is the only output)
    auto mffc_8 = extractor.compute_mffc(8);
    std::cout << "MFFC(8): {";
    first = true;
    for (int node : mffc_8) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}\n";
    ASSERT(mffc_8.size() == 5);
    ASSERT(mffc_8.find(4) != mffc_8.end());
    ASSERT(mffc_8.find(5) != mffc_8.end());
    ASSERT(mffc_8.find(6) != mffc_8.end());
    ASSERT(mffc_8.find(7) != mffc_8.end());
    ASSERT(mffc_8.find(8) != mffc_8.end());
    std::cout << "✓ MFFC(8) correct: {4, 5, 6, 7, 8}\n";
    
    std::cout << "\n=== TESTING TFO COMPUTATION ===\n";
    
    // Test TFO for node 4 within full circuit (4 feeds 6,7 which feed 8)
    std::vector<int> all_nodes = {1, 2, 3, 4, 5, 6, 7, 8};
    auto tfo_4 = extractor.compute_tfo_in_window(4, all_nodes);
    std::cout << "TFO(4) in full circuit: {";
    first = true;
    for (int node : tfo_4) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}\n";
    ASSERT(tfo_4.size() == 4);
    ASSERT(tfo_4.find(4) != tfo_4.end());
    ASSERT(tfo_4.find(6) != tfo_4.end());
    ASSERT(tfo_4.find(7) != tfo_4.end());
    ASSERT(tfo_4.find(8) != tfo_4.end());
    std::cout << "✓ TFO(4) correct: {4, 6, 7, 8}\n";
    
    // Test TFO for node 5 (5 feeds 6 which feeds 8)
    auto tfo_5 = extractor.compute_tfo_in_window(5, all_nodes);
    std::cout << "TFO(5) in full circuit: {";
    first = true;
    for (int node : tfo_5) {
        if (!first) std::cout << ", ";
        std::cout << node;
        first = false;
    }
    std::cout << "}\n";
    ASSERT(tfo_5.size() == 3);
    ASSERT(tfo_5.find(5) != tfo_5.end());
    ASSERT(tfo_5.find(6) != tfo_5.end());
    ASSERT(tfo_5.find(8) != tfo_5.end());
    std::cout << "✓ TFO(5) correct: {5, 6, 8}\n";
    
    std::cout << "\n=== TESTING WINDOW EXTRACTION ===\n";
    
    // Extract windows and validate divisors
    std::vector<Window> windows;
    extractor.extract_all_windows(windows);
    
    std::cout << "Generated " << windows.size() << " windows:\n";
    ASSERT(!windows.empty());
    
    // Test specific windows and verify divisor correctness
    for (const auto& window : windows) {
        std::cout << "Window (target=" << window.target_node << "):\n";
        std::cout << "  Inputs: ";
        print_node_list(window.inputs);
        std::cout << "\n  Nodes: ";
        print_node_list(window.nodes);
        std::cout << "\n  Divisors: ";
        print_node_list(window.divisors);
        std::cout << "\n";
        
        // Compute MFFC and TFO for this target
        auto mffc = extractor.compute_mffc(window.target_node);
        auto tfo = extractor.compute_tfo_in_window(window.target_node, window.nodes);
        
        // Verify divisors don't include MFFC nodes
        for (int divisor : window.divisors) {
            ASSERT(mffc.find(divisor) == mffc.end()); // Divisor should NOT be in MFFC
        }
        
        // Verify divisors don't include TFO nodes
        for (int divisor : window.divisors) {
            ASSERT(tfo.find(divisor) == tfo.end()); // Divisor should NOT be in TFO
        }
        
        // Verify all divisors are in window nodes
        for (int divisor : window.divisors) {
            ASSERT(std::find(window.nodes.begin(), window.nodes.end(), divisor) != window.nodes.end());
        }
        
        std::cout << "  ✓ Divisors correctly exclude MFFC(" << window.target_node << ") and TFO(" << window.target_node << ")\n\n";
    }
}


int main() {
    std::cout << "Window Extraction Test (aigman + exopt)\n";
    std::cout << "========================================\n\n";
    
    // Test hardcoded AIG for verification
    test_hardcoded_aig();
    
    if (passed_tests == total_tests) {
        std::cout << "\n✅ PASSED (" << passed_tests << "/" << total_tests << ")\n";
        return 0;
    } else {
        std::cout << "\n❌ FAILED (" << passed_tests << "/" << total_tests << ")\n";
        return 1;
    }
}
