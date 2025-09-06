#pragma once

#include <cuda_runtime.h>
#include <cstdio>

// Provide a unified CUDA error check macro used across CUDA translation units.
#ifndef CHECK_CUDA_ERROR
#define CHECK_CUDA_ERROR(call)                                                       \
    do {                                                                            \
        cudaError_t error = call;                                                   \
        if (error != cudaSuccess) {                                                 \
            fprintf(stderr, "CUDA error at %s:%d - %s\n", __FILE__, __LINE__,     \
                    cudaGetErrorString(error));                                     \
            exit(1);                                                                \
        }                                                                           \
    } while (0)
#endif
