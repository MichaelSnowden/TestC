add_subdirectory(subdir)

add_executable(stack_trace_test stack_trace_test.c )
target_link_libraries(stack_trace_test foo)