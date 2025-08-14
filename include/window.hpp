#pragma once

#include <vector>
#include <unordered_set>
#include <bitset>
#include <algorithm>
#include "fresub_aig.hpp"

namespace fresub {

struct Window {
    int target_node;
    std::vector<int> inputs;     // Window inputs (cut leaves)
    std::vector<int> nodes;      // All nodes in window
    std::vector<int> divisors;   // Window nodes - MFFC(target)
    int cut_id;                  // ID of the cut that generated this window
};

// Cut structure (based on exopt implementation)
struct Cut {
    std::vector<int> leaves;
    uint64_t signature;
    
    Cut() : signature(0) {}
    Cut(int node) : leaves{node}, signature(1ULL << (node % 64)) {}
};

// Cut enumeration (based on exopt algorithm)
class CutEnumerator {
public:
    CutEnumerator(AIG& aig, int max_cut_size = 6);
    
    void enumerate_cuts();
    const std::vector<Cut>& get_cuts(int node) const;
    
private:
    AIG& aig;
    int max_cut_size;
    std::vector<std::vector<Cut>> cuts;
    
    bool dominate(const Cut& a, const Cut& b) const;
};

// Window extraction using cut-based approach
class WindowExtractor {
public:
    WindowExtractor(AIG& aig, int max_cut_size = 6);
    
    void extract_all_windows(std::vector<Window>& windows);
    
private:
    AIG& aig;
    int max_cut_size;
    CutEnumerator cut_enumerator;
    
    // Window creation from cuts
    void create_windows_from_cuts(std::vector<Window>& windows);
    void propagate_all_cuts_simultaneously(const std::vector<std::pair<int, Cut>>& all_cuts, 
                                          std::vector<Window>& windows);
    void propagate_cut_to_window(int cut_id, const Cut& cut, int target_node, Window& window);
    
    // MFFC computation
    std::unordered_set<int> compute_mffc(int root) const;
    void collect_mffc_recursive(int node, int root, std::unordered_set<int>& mffc, 
                                std::unordered_set<int>& visited) const;
    
    // TFO computation within window bounds
    std::unordered_set<int> compute_tfo_in_window(int root, const std::vector<int>& window_nodes) const;
    
    // Utility functions
    bool has_external_fanout(int node, int root) const;
};

} // namespace fresub