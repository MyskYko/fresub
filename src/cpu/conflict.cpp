#include "conflict.hpp"
#include <functional>
#include <queue>
#include <iostream>

namespace fresub {

ConflictResolver::ConflictResolver(aigman& aig) : aig(aig) {}

bool ConflictResolver::is_node_accessible(int node) const {
    if (node <= 0 || node >= aig.nObjs) {
        return false;
    }
    return aig.vDeads.empty() || !aig.vDeads[node];
}


bool ConflictResolver::is_candidate_valid(const ResubstitutionCandidate& candidate) const {
    // (1) Check if target node still exists
    if (!is_node_accessible(candidate.target_node)) {
        return false;
    }
    
    // (2) Check if selected divisors still exist
    for (int divisor_node : candidate.selected_divisor_nodes) {
        if (!is_node_accessible(divisor_node)) {
            return false;
        }
    }
    
    // (3) Check if target node is NOT reachable to selected divisors
    if (!candidate.selected_divisor_nodes.empty()) {
        std::vector<int> target_nodes = {candidate.target_node};
        if (aig.reach(target_nodes, candidate.selected_divisor_nodes)) {
            return false;  // Target can reach selected divisors - candidate invalid
        }
    }
    
    return true;
}

std::vector<bool> ConflictResolver::process_candidates_sequentially(
    const std::vector<ResubstitutionCandidate>& candidates, bool verbose) const {
    
    std::vector<bool> results(candidates.size(), false);
    int applied = 0, skipped = 0;
    
    if (verbose) {
        std::cout << "Processing " << candidates.size() << " resubstitution candidates sequentially...\n";
    }
    
    for (size_t i = 0; i < candidates.size(); i++) {
        const ResubstitutionCandidate& candidate = candidates[i];
        
        // Check if candidate is still valid
        if (!is_candidate_valid(candidate)) {
            if (verbose) {
                std::cout << "  Candidate " << i << " (target " << candidate.target_node 
                          << "): SKIPPED (invalid)\n";
            }
            skipped++;
            continue;
        }
	
	std::vector<int> outputs = {candidates[i].target_node << 1};
        aig.import(candidates[i].aig, candidates[i].selected_divisor_nodes, outputs);
	
        if (verbose) {
            std::cout << "  Candidate " << i << " (target " << candidate.target_node 
                      << "): APPLIED (synthesized with " << candidate.selected_divisor_nodes.size() 
                      << " divisors)\n";
        }
        results[i] = true;
        applied++;
    }
    
    if (verbose) {
        std::cout << "Sequential processing complete: " << applied 
                  << " applied, " << skipped << " skipped\n";
    }
    
    return results;
}

} // namespace fresub
