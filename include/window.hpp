#pragma once

#include <vector>
#include <cstdint>
#include <unordered_set>
#include <aig.hpp>
#include <cut.hpp>


namespace fresub {

struct Window {
    int target_node;
    std::vector<int> inputs;     // Window inputs (cut leaves)
    std::vector<int> nodes;      // All nodes in window
    std::vector<int> divisors;   // Window nodes - MFFC(target)
    int cut_id;                  // ID of the cut that generated this window
};

// Window extraction using exopt's cut enumeration
class WindowExtractor {
public:
    WindowExtractor(aigman& aig, int max_cut_size = 6);
    
    void extract_all_windows(std::vector<Window>& windows);
    
    // MFFC computation using aigman (public for testing)
    std::unordered_set<int> compute_mffc(int root) const;
    
    // TFO computation within window bounds (public for testing)
    std::unordered_set<int> compute_tfo_in_window(int root, const std::vector<int>& window_nodes) const;
    
    // Truth table computation for window simulation
    // Returns vector<vector<word>> where:
    // - results[0..n-1] = divisors[0..n-1] truth tables
    // - results[n] = target truth table (at the end)
    std::vector<std::vector<uint64_t>> compute_truth_tables_for_window(
        int target_node,
        const std::vector<int>& window_inputs,
        const std::vector<int>& window_nodes,
        const std::vector<int>& divisors,
        bool verbose = false) const;
    
private:
    aigman& aig;
    int max_cut_size;
    std::vector<std::vector<Cut>> cuts;
    
    // Window creation from cuts
    void create_windows_from_cuts(std::vector<Window>& windows);
    void propagate_all_cuts_simultaneously(const std::vector<std::pair<int, Cut>>& all_cuts, 
                                          std::vector<Window>& windows);
    
    void collect_mffc_recursive(int node, std::unordered_set<int>& mffc, 
                                std::unordered_set<int>& visited) const;
    
    // Utility functions
    bool has_external_fanout(int node, int root) const;
    
    // Helper functions for aigman structure
    inline int lit2var(int lit) const { return lit >> 1; }
    inline bool is_complemented(int lit) const { return lit & 1; }
    inline int var2lit(int var, bool comp = false) const { return (var << 1) | (comp ? 1 : 0); }
};

} // namespace fresub
