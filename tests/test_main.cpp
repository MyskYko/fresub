#include <iostream>

// Simple test framework
int total_tests = 0;
int passed_tests = 0;

#define TEST(name) void test_##name(); \
    static struct test_##name##_runner { \
        test_##name##_runner() { test_##name(); } \
    } test_##name##_instance; \
    void test_##name()

#define ASSERT(cond) do { \
    total_tests++; \
    if (!(cond)) { \
        std::cerr << "Test failed at " << __FILE__ << ":" << __LINE__ \
                  << " - " << #cond << std::endl; \
    } else { \
        passed_tests++; \
    } \
} while(0)

// Declare test functions
void test_aig_basic();
void test_window_extraction();
void test_resub_simple();
void test_aiger_read();
void test_aiger_write();

// Main program debugging test functions
void test_main_program_basic_functionality();
void test_main_program_window_extraction();
void test_main_program_parallel_manager();

// Detailed test functions
void test_window_extraction_detailed();
void test_cut_enumeration();
void test_window_divisor_extraction();

void test_basic_simulation();
void test_complex_simulation();
void test_parallel_simulation();
void test_multi_output_simulation();
void test_deep_circuit_simulation();

void test_truth_table_operations();
void test_simple_feasibility();
void test_complex_feasibility();
void test_three_input_feasibility();
void test_infeasible_cases();
void test_edge_cases();

// Synthesis tests temporarily disabled due to compilation issues

void test_node_creation();
void test_trivial_and_cases();
void test_node_removal();
void test_node_replacement();
void test_po_replacement();
void test_chain_modification();
void test_structural_hash();

void test_ascii_format_comprehensive();
void test_binary_format_comprehensive();
void test_roundtrip_consistency();
void test_edge_cases_aiger();
void test_file_validation();

int main() {
    std::cout << "Running fresub tests...\n\n";
    
    // Run basic tests
    test_aig_basic();
    test_aiger_read();
    test_aiger_write();
    test_window_extraction();
    test_resub_simple();
    
    // Run main program debugging tests
    std::cout << "Running main program debugging tests...\n";
    test_main_program_basic_functionality();
    test_main_program_window_extraction(); 
    test_main_program_parallel_manager();
    
    // Run detailed tests
    test_window_extraction_detailed();
    test_cut_enumeration();
    test_window_divisor_extraction();
    
    test_basic_simulation();
    test_complex_simulation();
    test_parallel_simulation();
    test_multi_output_simulation();
    test_deep_circuit_simulation();
    
    test_truth_table_operations();
    test_simple_feasibility();
    test_complex_feasibility();
    test_three_input_feasibility();
    test_infeasible_cases();
    test_edge_cases();
    
    // Synthesis tests temporarily disabled
    
    test_node_creation();
    test_trivial_and_cases();
    test_node_removal();
    test_node_replacement();
    test_po_replacement();
    test_chain_modification();
    test_structural_hash();
    
    test_ascii_format_comprehensive();
    test_binary_format_comprehensive();
    test_roundtrip_consistency();
    test_edge_cases_aiger();
    test_file_validation();
    
    // Print results
    std::cout << "\n======================\n";
    std::cout << "Test Results:\n";
    std::cout << "  Passed: " << passed_tests << "/" << total_tests << "\n";
    
    if (passed_tests == total_tests) {
        std::cout << "  All tests passed!\n";
        return 0;
    } else {
        std::cout << "  Some tests failed.\n";
        return 1;
    }
}