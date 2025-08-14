cmake_minimum_required(VERSION 3.18)

# Core functionality tests - essential functionality that should work
set(CORE_TESTS
    core/test_aig
    core/test_aig_modification
    core/test_aiger
    core/test_aiger_comprehensive
    core/test_complete_resubstitution
    core/test_conflict_resolution
    core/test_cuts
    core/test_feasibility_check
    core/test_main_simple
    core/test_resub
    core/test_resub_simple
    core/test_simple_insertion
    core/test_single_insertion
    core/test_synthesis_integration
    core/test_window
    core/test_window_proper
)

# Integration tests - complex full pipeline tests
set(INTEGRATION_TESTS
    integration/test_complete_verification
    integration/test_full_synthesis
    integration/test_real_conversion
    integration/test_main_window
    integration/test_synthesis_conversion
    integration/test_two_stage
)


# Add core tests as separate executables (these should build and work)
foreach(test_name IN LISTS CORE_TESTS)
    get_filename_component(test_basename ${test_name} NAME)
    add_executable(${test_basename} ${test_name}.cpp)
    target_include_directories(${test_basename} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${EXOPT_INCLUDE_DIR}
        ${EXOPT_AIG_INCLUDE_DIR}
        ${EXOPT_KISSAT_INCLUDE_DIR}
        ${EXOPT_CADICAL_INCLUDE_DIR}
    )
    target_link_libraries(${test_basename} 
        fresub_cpu 
        Threads::Threads
    )
endforeach()

# Add integration tests
foreach(test_name IN LISTS INTEGRATION_TESTS)
    get_filename_component(test_basename ${test_name} NAME)
    add_executable(${test_basename} ${test_name}.cpp)
    target_include_directories(${test_basename} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${EXOPT_INCLUDE_DIR}
        ${EXOPT_AIG_INCLUDE_DIR}
        ${EXOPT_KISSAT_INCLUDE_DIR}
        ${EXOPT_CADICAL_INCLUDE_DIR}
    )
    target_link_libraries(${test_basename} 
        fresub_cpu 
        Threads::Threads
    )
endforeach()

# All debug tests with main() functions
set(DEBUG_TESTS_ALL
    debug/test_complex_windows
    debug/test_cuts_stepby
    debug/test_debug_insertion
    debug/test_exopt_debug
    debug/test_feasibility_detailed
    debug/test_feasibility_verification
    debug/test_isolated_conversion
    debug/test_mffc_debug
    debug/test_mffc_node10
    debug/test_parallel_debug
    debug/test_parallel_stress
    debug/test_resub_detection_status
    debug/test_resub_opportunities
    debug/test_simulation_detailed
    debug/test_synthesis_detailed
    debug/test_tfo_analysis
    debug/test_truth_table_debug
    debug/test_window_compare
    debug/test_window_detailed
    debug/test_window_mffc
    debug/test_window_stepby
    debug/test_window_stepby_v2
)

# Add all debug tests
foreach(test_name IN LISTS DEBUG_TESTS_ALL)
    get_filename_component(test_basename ${test_name} NAME)
    add_executable(${test_basename} ${test_name}.cpp)
    target_include_directories(${test_basename} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${EXOPT_INCLUDE_DIR}
        ${EXOPT_AIG_INCLUDE_DIR}
        ${EXOPT_KISSAT_INCLUDE_DIR}
        ${EXOPT_CADICAL_INCLUDE_DIR}
    )
    target_link_libraries(${test_basename} 
        fresub_cpu 
        Threads::Threads
    )
endforeach()


