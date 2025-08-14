#pragma once

#include <cuda_runtime.h>
#include <cstdint>

namespace fresub {
namespace cuda {

// CUDA error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d - %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(error)); \
            exit(1); \
        } \
    } while(0)

// Configuration constants
constexpr int BLOCK_SIZE = 256;
constexpr int THREADS_PER_PROBLEM = 32;
constexpr int MAX_DIVISORS = 30;
constexpr int MAX_TRUTH_WORDS = 16;  // For up to 10 variables

// GPU problem structure
struct GPUResubProblem {
    uint32_t window_id;
    uint32_t target_node;
    uint32_t num_divisors;
    uint32_t num_words;
    uint64_t divisor_truths[MAX_DIVISORS * MAX_TRUTH_WORDS];
    uint64_t target_on[MAX_TRUTH_WORDS];
    uint64_t target_off[MAX_TRUTH_WORDS];
};

// GPU result structure
struct GPUResubResult {
    uint32_t window_id;
    uint32_t success;
    uint32_t divisor_mask;  // Bit mask of selected divisors
    int32_t gain;
};

// Kernel functions
__global__ void resub_feasibility_kernel(
    const GPUResubProblem* problems,
    GPUResubResult* results,
    int num_problems
);

__global__ void parallel_resub_kernel(
    const GPUResubProblem* problems,
    GPUResubResult* results,
    int num_problems,
    int max_divisor_size
);

// Device functions for truth table operations
__device__ bool check_implication(
    const uint64_t* on_set,
    const uint64_t* off_set,
    const uint64_t* impl_func,
    int num_words
);

__device__ uint32_t find_best_divisors(
    const uint64_t* divisor_truths,
    const uint64_t* target_on,
    const uint64_t* target_off,
    int num_divisors,
    int num_words,
    int max_size
);

// Host interface functions
class GPUResubEngine {
public:
    GPUResubEngine(int max_batch_size = 1024);
    ~GPUResubEngine();
    
    // Process a batch of resubstitution problems
    void process_batch(
        const GPUResubProblem* h_problems,
        GPUResubResult* h_results,
        int num_problems
    );
    
    // Memory management
    void allocate_device_memory(int batch_size);
    void free_device_memory();
    
private:
    int max_batch_size;
    GPUResubProblem* d_problems;
    GPUResubResult* d_results;
    
    // Stream management for concurrent execution
    cudaStream_t stream;
};

} // namespace cuda
} // namespace fresub