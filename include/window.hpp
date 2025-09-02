#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <aig.hpp>
#include <cut.hpp>


namespace fresub {

  struct FeasibleSet {
    std::vector<int> divisor_indices; // indices into window.divisors
    std::vector<int> divisor_nodes;   // resolved node IDs for convenience
    std::vector<aigman*> synths;      // synthesized subcircuits for this set
  };

  struct Window {
    int target_node;
    std::vector<int> inputs;     // Window inputs (cut leaves)
    std::vector<int> nodes;      // All nodes in window
    std::vector<int> divisors;   // Window nodes - MFFC(target)
    int cut_id;                  // ID of the cut that generated this window
    int mffc_size;
    std::vector<std::vector<uint64_t>> truth_tables;
    std::vector<FeasibleSet> feasible_sets; // optional: enriched storage per feasible set
  };

  // Window extraction using exopt's cut enumeration
  class WindowExtractor {
  public:
    WindowExtractor(aigman& aig, int max_cut_size, bool verbose);
    
    void extract_all_windows(std::vector<Window>& windows);
    
    // MFFC computation using aigman (public for testing)
    std::unordered_set<int> compute_mffc(int root) const;
    
    // TFO computation within window bounds (public for testing)
    std::unordered_set<int> compute_tfo_in_window(int root, const std::vector<int>& window_nodes) const;
    
  private:
    aigman& aig;
    int max_cut_size;
    bool verbose;
    std::vector<std::vector<Cut>> cuts;
    
    // Window creation from cuts
    void create_windows_from_cuts(std::vector<Window>& windows);
    
    void collect_mffc_recursive(int node, std::unordered_set<int>& mffc) const;
    
    // Utility functions
    bool has_external_fanout(int node, int root) const;
    
    // Helper functions for aigman structure
    inline int lit2var(int lit) const { return lit >> 1; }
    inline bool is_complemented(int lit) const { return lit & 1; }
    inline int var2lit(int var, bool comp = false) const { return (var << 1) | (comp ? 1 : 0); }
  };

} // namespace fresub
