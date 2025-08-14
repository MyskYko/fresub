# Fresub - Complete AIG Resubstitution Pipeline

## Overview

Fresub implements a complete AIG (And-Inverter Graph) resubstitution system for Boolean circuit optimization. The system uses exact synthesis to find optimal replacements that traditional algorithms might miss.

## Complete Resubstitution Pipeline

```
AIG Input → Window Extraction → Feasibility Check → Exact Synthesis → AIG Insertion → Node Replacement → Optimized AIG
```

## File Organization by Pipeline Step

### STEP 1: Window Extraction
- **`src/cpu/window.cpp`** - Extracts optimization windows using cut enumeration
- **`src/cpu/window.hpp`** - Window and Cut data structures

### STEP 2: Feasibility Check
- **`src/cpu/test_complete_resubstitution.cpp`** - Contains `find_feasible_4resub()` and `solve_resub_overlap_cpu()` (gresub algorithm implementation)

### STEP 3: Exact Synthesis
- **`src/cpu/synthesis_bridge.cpp`** - Interface to exopt SAT-based synthesis
- **`src/cpu/synthesis_bridge.hpp`** - Synthesis interface definitions
- **`src/cpu/test_complete_resubstitution.cpp`** - Contains `convert_to_exopt_format()` for preparing boolean relation matrices

### STEP 4: AIG Insertion
- **`src/cpu/aig_insertion.cpp`** - Main insertion logic and coordination
- **`src/cpu/aig_insertion.hpp`** - Insertion interface definitions
- **`src/cpu/aig_converter.cpp`** - Two-stage conversion (exopt aigman → fresub AIG → target insertion)
- **`src/cpu/aig_converter.hpp`** - Conversion interface definitions

### STEP 5: Node Replacement
- **`src/cpu/aig_insertion.cpp`** - Contains `replace_target_with_circuit()` method
- **`src/cpu/fresub_aig.cpp`** - Core AIG operations including `replace_node()`

## Core Files

### Main Pipeline Controller
- **`src/cpu/test_complete_resubstitution.cpp`** - Orchestrates the complete resubstitution pipeline

### Core AIG Infrastructure
- **`src/cpu/fresub_aig.cpp/hpp`** - AIG data structure and fundamental operations

### External Integration
- **`src/cpu/synthesis_bridge.cpp/hpp`** - Interface to exopt synthesis tool
- **`../exopt/`** - External exact synthesis tool integration

## Key Algorithms

### Window Extraction
- **Cut Enumeration**: Finds all possible cuts for each node
- **Cut Propagation**: Determines window boundaries using cut intersection
- **MFFC Computation**: Identifies nodes that can be safely replaced

### Feasibility Analysis (gresub)
- **4-Input Resubstitution**: Checks if target can be replaced using 4 divisors
- **Truth Table Simulation**: Computes node functions within window contexts  
- **Overlap Resolution**: Uses 32 qs arrays to validate resubstitution feasibility

### Exact Synthesis (exopt integration)
- **Boolean Relations**: Encodes target function requirements
- **SAT-Based Synthesis**: Generates minimal circuits satisfying constraints
- **Optimal Circuit Generation**: Produces circuits optimized for gate count

### Two-Stage AIG Conversion
- **Stage 1**: Pure structural conversion from exopt aigman to fresub AIG
- **Stage 2**: Mapping and insertion of converted AIG into target structure

## Build Instructions

```bash
cd build
cmake ..
make test_complete_resubstitution
```

## Usage

```bash
./test_complete_resubstitution <input.aig> [max_cut_size]
```

Example:
```bash
./test_complete_resubstitution ../benchmarks/mul2.aig 4
```

## Pipeline Output

The system provides detailed debug output for each pipeline stage:

- **Window Info**: Target node, window inputs, and all divisors
- **Feasibility Info**: Selected divisors for resubstitution
- **Synthesis Info**: Input mapping and synthesized circuit details
- **Insertion Info**: Sub-AIG structure and PI mapping
- **Results**: Gate count changes (increases/reductions)

## Dependencies

- **exopt**: External exact synthesis tool (integrated via CMake)
- **C++17**: Modern C++ features
- **CUDA**: For GPU-accelerated components (optional)

## Status

✅ **COMPLETE**: All pipeline stages functional
- Window extraction working
- Feasibility check working (gresub algorithm)
- Exact synthesis working (exopt integration) 
- AIG insertion working (two-stage conversion)
- Node replacement working
- Complete pipeline functional

## Legacy/Unused Files

The following files exist but are not part of the main pipeline:
- **`resub.cpp`** - Old resubstitution implementation (before complete pipeline)
- **`synthesis.cpp`** - Old synthesis attempt (before exopt integration)
- Various `test_*` files - Individual component tests during development

## Research Applications

This tool enables research in:
- Logic synthesis optimization
- Boolean circuit minimization
- Hardware design automation
- Verification and equivalence checking
- Advanced resubstitution algorithms

The system can find complex resubstitutions that aren't discoverable by traditional greedy algorithms, making it valuable for both academic research and industrial circuit optimization.