cmake_minimum_required(VERSION 3.18)

# Test executable - only include working tests for now
add_executable(test_fresub
    test_aig.cpp
    test_aiger.cpp
    test_window.cpp
    test_resub.cpp
    test_window_detailed.cpp
    test_simulation_detailed.cpp
    test_feasibility_detailed.cpp
    test_aig_modification.cpp
    test_aiger_comprehensive.cpp
)

# Link with main library
target_link_libraries(test_fresub
    fresub_cpu
    fresub_cuda
    Threads::Threads
)

# Add test
add_test(NAME fresub_tests COMMAND test_fresub)

# Simple test executable with single main
add_executable(test_simple test_main_simple.cpp)
target_link_libraries(test_simple
    fresub_cpu
    fresub_cuda
    Threads::Threads
)