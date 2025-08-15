# Fresub Test Suite

This directory contains the consolidated test suite for the Fresub project, systematically reorganized for maintainability and comprehensive coverage.

## Directory Structure

### `unit/` - Consolidated Unit Tests (Main Test Suite)
These are the primary tests that provide comprehensive coverage of all functionality:

- **`test_aig_operations.cpp`** - AIG data structures, AIGER I/O, file format handling
- **`test_window_extraction.cpp`** - Window extraction, cut enumeration, MFFC computation  
- **`test_simulation.cpp`** - Truth table computation and bit-parallel simulation
- **`test_feasibility.cpp`** - Feasibility checking for 4-input resubstitution candidates
- **`test_synthesis.cpp`** - Exact synthesis integration with ExOpt bridge
- **`test_insertion.cpp`** - AIG insertion, node replacement, circuit integration
- **`test_conflicts.cpp`** - Conflict resolution and sequential window processing
- **`test_complete_pipeline.cpp`** - End-to-end integration testing of entire pipeline


## Build and Usage

### Primary Test Suite
Run the consolidated unit tests for comprehensive validation:

```bash
cd build
cmake ..
make

# Run individual test suites
./tests/test_aig_operations ../benchmarks/mul2.aig
./tests/test_window_extraction ../benchmarks/mul2.aig  
./tests/test_simulation ../benchmarks/mul2.aig
./tests/test_feasibility ../benchmarks/mul2.aig
./tests/test_synthesis ../benchmarks/mul2.aig
./tests/test_insertion ../benchmarks/mul2.aig
./tests/test_conflicts ../benchmarks/mul2.aig
./tests/test_complete_pipeline ../benchmarks/mul2.aig
```


## Test Coverage by Component

### AIG Operations (test_aig_operations.cpp)
- Basic AIG node creation and manipulation
- AIGER file format reading/writing (ASCII and binary)
- AIG structural validation and integrity checking
- Command-line argument processing for benchmark files

### Window Processing (test_window_extraction.cpp)  
- Cut enumeration with size limits and dominance checking
- Window extraction with simultaneous cut propagation
- MFFC (Maximum Fanout-Free Cone) computation
- TFO (Transitive Fanout) analysis and divisor filtering
- Rich visualization and step-by-step analysis output

### Simulation (test_simulation.cpp)
- Truth table computation within extracted windows
- Bit-parallel simulation for performance
- Pattern analysis and function complexity assessment
- Parallel processing validation

### Feasibility (test_feasibility.cpp)
- 4-input resubstitution feasibility checking using gresub algorithm
- Truth table-based analysis for resubstitution opportunities
- **Note**: Known architectural issues documented in TODO_ARCHITECTURE.md

### Synthesis (test_synthesis.cpp)
- Integration with ExOpt exact synthesis engine
- Binary relation and simulation matrix conversion
- Synthesis result validation and error handling
- Gate count optimization analysis

### Insertion (test_insertion.cpp)
- AIG insertion from synthesized circuits
- Node replacement and circuit integration
- Fanout update and structural consistency
- Success rate analysis on real benchmarks

### Conflict Resolution (test_conflicts.cpp)
- Conflict detection between overlapping windows
- Sequential processing to avoid conflicts
- MFFC-based dead node marking
- Parallel simulation stress testing

### Complete Pipeline (test_complete_pipeline.cpp)
- End-to-end workflow validation
- Component integration testing
- Performance characteristic validation
- Two-stage conversion workflows

## Test Results Summary

All 8 consolidated unit tests pass with comprehensive coverage:
- **test_aig_operations**: 60 assertions passed
- **test_window_extraction**: 358 assertions passed  
- **test_simulation**: 29 assertions passed
- **test_feasibility**: 10 assertions passed (architectural issues noted)
- **test_synthesis**: 13 tests passed
- **test_insertion**: 10 tests passed  
- **test_conflicts**: 13 tests passed
- **test_complete_pipeline**: 25 tests passed

## Consolidation Benefits

1. **Reduced Maintenance**: From 44 fragmented tests to 8 comprehensive suites
2. **Rich Debug Output**: Each test provides detailed step-by-step analysis
3. **Better Coverage**: Systematic testing of all pipeline components
4. **Easier Debugging**: Consolidated output shows complete component behavior
5. **Performance Validation**: Integrated timing and throughput measurements

## Known Issues

- **Feasibility Test Logic**: Fundamental architectural coupling between simulation and feasibility components needs redesign (documented in TODO_ARCHITECTURE.md)
- **Synthesis Segfaults**: Empty synthesis matrices can cause segfaults in SynthMan constructor
- **Benchmark Dependencies**: Tests require ../benchmarks/mul2.aig or similar AIG files

## Migration from Original Tests

The original test structure (44 tests) has been systematically consolidated into 8 comprehensive unit tests:
- **Core functionality tests** (16 files) → Merged into unit/ test suite
- **Integration tests** (6 files) → Merged into test_complete_pipeline.cpp  
- **Debug tests** (22 files) → Functionality integrated into unit tests with rich output
- **Visualization programs** → Integrated into unit tests with comprehensive debugging output

The new consolidated unit tests provide superior debugging capabilities with structured output, comprehensive assertions, and systematic coverage of all functionality.