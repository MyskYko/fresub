# Fresub Test Suite

This directory contains the organized test suite for the Fresub project, cleaned up and categorized for better maintainability.

## Directory Structure

### `core/` - Core Functionality Tests
These tests focus on essential fresub functionality and should build and work correctly:

- **`test_aig.cpp`** - Basic AIG data structure operations and validation
- **`test_window.cpp`** - Window extraction algorithms for resubstitution
- **`test_feasibility_check.cpp`** - Core feasibility checking for resubstitution candidates
- **`test_cuts.cpp`** - Cut enumeration and management
- **`test_complete_resubstitution.cpp`** - Full resubstitution pipeline (working)
- **`test_main_simple.cpp`** - Simple integration test for basic AIG processing (working)
- **`test_synthesis_integration.cpp`** - Integration with synthesis tools (ExOpt)
- **`test_aiger.cpp`** - AIGER file format reading/writing
- **`test_aiger_comprehensive.cpp`** - Comprehensive AIGER format testing
- **`test_resub.cpp`** - Basic resubstitution functionality
- **`test_resub_simple.cpp`** - Simplified resubstitution testing
- **`test_simple_insertion.cpp`** - Basic node insertion operations
- **`test_single_insertion.cpp`** - Single node insertion testing
- **`test_window_proper.cpp`** - Proper window extraction validation
- **`test_aig_modification.cpp`** - AIG modification operations

### `integration/` - Integration Tests
Complex full pipeline tests that combine multiple components:

- **`test_main.cpp`** - Main test framework runner with comprehensive test suite
- **`test_complete_verification.cpp`** - End-to-end verification of resubstitution results
- **`test_full_synthesis.cpp`** - Complete synthesis pipeline integration
- **`test_real_conversion.cpp`** - Real-world AIG conversion scenarios
- **`test_main_window.cpp`** - Window-based processing integration
- **`test_synthesis_conversion.cpp`** - Synthesis tool conversion testing
- **`test_two_stage.cpp`** - Two-stage resubstitution pipeline

### `debug/` - Debug and Development Tests
These tests are for debugging and development purposes. They may be broken but are kept for reference:

- **`test_debug_insertion.cpp`** - Debugging node insertion with crash handlers
- **`test_exopt_debug.cpp`** - ExOpt synthesis debugging
- **`test_truth_table_debug.cpp`** - Truth table computation debugging
- **`test_parallel_debug.cpp`** - Parallel processing debugging
- **`test_mffc_debug.cpp`** - Maximum Fanout-Free Cone debugging
- **`test_parallel_stress.cpp`** - Stress testing for parallel operations
- **`test_*_stepby*.cpp`** - Step-by-step debugging variants
- **`test_*_detailed*.cpp`** - Detailed analysis and debugging tests
- **`test_*_compare*.cpp`** - Comparison and validation tests
- **`test_*_opportunities*.cpp`** - Resubstitution opportunity analysis
- **`test_*_analysis.cpp`** - Various analysis and profiling tests

## Build Configuration

### Core Tests (Default Build)
The CMakeLists.txt is configured to build core tests by default:
```bash
cd build
make test_aig test_window test_feasibility_check # etc.
```

### Integration Tests
Integration tests are commented out by default. To enable them, uncomment the integration test section in CMakeLists.txt.

### Debug Tests
Debug tests are intentionally not built by default as they may contain broken code. Add them manually to CMakeLists.txt if needed for debugging purposes.

## Test Categories by Functionality

### AIG Operations
- `core/test_aig.cpp` - Basic operations
- `core/test_aig_modification.cpp` - Modification operations
- `core/test_aiger*.cpp` - File I/O operations

### Window Processing
- `core/test_window.cpp` - Basic window extraction
- `core/test_window_proper.cpp` - Proper window validation
- `debug/test_window_*.cpp` - Detailed window debugging

### Resubstitution
- `core/test_resub*.cpp` - Core resubstitution functionality
- `core/test_complete_resubstitution.cpp` - Full pipeline
- `debug/test_resub_*.cpp` - Detailed debugging

### Synthesis Integration
- `core/test_synthesis_integration.cpp` - Basic integration
- `integration/test_full_synthesis.cpp` - Complete pipeline
- `debug/test_exopt_debug.cpp` - Synthesis debugging

### Node Insertion
- `core/test_simple_insertion.cpp` - Basic insertion
- `core/test_single_insertion.cpp` - Single node operations
- `debug/test_debug_insertion.cpp` - Insertion debugging

## Usage Guidelines

1. **Start with Core Tests**: Always begin with core functionality tests to ensure basic operations work.

2. **Validate with Integration Tests**: Once core tests pass, run integration tests to verify complete pipelines.

3. **Use Debug Tests for Development**: When debugging issues, refer to debug tests for detailed analysis and step-by-step processing.

4. **Test Input Files**: Use `output_simple.aig` or provide your own AIG files for testing.

## Known Status

- **Working Tests**: `test_complete_resubstitution`, `test_main_simple`
- **Core Tests**: Should build and run (may need minor fixes)
- **Integration Tests**: May require more complex setup and dependencies
- **Debug Tests**: Likely broken, kept for reference and development

## Cleanup Actions Performed

1. Removed all binary executables and build artifacts
2. Organized tests into logical categories
3. Updated build system to focus on working tests
4. Preserved debugging code for future reference
5. Eliminated duplicate and obsolete test files

For issues or questions about specific tests, refer to the source code comments and the main Fresub documentation.