#include "cuda_kernels.cuh"
#include <cstdio>
#include <algorithm>

namespace fresub {
namespace cuda {

// Device function to check if a function can be implemented with given divisors
__device__ bool check_implication(
    const uint64_t* on_set,
    const uint64_t* off_set,
    const uint64_t* impl_func,
    int num_words
) {
    // Check that impl_func covers on_set and doesn't intersect off_set
    for (int i = 0; i < num_words; i++) {
        // Must cover all on-set minterms
        if ((on_set[i] & ~impl_func[i]) != 0) {
            return false;
        }
        // Must not cover any off-set minterms
        if ((off_set[i] & impl_func[i]) != 0) {
            return false;
        }
    }
    return true;
}

// Find best subset of divisors that implements the target function
__device__ uint32_t find_best_divisors(
    const uint64_t* divisor_truths,
    const uint64_t* target_on,
    const uint64_t* target_off,
    int num_divisors,
    int num_words,
    int max_size
) {
    uint32_t best_mask = 0;
    
    // Try all combinations up to max_size divisors
    // This is simplified - in practice we'd use more sophisticated search
    for (int size = 1; size <= max_size && size <= 4; size++) {
        // Generate all k-combinations
        if (size == 1) {
            for (int i = 0; i < num_divisors; i++) {
                if (check_implication(target_on, target_off, 
                                     &divisor_truths[i * num_words], num_words)) {
                    return (1U << i);
                }
            }
        } else if (size == 2) {
            for (int i = 0; i < num_divisors; i++) {
                for (int j = i + 1; j < num_divisors; j++) {
                    // Compute AND of two divisors
                    uint64_t combined[MAX_TRUTH_WORDS];
                    for (int w = 0; w < num_words; w++) {
                        combined[w] = divisor_truths[i * num_words + w] & 
                                     divisor_truths[j * num_words + w];
                    }
                    if (check_implication(target_on, target_off, combined, num_words)) {
                        return (1U << i) | (1U << j);
                    }
                    
                    // Try OR as well
                    for (int w = 0; w < num_words; w++) {
                        combined[w] = divisor_truths[i * num_words + w] | 
                                     divisor_truths[j * num_words + w];
                    }
                    if (check_implication(target_on, target_off, combined, num_words)) {
                        return (1U << i) | (1U << j);
                    }
                }
            }
        } else if (size == 3) {
            for (int i = 0; i < num_divisors - 2; i++) {
                for (int j = i + 1; j < num_divisors - 1; j++) {
                    for (int k = j + 1; k < num_divisors; k++) {
                        // Try various 3-input functions
                        uint64_t combined[MAX_TRUTH_WORDS];
                        
                        // Example: (i & j) | k
                        for (int w = 0; w < num_words; w++) {
                            combined[w] = (divisor_truths[i * num_words + w] & 
                                          divisor_truths[j * num_words + w]) |
                                         divisor_truths[k * num_words + w];
                        }
                        if (check_implication(target_on, target_off, combined, num_words)) {
                            return (1U << i) | (1U << j) | (1U << k);
                        }
                        
                        // Example: i & j & k
                        for (int w = 0; w < num_words; w++) {
                            combined[w] = divisor_truths[i * num_words + w] & 
                                         divisor_truths[j * num_words + w] &
                                         divisor_truths[k * num_words + w];
                        }
                        if (check_implication(target_on, target_off, combined, num_words)) {
                            return (1U << i) | (1U << j) | (1U << k);
                        }
                    }
                }
            }
        } else if (size == 4) {
            // Similar for 4 divisors, but more complex
            // Simplified version here
            for (int i = 0; i < num_divisors - 3; i++) {
                for (int j = i + 1; j < num_divisors - 2; j++) {
                    for (int k = j + 1; k < num_divisors - 1; k++) {
                        for (int l = k + 1; l < num_divisors; l++) {
                            uint64_t combined[MAX_TRUTH_WORDS];
                            
                            // Try: ((i & j) | (k & l))
                            for (int w = 0; w < num_words; w++) {
                                combined[w] = (divisor_truths[i * num_words + w] & 
                                              divisor_truths[j * num_words + w]) |
                                             (divisor_truths[k * num_words + w] &
                                              divisor_truths[l * num_words + w]);
                            }
                            if (check_implication(target_on, target_off, combined, num_words)) {
                                return (1U << i) | (1U << j) | (1U << k) | (1U << l);
                            }
                        }
                    }
                }
            }
        }
    }
    
    return best_mask;
}

// Main kernel for resubstitution feasibility checking
__global__ void resub_feasibility_kernel(
    const GPUResubProblem* problems,
    GPUResubResult* results,
    int num_problems
) {
    int global_tid = blockIdx.x * blockDim.x + threadIdx.x;
    int problem_id = global_tid / THREADS_PER_PROBLEM;
    int thread_in_problem = global_tid % THREADS_PER_PROBLEM;
    
    if (problem_id >= num_problems) return;
    
    const GPUResubProblem& problem = problems[problem_id];
    
    // Shared memory for reduction
    __shared__ uint32_t shared_masks[BLOCK_SIZE];
    __shared__ int32_t shared_gains[BLOCK_SIZE];
    
    uint32_t local_mask = 0;
    int32_t local_gain = -1;
    
    // Each thread checks different divisor combinations
    int start_idx = thread_in_problem;
    int stride = THREADS_PER_PROBLEM;
    
    // Simple strategy: each thread checks different starting points
    for (int i = start_idx; i < problem.num_divisors && i < MAX_DIVISORS; i += stride) {
        // Try single divisor first
        if (check_implication(problem.target_on, problem.target_off,
                             &problem.divisor_truths[i * problem.num_words],
                             problem.num_words)) {
            local_mask = (1U << i);
            local_gain = 1;  // Simplified gain calculation
            break;
        }
        
        // Try pairs starting from i
        for (int j = i + 1; j < problem.num_divisors && j < MAX_DIVISORS; j++) {
            uint64_t combined[MAX_TRUTH_WORDS];
            
            // AND combination
            for (int w = 0; w < problem.num_words; w++) {
                combined[w] = problem.divisor_truths[i * problem.num_words + w] &
                             problem.divisor_truths[j * problem.num_words + w];
            }
            
            if (check_implication(problem.target_on, problem.target_off,
                                combined, problem.num_words)) {
                local_mask = (1U << i) | (1U << j);
                local_gain = 1;
                break;
            }
            
            // OR combination
            for (int w = 0; w < problem.num_words; w++) {
                combined[w] = problem.divisor_truths[i * problem.num_words + w] |
                             problem.divisor_truths[j * problem.num_words + w];
            }
            
            if (check_implication(problem.target_on, problem.target_off,
                                combined, problem.num_words)) {
                local_mask = (1U << i) | (1U << j);
                local_gain = 1;
                break;
            }
        }
        
        if (local_mask != 0) break;
    }
    
    // Store in shared memory
    int shared_idx = threadIdx.x;
    shared_masks[shared_idx] = local_mask;
    shared_gains[shared_idx] = local_gain;
    __syncthreads();
    
    // Reduction within threads working on same problem
    if (thread_in_problem == 0) {
        uint32_t final_mask = 0;
        int32_t final_gain = -1;
        
        for (int t = 0; t < THREADS_PER_PROBLEM && 
             (threadIdx.x / THREADS_PER_PROBLEM) * THREADS_PER_PROBLEM + t < blockDim.x; t++) {
            int idx = (threadIdx.x / THREADS_PER_PROBLEM) * THREADS_PER_PROBLEM + t;
            if (shared_masks[idx] != 0 && 
                (final_mask == 0 || __popc(shared_masks[idx]) < __popc(final_mask))) {
                final_mask = shared_masks[idx];
                final_gain = shared_gains[idx];
            }
        }
        
        results[problem_id].window_id = problem.window_id;
        results[problem_id].success = (final_mask != 0) ? 1 : 0;
        results[problem_id].divisor_mask = final_mask;
        results[problem_id].gain = final_gain;
    }
}

// Extended kernel for more complex resubstitution
__global__ void parallel_resub_kernel(
    const GPUResubProblem* problems,
    GPUResubResult* results,
    int num_problems,
    int max_divisor_size
) {
    int problem_id = blockIdx.x;
    if (problem_id >= num_problems) return;
    
    const GPUResubProblem& problem = problems[problem_id];
    int tid = threadIdx.x;
    
    // Use shared memory for divisor truth tables
    extern __shared__ uint64_t shared_truths[];
    
    // Cooperatively load divisor truth tables into shared memory
    int truths_per_thread = (problem.num_divisors * problem.num_words + blockDim.x - 1) / blockDim.x;
    for (int i = 0; i < truths_per_thread; i++) {
        int idx = tid * truths_per_thread + i;
        if (idx < problem.num_divisors * problem.num_words) {
            shared_truths[idx] = problem.divisor_truths[idx];
        }
    }
    __syncthreads();
    
    // Each thread explores different combinations
    uint32_t best_mask = find_best_divisors(
        shared_truths,
        problem.target_on,
        problem.target_off,
        problem.num_divisors,
        problem.num_words,
        max_divisor_size
    );
    
    // Reduction to find best result
    __shared__ uint32_t best_masks[256];
    best_masks[tid] = best_mask;
    __syncthreads();
    
    // Tree reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            uint32_t mask1 = best_masks[tid];
            uint32_t mask2 = best_masks[tid + stride];
            
            // Choose better mask (fewer divisors)
            if (mask2 != 0 && (mask1 == 0 || __popc(mask2) < __popc(mask1))) {
                best_masks[tid] = mask2;
            }
        }
        __syncthreads();
    }
    
