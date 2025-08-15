#!/bin/bash

# Fresub Comprehensive Test Suite Runner
# Runs all 44 tests across Core, Integration, and Debug categories

set -e  # Exit on any test failure (remove if you want to run all tests regardless)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# Timing
START_TIME=$(date +%s)

# Change to build/tests directory where test executables are
cd "$(dirname "$0")/build/tests" || exit 1

# Function to run a test with proper arguments
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local category="$3"
    
    TOTAL=$((TOTAL + 1))
    
    # Check if test executable exists
    if [ ! -f "$test_name" ]; then
        echo -e "${YELLOW}[SKIP]${NC} $category/$test_name - executable not found"
        SKIPPED=$((SKIPPED + 1))
        return
    fi
    
    # Run test with timeout
    if timeout 60 $test_cmd >/dev/null 2>&1; then
        echo -e "${GREEN}[PASS]${NC} $category/$test_name"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}[FAIL]${NC} $category/$test_name"
        FAILED=$((FAILED + 1))
        # Optionally show error output (uncomment if needed)
        # echo "  Error output:"
        # timeout 5 $test_cmd 2>&1 | head -10 | sed 's/^/    /'
    fi
}

echo "=========================================="
echo "    Fresub Comprehensive Test Suite"
echo "=========================================="
echo

# ====================
# CORE TESTS (16 tests)
# ====================
echo -e "${BLUE}=== CORE TESTS (16) ===${NC}"
echo "Testing essential functionality..."
echo

run_test "test_aig" "./test_aig" "Core"
run_test "test_aig_modification" "./test_aig_modification" "Core"
run_test "test_aiger" "./test_aiger ../../benchmarks/mul2.aig" "Core"
run_test "test_aiger_comprehensive" "./test_aiger_comprehensive ../../benchmarks/mul2.aig" "Core"
run_test "test_complete_resubstitution" "./test_complete_resubstitution ../../benchmarks/mul2.aig 4" "Core"
run_test "test_conflict_resolution" "./test_conflict_resolution ../../benchmarks/mul2.aig" "Core"
run_test "test_cuts" "./test_cuts ../../benchmarks/mul2.aig 4" "Core"
run_test "test_feasibility_check" "./test_feasibility_check ../../benchmarks/mul2.aig" "Core"
run_test "test_main_simple" "./test_main_simple ../../benchmarks/mul2.aig /tmp/output.aig" "Core"
run_test "test_resub" "./test_resub ../../benchmarks/mul2.aig" "Core"
run_test "test_resub_simple" "./test_resub_simple ../../benchmarks/mul2.aig" "Core"
run_test "test_simple_insertion" "./test_simple_insertion ../benchmarks/mul2.aig" "Core"
run_test "test_single_insertion" "./test_single_insertion ../../benchmarks/mul2.aig" "Core"
run_test "test_synthesis_integration" "./test_synthesis_integration ../../benchmarks/mul2.aig 4" "Core"
run_test "test_window" "./test_window ../../benchmarks/mul2.aig" "Core"
run_test "test_window_proper" "./test_window_proper ../../benchmarks/mul2.aig 4" "Core"

echo

# ==========================
# INTEGRATION TESTS (6 tests)
# ==========================
echo -e "${BLUE}=== INTEGRATION TESTS (6) ===${NC}"
echo "Testing complete pipeline integration..."
echo

run_test "test_complete_verification" "./test_complete_verification ../../benchmarks/mul2.aig 4" "Integration"
run_test "test_full_synthesis" "./test_full_synthesis ../../benchmarks/mul2.aig 4" "Integration"
run_test "test_main_window" "./test_main_window ../../benchmarks/mul2.aig" "Integration"
run_test "test_real_conversion" "./test_real_conversion ../../benchmarks/mul2.aig 4" "Integration"
run_test "test_synthesis_conversion" "./test_synthesis_conversion ../../benchmarks/mul2.aig 4" "Integration"
run_test "test_two_stage" "./test_two_stage ../../benchmarks/mul2.aig 4" "Integration"

echo

# ====================
# DEBUG TESTS (22 tests)
# ====================
echo -e "${BLUE}=== DEBUG TESTS (22) ===${NC}"
echo "Testing debug and analysis utilities..."
echo

run_test "test_complex_windows" "./test_complex_windows ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_cuts_stepby" "./test_cuts_stepby ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_debug_insertion" "./test_debug_insertion ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_exopt_debug" "./test_exopt_debug ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_feasibility_detailed" "./test_feasibility_detailed ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_feasibility_verification" "./test_feasibility_verification ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_isolated_conversion" "./test_isolated_conversion" "Debug"
run_test "test_mffc_debug" "./test_mffc_debug ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_mffc_node10" "./test_mffc_node10 ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_parallel_debug" "./test_parallel_debug ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_parallel_stress" "./test_parallel_stress ../../benchmarks/mul2.aig 4 50" "Debug"
run_test "test_resub_detection_status" "./test_resub_detection_status ../../benchmarks/mul2.aig" "Debug"
run_test "test_resub_opportunities" "./test_resub_opportunities ../../benchmarks/mul2.aig" "Debug"
run_test "test_simulation_detailed" "./test_simulation_detailed" "Debug"
run_test "test_synthesis_detailed" "./test_synthesis_detailed" "Debug"
run_test "test_tfo_analysis" "./test_tfo_analysis ../../benchmarks/mul2.aig" "Debug"
run_test "test_truth_table_debug" "./test_truth_table_debug ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_window_compare" "./test_window_compare ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_window_detailed" "./test_window_detailed ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_window_mffc" "./test_window_mffc ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_window_stepby" "./test_window_stepby ../../benchmarks/mul2.aig 4" "Debug"
run_test "test_window_stepby_v2" "./test_window_stepby_v2 ../../benchmarks/mul2.aig 4" "Debug"

echo
echo "=========================================="
echo "           TEST RESULTS SUMMARY"
echo "=========================================="

# Calculate elapsed time
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

# Print summary
echo -e "Total tests:    $TOTAL"
echo -e "${GREEN}Passed:         $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed:         $FAILED${NC}"
else
    echo -e "Failed:         $FAILED"
fi
if [ $SKIPPED -gt 0 ]; then
    echo -e "${YELLOW}Skipped:        $SKIPPED${NC}"
else
    echo -e "Skipped:        $SKIPPED"
fi

# Calculate success rate
if [ $TOTAL -gt 0 ]; then
    SUCCESS_RATE=$((PASSED * 100 / TOTAL))
    echo -e "Success rate:   ${SUCCESS_RATE}%"
fi

echo -e "Time elapsed:   ${ELAPSED} seconds"

echo
echo "=========================================="

# Exit status
if [ $FAILED -eq 0 ] && [ $SKIPPED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED!${NC}"
    exit 0
elif [ $FAILED -eq 0 ]; then
    echo -e "${YELLOW}⚠️  All run tests passed, but some were skipped${NC}"
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    exit 1
fi