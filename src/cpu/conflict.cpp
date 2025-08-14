#include "conflict.hpp"
#include <algorithm>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>

namespace fresub {

ConflictResolver::ConflictResolver(AIG& aig) : aig(aig) {}

bool ConflictResolver::check_conflict(const Commit& c1, const Commit& c2) const {
    // Two commits conflict if they modify overlapping nodes
    return nodes_overlap(c1.affected_nodes, c2.affected_nodes) ||
           nodes_overlap(c1.affected_nodes, c2.dependent_nodes) ||
           nodes_overlap(c1.dependent_nodes, c2.affected_nodes);
}

void ConflictResolver::find_conflicts(const std::vector<Commit>& commits,
                                      std::vector<std::pair<int, int>>& conflicts) {
    conflicts.clear();
    
    for (size_t i = 0; i < commits.size(); i++) {
        for (size_t j = i + 1; j < commits.size(); j++) {
            if (check_conflict(commits[i], commits[j])) {
                conflicts.push_back({i, j});
            }
        }
    }
}

std::vector<Commit> ConflictResolver::resolve_conflicts(const std::vector<Commit>& commits) {
    // Find all conflicts
    std::vector<std::pair<int, int>> conflicts;
    find_conflicts(commits, conflicts);
    
    if (conflicts.empty()) {
        return commits;  // No conflicts
    }
    
    // Use greedy resolution by default
    return greedy_resolution(commits, conflicts);
}

void ConflictResolver::build_dependency_graph(const std::vector<Commit>& commits) {
    dependency_graph.clear();
    
    for (size_t i = 0; i < commits.size(); i++) {
        dependency_graph[i] = std::vector<int>();
        
        for (size_t j = 0; j < commits.size(); j++) {
            if (i != j && check_conflict(commits[i], commits[j])) {
                dependency_graph[i].push_back(j);
            }
        }
    }
}

std::unordered_set<int> ConflictResolver::compute_affected_nodes(const ResubResult& result) {
    std::unordered_set<int> affected;
    
    // Add the target node itself
    affected.insert(result.target_node);
    
    // Add all nodes in the transitive fanout
    compute_transitive_fanout(result.target_node, affected);
    
    return affected;
}

std::vector<Commit> ConflictResolver::greedy_resolution(
    const std::vector<Commit>& commits,
    const std::vector<std::pair<int, int>>& conflicts) {
    
    // Sort commits by priority (gain)
    std::vector<std::pair<int, int>> sorted_commits;
    for (size_t i = 0; i < commits.size(); i++) {
        sorted_commits.push_back({commits[i].priority, i});
    }
    std::sort(sorted_commits.begin(), sorted_commits.end(), std::greater<>());
    
    // Greedily select non-conflicting commits
    std::vector<bool> selected(commits.size(), false);
    std::vector<bool> excluded(commits.size(), false);
    
    for (const auto& [priority, idx] : sorted_commits) {
        if (excluded[idx]) continue;
        
        selected[idx] = true;
        
        // Exclude all conflicting commits
        for (const auto& [i, j] : conflicts) {
            if (i == idx) excluded[j] = true;
            if (j == idx) excluded[i] = true;
        }
    }
    
    // Collect selected commits
    std::vector<Commit> result;
    for (size_t i = 0; i < commits.size(); i++) {
        if (selected[i]) {
            result.push_back(commits[i]);
        }
    }
    
    return result;
}

std::vector<Commit> ConflictResolver::max_independent_set(
    const std::vector<Commit>& commits,
    const std::vector<std::pair<int, int>>& conflicts) {
    
    // Build conflict graph
    int n = commits.size();
    std::vector<std::vector<int>> adj(n);
    
    for (const auto& [i, j] : conflicts) {
        adj[i].push_back(j);
        adj[j].push_back(i);
    }
    
    // Use branch-and-bound or approximation algorithm
    // For now, use greedy approximation
    std::vector<bool> selected(n, false);
    std::vector<int> degree(n);
    
    // Calculate degrees
    for (int i = 0; i < n; i++) {
        degree[i] = adj[i].size();
    }
    
    // Iteratively select node with minimum degree
    int remaining = n;
    while (remaining > 0) {
        // Find node with minimum degree
        int min_degree = n + 1;
        int min_node = -1;
        
        for (int i = 0; i < n; i++) {
            if (!selected[i] && degree[i] >= 0 && degree[i] < min_degree) {
                min_degree = degree[i];
                min_node = i;
            }
        }
        
        if (min_node == -1) break;
        
        // Select this node
        selected[min_node] = true;
        degree[min_node] = -1;
        remaining--;
        
        // Remove neighbors
        for (int neighbor : adj[min_node]) {
            if (degree[neighbor] >= 0) {
                degree[neighbor] = -1;
                remaining--;
                
                // Update degrees of neighbors' neighbors
                for (int nn : adj[neighbor]) {
                    if (degree[nn] > 0) {
                        degree[nn]--;
                    }
                }
            }
        }
    }
    
    // Collect selected commits
    std::vector<Commit> result;
    for (int i = 0; i < n; i++) {
        if (selected[i]) {
            result.push_back(commits[i]);
        }
    }
    
    return result;
}

void ConflictResolver::compute_transitive_fanout(int node, std::unordered_set<int>& fanout) {
    std::queue<int> queue;
    queue.push(node);
    
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        
        for (int fo : aig.nodes[current].fanouts) {
            if (fanout.find(fo) == fanout.end()) {
                fanout.insert(fo);
                queue.push(fo);
            }
        }
    }
}

