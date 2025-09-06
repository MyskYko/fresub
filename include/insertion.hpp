#pragma once

#include <vector>

#include <aig.hpp>
#include "window.hpp"

namespace fresub {

  class Inserter {
  public:
    Inserter(aigman& aig);

    // New: Process windows directly using a gain-ordered heap over feasible sets
    // Returns number of applied resubstitutions
    int process_windows_heap(std::vector<Window>& windows, bool verbose = false) const;
    
  private:
    aigman& aig;
    
    // Check if node is still alive and accessible
    bool is_node_accessible(int node) const;
  };

} // namespace fresub
