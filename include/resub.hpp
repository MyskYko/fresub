#pragma once

#include <vector>
#include <cstdint>
#include "fresub_aig.hpp"
#include "window.hpp"

namespace fresub {

// Truth table representation for small functions
using TruthTable = std::vector<uint64_t>;

struct ResubDivisor {
    std::vector<int> divisor_ids;  // Indices of divisors used
    TruthTable function;           // Boolean function to implement
    int estimated_gain;            // Estimated area/delay gain
};

struct ResubResult {
    int target_node;
    ResubDivisor divisor;
    int actual_gain;
    bool success;
};

// Resubstitution problem for GPU processing
struct ResubProblem {
    int window_id;
    int target_node;
    std::vector<TruthTable> divisor_truths;  // Truth tables of divisors
    TruthTable target_on;   // On-set of target function
    TruthTable target_off;  // Off-set of target function
    TruthTable target_dc;   // Don't-care set
    int num_vars;           // Number of variables
};

class ResubEngine {
public:
    ResubEngine(AIG& aig, bool use_gpu = true);
    
    // Perform resubstitution on a single window
    ResubResult resubstitute(const Window& window);
    
    // Batch resubstitution for GPU processing
    std::vector<ResubResult> resubstitute_batch(const std::vector<Window>& windows);
    
    // Check if a resubstitution is feasible
    bool check_feasibility(const Window& window, const ResubDivisor& divisor);
    
    // Synthesize the resubstituted circuit
    int synthesize_replacement(const Window& window, const ResubDivisor& divisor);
    
    // Truth table operations (public for testing)
    void compute_truth_table(int node, const std::vector<int>& inputs, 
                            TruthTable& truth);

private:
    AIG& aig;
    bool use_gpu;
    
    // CPU-based feasibility checking
    bool check_feasibility_cpu(const ResubProblem& problem, 
                               const std::vector<int>& divisor_subset);
    
    // Prepare problems for GPU processing
    void prepare_gpu_problems(const std::vector<Window>& windows,
                              std::vector<ResubProblem>& problems);
    
    bool is_function_feasible(const TruthTable& on_set, const TruthTable& off_set,
                              const std::vector<TruthTable>& divisors,
                              const std::vector<int>& selected);
};

// GPU kernel interface
#ifdef USE_CUDA
extern "C" {
    void launch_resub_kernel(const ResubProblem* problems, 
                            ResubResult* results,
                            int num_problems);
}
#endif

} // namespace fresub