bool ConflictResolver::nodes_overlap(const std::unordered_set<int>& set1,
                                     const std::unordered_set<int>& set2) const {
    for (int node : set1) {
        if (set2.find(node) != set2.end()) {
            return true;
        }
    }
    return false;
}

// ParallelResubManager implementation
ParallelResubManager::ParallelResubManager(AIG& aig, int num_threads)
    : aig(aig), num_threads(num_threads), resolver(aig), engine(aig, true) {
    stats = {0, 0, 0, 0, 0};
}

void ParallelResubManager::parallel_resubstitute(const std::vector<Window>& windows) {
    stats.total_windows = windows.size();
    
    std::cout << "=== Resubstitution Phase ===\n";
    std::cout << "Processing " << windows.size() << " windows with " << num_threads << " threads\n";
    
    // Process windows in batches
    const int batch_size = 100;
    std::cout << "Batch size: " << batch_size << " windows per batch\n";
    
    std::vector<Commit> all_commits;
    std::mutex commits_mutex;
    
    // Parallel processing using threads
    std::vector<std::thread> threads;
    std::atomic<size_t> window_index(0);
    std::atomic<int> batches_completed(0);
    int total_batches = (windows.size() + batch_size - 1) / batch_size;
    
    std::cout << "Starting " << num_threads << " threads to process " << total_batches << " batches\n";
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            std::vector<Commit> local_commits;
            int thread_batches = 0;
            
            while (true) {
                size_t start = window_index.fetch_add(batch_size);
                if (start >= windows.size()) break;
                
                size_t end = std::min(start + batch_size, windows.size());
                std::vector<Window> batch(windows.begin() + start, windows.begin() + end);
                thread_batches++;
                
                std::cout << "Thread " << t << " processing batch " << (batches_completed + 1) 
                         << "/" << total_batches << " (windows " << start << "-" << (end-1) << ")\n";
                
                process_batch(batch, local_commits);
                
                std::lock_guard<std::mutex> lock(commits_mutex);
                all_commits.insert(all_commits.end(), local_commits.begin(), local_commits.end());
                batches_completed++;
                
                std::cout << "Thread " << t << " completed batch, found " << local_commits.size() 
                         << " commits (total so far: " << all_commits.size() << ")\n";
                local_commits.clear();
            }
            
            std::cout << "Thread " << t << " finished after processing " << thread_batches << " batches\n";
        });
    }
    
    // Wait for all threads
    std::cout << "Waiting for all threads to complete...\n";
    for (auto& thread : threads) {
        thread.join();
    }
    
    stats.successful_resubs = all_commits.size();
    std::cout << "Parallel processing complete. Found " << all_commits.size() 
              << " potential resubstitutions\n";
    
    // Compute affected nodes in serial phase to avoid race conditions
    std::cout << "=== Conflict Analysis Phase ===\n";
    std::cout << "Computing affected nodes for conflict detection...\n";
    for (auto& commit : all_commits) {
        commit.affected_nodes = resolver.compute_affected_nodes(commit.result);
        commit.dependent_nodes = commit.affected_nodes;  // Simplified
    }
    
    // Resolve conflicts
    std::cout << "Resolving conflicts between " << all_commits.size() << " commits...\n";
    
    // Show conflict analysis details
    std::vector<std::pair<int, int>> conflicts;
    resolver.find_conflicts(all_commits, conflicts);
    std::cout << "Conflict detection found " << conflicts.size() << " pairwise conflicts:\n";
    
    if (conflicts.size() > 0) {
        // Show first few conflicts for insight
        int show_conflicts = std::min(5, (int)conflicts.size());
        for (int i = 0; i < show_conflicts; i++) {
            int idx1 = conflicts[i].first, idx2 = conflicts[i].second;
            std::cout << "  Conflict " << (i+1) << ": commits targeting nodes " 
                     << all_commits[idx1].target_node << " and " 
                     << all_commits[idx2].target_node << " interfere\n";
        }
        if (conflicts.size() > 5) {
            std::cout << "  ... and " << (conflicts.size() - 5) << " more conflicts\n";
        }
    }
    
    std::vector<Commit> non_conflicting = resolver.resolve_conflicts(all_commits);
    stats.conflicts_detected = all_commits.size() - non_conflicting.size();
    std::cout << "Conflict resolution complete:\n";
    std::cout << "  Total candidates: " << all_commits.size() << "\n";
    std::cout << "  Pairwise conflicts: " << conflicts.size() << "\n";
    std::cout << "  Commits excluded due to conflicts: " << stats.conflicts_detected << "\n"; 
    std::cout << "  Non-conflicting commits selected: " << non_conflicting.size() << "\n";
    
    // Apply commits
    std::cout << "=== AIG Modification Phase ===\n";
    std::cout << "Applying " << non_conflicting.size() << " resubstitutions to AIG...\n";
    apply_commits(non_conflicting);
    stats.commits_applied = non_conflicting.size();
    
    // Calculate total gain
    for (const auto& commit : non_conflicting) {
        stats.total_gain += commit.result.actual_gain;
    }
    
    std::cout << "Resubstitution complete! Total gain: " << stats.total_gain << " gates\n";
}

