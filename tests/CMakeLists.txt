cmake_minimum_required(VERSION 3.18)

# Tests during restructuring
set(UNIT_TESTS
    unit/test_window_extraction
    unit/test_simulation
    unit/test_feasibility
    unit/test_synthesis
    unit/test_insertion
)

# Add unit tests (window test only)
foreach(test_name IN LISTS UNIT_TESTS)
    get_filename_component(test_basename ${test_name} NAME)
    add_executable(${test_basename} ${test_name}.cpp)
    target_include_directories(${test_basename} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(${test_basename} 
        fresub_cpu 
        Threads::Threads
    )
endforeach()