    // Thread 0 writes the result
    if (tid == 0) {
        results[problem_id].window_id = problem.window_id;
        results[problem_id].success = (best_masks[0] != 0) ? 1 : 0;
        results[problem_id].divisor_mask = best_masks[0];
        results[problem_id].gain = best_masks[0] ? __popc(best_masks[0]) : -1;
    }
}

// GPUResubEngine implementation
GPUResubEngine::GPUResubEngine(int max_batch_size) 
    : max_batch_size(max_batch_size), d_problems(nullptr), d_results(nullptr) {
    CUDA_CHECK(cudaStreamCreate(&stream));
    allocate_device_memory(max_batch_size);
}

GPUResubEngine::~GPUResubEngine() {
    free_device_memory();
    cudaStreamDestroy(stream);
}

void GPUResubEngine::allocate_device_memory(int batch_size) {
    if (d_problems != nullptr) {
        free_device_memory();
    }
    
    CUDA_CHECK(cudaMalloc(&d_problems, batch_size * sizeof(GPUResubProblem)));
    CUDA_CHECK(cudaMalloc(&d_results, batch_size * sizeof(GPUResubResult)));
}

void GPUResubEngine::free_device_memory() {
    if (d_problems != nullptr) {
        cudaFree(d_problems);
        d_problems = nullptr;
    }
    if (d_results != nullptr) {
        cudaFree(d_results);
        d_results = nullptr;
    }
}

