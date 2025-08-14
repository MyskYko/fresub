#include "resub.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unordered_map>

#ifdef USE_CUDA
#include "cuda_kernels.cuh"
#endif

namespace fresub {

ResubEngine::ResubEngine(AIG& aig, bool use_gpu) 
    : aig(aig), use_gpu(use_gpu) {}

ResubResult ResubEngine::resubstitute(const Window& window) {
    ResubResult result;
    result.target_node = window.target_node;
    result.success = false;
    result.actual_gain = 0;
    
    // Prepare resubstitution problem
    ResubProblem problem;
    problem.window_id = 0;
    problem.target_node = window.target_node;
    problem.num_vars = window.inputs.size();
    
    // Compute truth tables for divisors
    problem.divisor_truths.resize(window.divisors.size());
    for (size_t i = 0; i < window.divisors.size(); i++) {
        compute_truth_table(window.divisors[i], window.inputs, problem.divisor_truths[i]);
    }
    
    // Compute target function truth tables
    compute_truth_table(window.target_node, window.inputs, problem.target_on);
    
    // Debug: Show truth table information for first few resubstitutions
    static int debug_count = 0;
    if (debug_count < 3) {
        std::cout << "\n--- Resubstitution Analysis " << (debug_count + 1) << " ---\n";
        std::cout << "Target node " << window.target_node << " with " << window.inputs.size() << " inputs\n";
        
        // Show target truth table (first 64 bits)
        if (!problem.target_on.empty()) {
            uint64_t target_tt = problem.target_on[0];
            int num_patterns = std::min(16, 1 << window.inputs.size()); // Show up to 16 patterns
            std::cout << "Target truth table (first " << num_patterns << " patterns): ";
            for (int p = 0; p < num_patterns; p++) {
                std::cout << ((target_tt >> p) & 1);
            }
            std::cout << "\n";
        }
        
        std::cout << "Available divisors (" << window.divisors.size() << " total):\n";
        for (size_t i = 0; i < std::min((size_t)6, window.divisors.size()); i++) {
            std::cout << "  Divisor " << i << " (node " << window.divisors[i] << "): ";
            if (!problem.divisor_truths[i].empty()) {
                uint64_t div_tt = problem.divisor_truths[i][0];
                int num_patterns = std::min(16, 1 << window.inputs.size());
                for (int p = 0; p < num_patterns; p++) {
                    std::cout << ((div_tt >> p) & 1);
                }
            }
            std::cout << "\n";
        }
        debug_count++;
    }
    // For now, assume complete specification (no don't cares)
    problem.target_off.resize(problem.target_on.size());
    for (size_t i = 0; i < problem.target_on.size(); i++) {
        problem.target_off[i] = ~problem.target_on[i];
    }
    
    // Try different divisor combinations
    bool found = false;
    std::vector<int> best_divisors;
    
    // Try single divisors first
    if (debug_count <= 3) {
        std::cout << "Trying single divisor replacements...\n";
    }
    for (size_t i = 0; i < window.divisors.size() && !found; i++) {
        if (check_feasibility_cpu(problem, {static_cast<int>(i)})) {
            best_divisors = {static_cast<int>(i)};
            found = true;
            if (debug_count <= 3) {
                std::cout << "  SUCCESS: Single divisor " << i << " (node " << window.divisors[i] << ") can implement target!\n";
            }
            break;
        }
    }
    
    // Try pairs of divisors
    if (!found) {
        if (debug_count <= 3) {
            std::cout << "No single divisor works, trying pairs...\n";
        }
        int pairs_tried = 0;
        for (size_t i = 0; i < window.divisors.size() && !found; i++) {
            for (size_t j = i + 1; j < window.divisors.size() && !found; j++) {
                pairs_tried++;
                if (check_feasibility_cpu(problem, {static_cast<int>(i), static_cast<int>(j)})) {
                    best_divisors = {static_cast<int>(i), static_cast<int>(j)};
                    found = true;
                    if (debug_count <= 3) {
                        std::cout << "  SUCCESS: Divisor pair (" << i << "," << j << ") = (nodes " 
                                 << window.divisors[i] << "," << window.divisors[j] << ") after trying " 
                                 << pairs_tried << " pairs\n";
                    }
                }
            }
        }
        if (!found && debug_count <= 3) {
            std::cout << "  No pairs work after trying " << pairs_tried << " combinations\n";
        }
    }
    
    // Try three divisors
    if (!found && window.divisors.size() >= 3) {
        for (size_t i = 0; i < window.divisors.size() - 2; i++) {
            for (size_t j = i + 1; j < window.divisors.size() - 1; j++) {
                for (size_t k = j + 1; k < window.divisors.size(); k++) {
                    if (check_feasibility_cpu(problem, {static_cast<int>(i), 
                                                        static_cast<int>(j), 
                                                        static_cast<int>(k)})) {
                        best_divisors = {static_cast<int>(i), static_cast<int>(j), 
                                        static_cast<int>(k)};
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (found) break;
        }
    }
    
    if (found) {
        result.success = true;
        
        // Convert divisor indices to actual node IDs
        result.divisor.divisor_ids.clear();
        for (int idx : best_divisors) {
            result.divisor.divisor_ids.push_back(window.divisors[idx]);
        }
        
        result.divisor.function = problem.target_on;  // Simplified
        result.divisor.estimated_gain = 1;  // Simplified gain calculation
        result.actual_gain = 1;
        
        // AVOID RACE CONDITION: Don't modify AIG during parallel execution
        // Store replacement information but don't apply it yet
        // Actual AIG modifications will be done serially after conflict resolution
    }
    
    return result;
}

std::vector<ResubResult> ResubEngine::resubstitute_batch(const std::vector<Window>& windows) {
    std::vector<ResubResult> results;
    
#ifdef USE_CUDA
    if (use_gpu && windows.size() > 10) {  // Use GPU for larger batches
        // Prepare GPU problems
        std::vector<ResubProblem> problems;
        prepare_gpu_problems(windows, problems);
        
        // Convert to GPU format
        std::vector<cuda::GPUResubProblem> gpu_problems(problems.size());
        std::vector<cuda::GPUResubResult> gpu_results(problems.size());
        
        for (size_t i = 0; i < problems.size(); i++) {
            auto& gp = gpu_problems[i];
            auto& p = problems[i];
            
            gp.window_id = i;
            gp.target_node = p.target_node;
            gp.num_divisors = std::min(static_cast<int>(p.divisor_truths.size()), 
                                       cuda::MAX_DIVISORS);
            gp.num_words = std::min(static_cast<int>(p.target_on.size()), 
                                    cuda::MAX_TRUTH_WORDS);
            
            // Copy truth tables
            for (int j = 0; j < gp.num_divisors; j++) {
                for (int k = 0; k < gp.num_words; k++) {
                    gp.divisor_truths[j * cuda::MAX_TRUTH_WORDS + k] = 
                        p.divisor_truths[j][k];
                }
            }
            
            for (int k = 0; k < gp.num_words; k++) {
                gp.target_on[k] = p.target_on[k];
                gp.target_off[k] = p.target_off[k];
            }
        }
        
        // Process on GPU
        cuda::GPUResubEngine gpu_engine(gpu_problems.size());
        gpu_engine.process_batch(gpu_problems.data(), gpu_results.data(), 
                                 gpu_problems.size());
        
        // Convert results back
        for (size_t i = 0; i < gpu_results.size(); i++) {
            ResubResult result;
            result.target_node = windows[i].target_node;
            result.success = gpu_results[i].success;
            result.actual_gain = gpu_results[i].gain;
            
            if (result.success) {
                // Extract divisor indices from mask
                uint32_t mask = gpu_results[i].divisor_mask;
                for (int j = 0; j < 32; j++) {
                    if (mask & (1U << j)) {
                        result.divisor.divisor_ids.push_back(j);
                    }
                }
                result.divisor.estimated_gain = result.actual_gain;
            }
            
            results.push_back(result);
        }
    } else
#endif
    {
        // CPU fallback
        for (const auto& window : windows) {
            results.push_back(resubstitute(window));
        }
    }
    
    return results;
}

bool ResubEngine::check_feasibility(const Window& window, const ResubDivisor& divisor) {
    ResubProblem problem;
    problem.window_id = 0;
    problem.target_node = window.target_node;
    problem.num_vars = window.inputs.size();
    
    // Compute truth tables
    problem.divisor_truths.resize(window.divisors.size());
    for (size_t i = 0; i < window.divisors.size(); i++) {
        compute_truth_table(window.divisors[i], window.inputs, problem.divisor_truths[i]);
    }
    
    compute_truth_table(window.target_node, window.inputs, problem.target_on);
    problem.target_off.resize(problem.target_on.size());
    for (size_t i = 0; i < problem.target_on.size(); i++) {
        problem.target_off[i] = ~problem.target_on[i];
    }
    
    return check_feasibility_cpu(problem, divisor.divisor_ids);
}

int ResubEngine::synthesize_replacement(const Window& window, const ResubDivisor& divisor) {
    // Simple synthesis - create AND/OR of selected divisors
    if (divisor.divisor_ids.empty()) return -1;
    
    if (divisor.divisor_ids.size() == 1) {
        // Single divisor - return it directly (now actual node ID, not index)
        return divisor.divisor_ids[0];
    }
    
    // Multiple divisors - create AND gate (simplified)
    int result = divisor.divisor_ids[0];
    for (size_t i = 1; i < divisor.divisor_ids.size(); i++) {
        int div = divisor.divisor_ids[i];
        result = aig.create_and(AIG::var2lit(result), AIG::var2lit(div));
        result = AIG::lit2var(result);
    }
    
    return result;
}

bool ResubEngine::check_feasibility_cpu(const ResubProblem& problem,
                                        const std::vector<int>& divisor_subset) {
    if (divisor_subset.empty()) return false;
    
    int num_words = problem.target_on.size();
    std::vector<uint64_t> combined(num_words, 0);
    
    if (divisor_subset.size() == 1) {
        // Single divisor
        combined = problem.divisor_truths[divisor_subset[0]];
    } else if (divisor_subset.size() == 2) {
        // Two divisors - try AND and OR
        const auto& div0 = problem.divisor_truths[divisor_subset[0]];
        const auto& div1 = problem.divisor_truths[divisor_subset[1]];
        
        // Try AND
        for (int i = 0; i < num_words; i++) {
            combined[i] = div0[i] & div1[i];
        }
        
        if (is_function_feasible(problem.target_on, problem.target_off, 
                                 problem.divisor_truths, divisor_subset)) {
            return true;
        }
        
        // Try OR
        for (int i = 0; i < num_words; i++) {
            combined[i] = div0[i] | div1[i];
        }
    } else {
        // Multiple divisors - try various combinations
        // Simplified: just try AND of all
        combined = problem.divisor_truths[divisor_subset[0]];
        for (size_t j = 1; j < divisor_subset.size(); j++) {
            const auto& div = problem.divisor_truths[divisor_subset[j]];
            for (int i = 0; i < num_words; i++) {
                combined[i] &= div[i];
            }
        }
    }
    
    // Check if combined function matches target
    for (int i = 0; i < num_words; i++) {
        // Must cover all on-set minterms
        if ((problem.target_on[i] & ~combined[i]) != 0) {
            return false;
        }
        // Must not cover any off-set minterms
        if ((problem.target_off[i] & combined[i]) != 0) {
            return false;
        }
    }
    
    return true;
}

void ResubEngine::prepare_gpu_problems(const std::vector<Window>& windows,
                                       std::vector<ResubProblem>& problems) {
    problems.clear();
    problems.reserve(windows.size());
    
    for (const auto& window : windows) {
        ResubProblem problem;
        problem.window_id = problems.size();
        problem.target_node = window.target_node;
        problem.num_vars = window.inputs.size();
        
        // Compute truth tables
        problem.divisor_truths.resize(window.divisors.size());
        for (size_t i = 0; i < window.divisors.size(); i++) {
            compute_truth_table(window.divisors[i], window.inputs, 
                              problem.divisor_truths[i]);
        }
        
        compute_truth_table(window.target_node, window.inputs, problem.target_on);
        problem.target_off.resize(problem.target_on.size());
        for (size_t i = 0; i < problem.target_on.size(); i++) {
            problem.target_off[i] = ~problem.target_on[i];
        }
        
        problems.push_back(problem);
    }
}

void ResubEngine::compute_truth_table(int node, const std::vector<int>& inputs,
                                      TruthTable& truth) {
    int num_vars = inputs.size();
    if (num_vars > 6) {
        // Limit to 6 variables for bit-parallel simulation (64 patterns max)
        truth.clear();
        return;
    }
    if (num_vars == 0) {
        // Constant function
        truth.clear();
        truth.resize(1);
        // TODO: Determine if node is constant 0 or 1
        truth[0] = (node == 0) ? 0 : ~0ULL;
        return;
    }
    
    int num_patterns = 1 << num_vars;
    int num_words = (num_patterns + 63) / 64;
    
    truth.clear();
    truth.resize(num_words, 0);
    
    // Extract node definitions for stateless simulation
    std::unordered_set<int> all_needed_nodes;
    std::vector<std::pair<int, std::pair<int, int>>> node_definitions;
    
    // Build transitive fanin closure starting from target node
    std::vector<int> worklist = {node};
    all_needed_nodes.insert(node);
    
    while (!worklist.empty()) {
        int current_node = worklist.back();
        worklist.pop_back();
        
        // If it's a PI or constant, stop
        if (current_node <= aig.num_pis) continue;
        if (current_node >= static_cast<int>(aig.nodes.size())) continue;
        if (aig.nodes[current_node].is_dead) continue;
        
        // If it's a window input, stop (we'll set these as patterns)
        if (std::find(inputs.begin(), inputs.end(), current_node) != inputs.end()) continue;
        
        // Add fanins to closure and worklist
        int fanin0 = aig.nodes[current_node].fanin0;
        int fanin1 = aig.nodes[current_node].fanin1;
        int var0 = aig.lit2var(fanin0);
        int var1 = aig.lit2var(fanin1);
        
        if (var0 >= 0 && var0 < aig.num_nodes && all_needed_nodes.insert(var0).second) {
            worklist.push_back(var0);
        }
        
        if (var1 >= 0 && var1 < aig.num_nodes && all_needed_nodes.insert(var1).second) {
            worklist.push_back(var1);
        }
        
        // Add node definition for stateless simulation
        node_definitions.emplace_back(current_node, std::make_pair(fanin0, fanin1));
    }
    
    // Sort node definitions by topological order
    std::sort(node_definitions.begin(), node_definitions.end());
    
    // Use stateless simulation - NO SHARED STATE ACCESS!
    std::vector<int> nodes_to_compute(all_needed_nodes.begin(), all_needed_nodes.end());
    auto sim_results = AIG::simulate_window_stateless(inputs, nodes_to_compute, node_definitions);
    
    // Extract truth table from simulation results
    auto it = sim_results.find(node);
    if (it != sim_results.end()) {
        uint64_t result = it->second;
        
        // Store the result bits in truth table format
        if (num_patterns <= 64) {
            // All patterns fit in one word
            truth[0] = result & ((1ULL << num_patterns) - 1);
        } else {
            // Multiple words (shouldn't happen with num_vars <= 6)
            for (int word = 0; word < num_words; word++) {
                int start_bit = word * 64;
                int end_bit = std::min(start_bit + 64, num_patterns);
                uint64_t mask = (end_bit == start_bit + 64) ? ~0ULL : ((1ULL << (end_bit - start_bit)) - 1);
                truth[word] = (result >> start_bit) & mask;
            }
        }
    }
}

bool ResubEngine::is_function_feasible(const TruthTable& on_set, 
                                       const TruthTable& off_set,
                                       const std::vector<TruthTable>& divisors,
                                       const std::vector<int>& selected) {
    // This is a simplified version
    // In practice, would use more sophisticated Boolean matching
    return true;
}

} // namespace fresub