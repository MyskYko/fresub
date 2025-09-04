#include "aig_utils.hpp"

#include <iostream>
#include <cassert>
#include <queue>
#include <unordered_set>

namespace fresub {

// Recursive helper for deref-based MFFC
static void mffc_deref_dfs(aigman& aig,
                           int n,
                           std::vector<int>& deref,
                           std::unordered_set<int>& cone,
                           std::vector<int>& touched) {
  int f0 = aig.vObjs[n * 2] >> 1;
  int f1 = aig.vObjs[n * 2 + 1] >> 1;
  int fis[2] = {f0, f1};
  for (int idx = 0; idx < 2; ++idx) {
    int fi = fis[idx];
    if (fi <= aig.nPis) continue; // stop at PIs
    if (deref[fi] == 0) touched.push_back(fi);
    ++deref[fi];
    int eff_ref = static_cast<int>(aig.vvFanouts[fi].size()) - deref[fi];
    if (eff_ref == 0) {
      cone.insert(fi);
      mffc_deref_dfs(aig, fi, deref, cone, touched);
    }
  }
}

std::unordered_set<int> compute_mffc(aigman& aig, int root, std::vector<int>& deref) {
  if (aig.vvFanouts.empty()) {
    aig.supportfanouts();
  }
  // Ensure deref has capacity; caller should have zero-initialized it
  if (static_cast<int>(deref.size()) < aig.nObjs) deref.resize(aig.nObjs);

  // Result container and touched list for restoring deref
  std::unordered_set<int> cone;
  cone.reserve(16);
  std::vector<int> touched;
  touched.reserve(32);

  // Root must be a gate and not pre-primed
  assert(root > aig.nPis);
  assert(deref[root] == 0);

  // Seed: pretend all fanouts of root are removed, so it enters the cone
  touched.push_back(root);
  deref[root] = static_cast<int>(aig.vvFanouts[root].size());
  cone.insert(root);

  // Recurse on fanins
  mffc_deref_dfs(aig, root, deref, cone, touched);

  // Restore deref to 0
  for (int t : touched) deref[t] = 0;
  return cone;
}

std::unordered_set<int> compute_mffc_excluding_divisors(
  aigman& aig,
  int root,
  std::vector<int>& deref,
  const std::vector<int>& divisors_to_exclude) {
  // Ensure deref has capacity and is zero-initialized for untouched entries
  if (static_cast<int>(deref.size()) < aig.nObjs) deref.resize(aig.nObjs);
  // Prime deref for divisor nodes to simulate a persistent external fanout
  // so they (and their TFI) never enter the MFFC during this run.
  for (int d : divisors_to_exclude) {
    // caller guarantees valid ids; no bounds checks for speed
    // use -1 so even after consuming all internal fanouts, eff_ref stays > 0
    deref[d] = -1;
  }

  auto cone = compute_mffc(aig, root, deref);

  // Restore divisor deref entries to 0
  for (int d : divisors_to_exclude) {
    deref[d] = 0;
  }
  return cone;
}

void print_aig(const aigman& aig, const std::string& label) {
  std::cout << "=== " << label << " ===\n";
  std::cout << "nPis: " << aig.nPis << ", nGates: " << aig.nGates
            << ", nPos: " << aig.nPos << ", nObjs: " << aig.nObjs << "\n";

  std::cout << "PIs: ";
  for (int i = 1; i <= aig.nPis; ++i) {
    if (i > 1) std::cout << ", ";
    std::cout << i;
  }
  std::cout << "\n";

  std::cout << "Gates:\n";
  for (int i = aig.nPis + 1; i < aig.nObjs; ++i) {
    if (i * 2 + 1 < static_cast<int>(aig.vObjs.size())) {
      int f0 = aig.vObjs[i * 2];
      int f1 = aig.vObjs[i * 2 + 1];
      int v0 = f0 >> 1;
      int v1 = f1 >> 1;
      bool c0 = (f0 & 1) != 0;
      bool c1 = (f1 & 1) != 0;
      std::cout << "  Node " << i << " = AND(" << (c0 ? "!" : "") << v0
                << ", " << (c1 ? "!" : "") << v1 << ")  [lits: " << f0
                << ", " << f1 << "]\n";
    }
  }

  std::cout << "POs: ";
  for (int i = 0; i < aig.nPos; ++i) {
    if (i) std::cout << ", ";
    int lit = aig.vPos[i];
    int var = lit >> 1;
    bool comp = (lit & 1) != 0;
    if (comp) std::cout << "!";
    std::cout << var << " [lit: " << lit << "]";
  }
  std::cout << "\n\n";
}

} // namespace fresub