void ParallelResubManager::process_batch(const std::vector<Window>& batch,
                                         std::vector<Commit>& commits) {
    // Process batch using GPU if available
    std::vector<ResubResult> results = engine.resubstitute_batch(batch);
    
    for (size_t i = 0; i < results.size(); i++) {
        if (results[i].success) {
            Commit commit;
            commit.commit_id = commits.size();
            commit.target_node = results[i].target_node;
            commit.result = results[i];
            commit.priority = results[i].actual_gain;
            
            // AVOID RACE CONDITION: Don't compute affected nodes during parallel execution
            // We'll compute this later in the serial phase
            commit.affected_nodes.clear();
            commit.dependent_nodes.clear();
            
            commits.push_back(commit);
        }
    }
}

void ParallelResubManager::apply_commits(const std::vector<Commit>& commits) {
    int applied_count = 0;
    int synthesis_failures = 0;
    
    for (const auto& commit : commits) {
        // Apply the resubstitution by synthesizing and replacing the node
        // This is now done serially to avoid race conditions
        
        if (applied_count < 5) { // Show details for first few
            std::cout << "  Commit " << (applied_count + 1) << ": replacing node " 
                     << commit.target_node << " using " << commit.result.divisor.divisor_ids.size() 
                     << " divisors [";
            for (size_t i = 0; i < commit.result.divisor.divisor_ids.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << commit.result.divisor.divisor_ids[i];
            }
            std::cout << "]\n";
        }
        
        // For now, create a simple wrapper to access the window
        // In a more complete implementation, we'd store the window in the commit
        Window dummy_window;
        dummy_window.target_node = commit.target_node;
        
        // Synthesize replacement circuit
        int new_node = engine.synthesize_replacement(dummy_window, commit.result.divisor);
        if (new_node > 0) {
            // Replace target node with new implementation - now safe to do serially
            aig.replace_node(commit.target_node, new_node);
            applied_count++;
        } else {
            synthesis_failures++;
        }
    }
    
    std::cout << "Applied " << applied_count << " commits successfully";
    if (synthesis_failures > 0) {
        std::cout << " (" << synthesis_failures << " synthesis failures)";
    }
    std::cout << "\n";
}

} // namespace fresub