void GPUResubEngine::process_batch(
    const GPUResubProblem* h_problems,
    GPUResubResult* h_results,
    int num_problems
) {
    if (num_problems > max_batch_size) {
        // Process in chunks
        int offset = 0;
        while (offset < num_problems) {
            int chunk_size = std::min(max_batch_size, num_problems - offset);
            process_batch(h_problems + offset, h_results + offset, chunk_size);
            offset += chunk_size;
        }
        return;
    }
    
    // Copy problems to device
    CUDA_CHECK(cudaMemcpyAsync(d_problems, h_problems, 
                               num_problems * sizeof(GPUResubProblem),
                               cudaMemcpyHostToDevice, stream));
    
    // Launch kernel
    int total_threads = num_problems * THREADS_PER_PROBLEM;
    int num_blocks = (total_threads + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    resub_feasibility_kernel<<<num_blocks, BLOCK_SIZE, 0, stream>>>(
        d_problems, d_results, num_problems
    );
    
    // Copy results back
    CUDA_CHECK(cudaMemcpyAsync(h_results, d_results,
                               num_problems * sizeof(GPUResubResult),
                               cudaMemcpyDeviceToHost, stream));
    
    // Synchronize
    CUDA_CHECK(cudaStreamSynchronize(stream));
}

} // namespace cuda
} // namespace fresub