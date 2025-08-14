#pragma once

#include "fresub_aig.hpp"
#include "window.hpp"
#include <vector>
#include <unordered_set>
#include <functional>

namespace fresub {

class ConflictResolver {
public:
    ConflictResolver(AIG& aig);
    
    // Check if a window is still valid for resubstitution
    bool is_window_valid(const Window& window) const;
    
    // Mark nodes as dead after successful resubstitution
    void mark_mffc_dead(int target_node);
    
    // Process windows sequentially, skipping conflicting ones
    std::vector<bool> process_windows_sequentially(
        const std::vector<Window>& windows,
        std::function<bool(const Window&)> resubstitution_func
    );
    
private:
    AIG& aig;
    std::unordered_set<int> dead_nodes;
    
    // Compute MFFC (Maximum Fanout-Free Cone) of a node
    std::vector<int> compute_mffc(int node) const;
    
    // Check if node is still alive and accessible
    bool is_node_accessible(int node) const;
};

} // namespace fresub