#pragma once

#include <unordered_set>
#include <string>
#include <vector>

#include <aig.hpp>

namespace fresub {

// Compute the MFFC (maximum fanout-free cone) using a dereference counter array.
// - Assumes `deref` entries are all 0 on entry; the function will restore all
//   touched entries back to 0 before returning.
// - Returns the set of node IDs that belong to the MFFC, including the root.
// - Asserts `root` is not a PI.
std::unordered_set<int> compute_mffc(aigman& aig, int root, std::vector<int>& deref);

// Compute MFFC while excluding specific divisor nodes (and implicitly their TFI)
// by priming their deref counts to 1. The function will restore those entries
// back to 0 before returning.
std::unordered_set<int> compute_mffc_excluding_divisors(
  aigman& aig,
  int root,
  std::vector<int>& deref,
  const std::vector<int>& divisors_to_exclude);

// Debug-print the full AIG structure (PIs, gates, POs) with a label.
void print_aig(const aigman& aig, const std::string& label = "AIG");

} // namespace fresub
