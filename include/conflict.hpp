#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "resub.hpp"

namespace fresub {

struct Commit {
    int commit_id;
    int target_node;
    std::unordered_set<int> affected_nodes;  // Nodes modified by this commit
    std::unordered_set<int> dependent_nodes; // Nodes that depend on modified nodes
    ResubResult result;
    int priority;  // Priority for conflict resolution
};

class ConflictResolver {
public:
    ConflictResolver(AIG& aig);
    
    // Check if two commits conflict
    bool check_conflict(const Commit& c1, const Commit& c2) const;
    
    // Find all conflicts in a set of commits
    void find_conflicts(const std::vector<Commit>& commits,
                        std::vector<std::pair<int, int>>& conflicts);
    
    // Resolve conflicts and return non-conflicting commits
    std::vector<Commit> resolve_conflicts(const std::vector<Commit>& commits);
    
    // Build dependency graph for commits
    void build_dependency_graph(const std::vector<Commit>& commits);
    
    // Compute affected nodes for a resubstitution
    std::unordered_set<int> compute_affected_nodes(const ResubResult& result);
    
private:
    AIG& aig;
    std::unordered_map<int, std::vector<int>> dependency_graph;
    
    // Conflict resolution strategies
    std::vector<Commit> greedy_resolution(const std::vector<Commit>& commits,
                                          const std::vector<std::pair<int, int>>& conflicts);
    
    std::vector<Commit> max_independent_set(const std::vector<Commit>& commits,
                                            const std::vector<std::pair<int, int>>& conflicts);
    
    // Helper functions
    void compute_transitive_fanout(int node, std::unordered_set<int>& fanout);
    bool nodes_overlap(const std::unordered_set<int>& set1, 
                       const std::unordered_set<int>& set2) const;
};

// Parallel execution manager
class ParallelResubManager {
public:
    ParallelResubManager(AIG& aig, int num_threads = 8);
    
    // Execute resubstitution in parallel with conflict detection
    void parallel_resubstitute(const std::vector<Window>& windows);
    
    // Apply non-conflicting commits to the AIG
    void apply_commits(const std::vector<Commit>& commits);
    
    // Get statistics
    struct Stats {
        int total_windows;
        int successful_resubs;
        int conflicts_detected;
        int commits_applied;
        int total_gain;
    };
    
    Stats get_stats() const { return stats; }
    
private:
    AIG& aig;
    int num_threads;
    ConflictResolver resolver;
    ResubEngine engine;
    Stats stats;
    
    // Batch processing
    void process_batch(const std::vector<Window>& batch,
                       std::vector<Commit>& commits);
};

} // namespace fresub