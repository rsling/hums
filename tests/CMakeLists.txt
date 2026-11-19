# Each test executable is registered with hums_tests so that the fast loop
# builds every test and nothing of the app.
function(hums_add_test_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    add_dependencies(hums_tests ${target})
endfunction()

add_subdirectory(guardrails)
