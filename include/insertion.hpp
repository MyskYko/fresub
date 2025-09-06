#pragma once

#include <vector>

#include <aig.hpp>
#include "window.hpp"

namespace fresub {

  // Process windows directly using a gain-ordered heap over feasible sets.
  // Returns number of applied resubstitutions.
  int inserter_process_windows_heap(aigman& aig, std::vector<Window>& windows, bool verbose = false);

} // namespace fresub
