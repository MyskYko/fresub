# FRESUB - Fast Resubstitution for AIGs

A high-performance C++/CUDA implementation of AIG (And-Inverter Graph) resubstitution with GPU acceleration and parallel conflict resolution.

## Features

- **Window-based resubstitution**: Extracts local windows from the AIG for optimization
- **GPU acceleration**: CUDA kernels for parallel feasibility checking
- **Conflict resolution**: Detects and resolves conflicts between parallel resubstitutions
- **Multi-threaded processing**: Parallel window extraction and processing
- **Exact synthesis**: Synthesizes optimal implementations for small functions
- **AIGER format support**: Reads and writes standard AIGER files

## Architecture

The system consists of several key components:

1. **AIG Data Structure** (`aig.hpp/cpp`): Core AIG representation with basic operations
2. **Window Extraction** (`window.hpp/cpp`): Identifies optimization opportunities
3. **Resubstitution Engine** (`resub.hpp/cpp`): Performs the actual optimization
4. **CUDA Kernels** (`cuda_kernels.cuh`, `resub_kernels.cu`): GPU acceleration
5. **Conflict Resolution** (`conflict.hpp/cpp`): Handles parallel commits
6. **Synthesis** (`synthesis.cpp`): Circuit synthesis for replacements

## Building

### Requirements

- C++17 compatible compiler
- CUDA Toolkit (10.0 or higher)
- CMake 3.18 or higher
- GPU with compute capability 7.0+ (for GPU acceleration)

### Build Instructions

```bash
cd fresub
mkdir build
cd build
cmake ..
make -j8
```

To build without CUDA support:
```bash
cmake -DUSE_CUDA=OFF ..
```

## Usage

```bash
./fresub [options] <input.aig> <output.aig>

Options:
  -w <size>    Window size (default: 6)
  -c <size>    Cut size (default: 8)
  -r <rounds>  Number of rounds (default: 1)
  -t <threads> Number of threads (default: 8)
  -g           Use GPU acceleration
  -v           Verbose output
  -s           Print statistics
  -h           Show help
```

### Example

```bash
# Basic optimization
./fresub circuit.aig optimized.aig

# Multi-round optimization with GPU
./fresub -r 5 -g -v circuit.aig optimized.aig

# Parallel processing with 16 threads
./fresub -t 16 -s circuit.aig optimized.aig
```

## Algorithm Overview

The resubstitution algorithm works as follows:

1. **Window Extraction**: For each node in the AIG, extract a local window containing the node and its transitive fanin up to a certain depth

2. **Divisor Extraction**: Identify potential divisors (existing nodes that can be reused) within each window

3. **Feasibility Checking**: For each subset of divisors, check if they can implement the target function. This is done in parallel on the GPU.

4. **Synthesis**: For feasible divisor sets, synthesize the replacement circuit

5. **Conflict Resolution**: When multiple windows are processed in parallel, detect and resolve conflicts between overlapping modifications

6. **Commit**: Apply non-conflicting optimizations to the AIG

## Performance Considerations

- **GPU Utilization**: Best performance with batch sizes > 100 windows
- **Window Size**: Larger windows find more opportunities but are slower
- **Thread Count**: Set based on CPU cores (typically 2x physical cores)
- **Memory**: GPU memory limits the batch size for parallel processing

## Testing

Run the test suite:
```bash
cd build
make test
# or
./tests/test_fresub
```

## Implementation Notes

- Truth tables are used for functions up to 10 variables
- GPU kernels use warp-level parallelism for efficiency
- Conflict detection uses transitive fanout computation
- The synthesis module supports exact synthesis for small functions

## Future Improvements

- [ ] SAT-based exact synthesis for larger functions
- [ ] More sophisticated conflict resolution strategies
- [ ] Dynamic window size adjustment
- [ ] Support for sequential circuits
- [ ] Integration with ABC synthesis tool
- [ ] More aggressive GPU optimization

## References

Based on techniques from:
- ABC synthesis system (Berkeley)
- Window-based optimization from various papers
- GPU acceleration inspired by existing parallel Boolean matching work

## License

This project is for educational and research purposes.