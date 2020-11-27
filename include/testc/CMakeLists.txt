add_library(test_runner "${PROJECT_SOURCE_DIR}/src/test_runner.c" test_runner.h)
target_include_directories(test_runner PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_runner PUBLIC test_suite)
target_link_libraries(test_runner PRIVATE utils)
find_package(Threads)
target_link_libraries(test_runner PRIVATE ${CMAKE_THREAD_LIBS_INIT})

add_library(stack_trace "${PROJECT_SOURCE_DIR}/src/stack_trace.c" stack_trace.h)

# Force -rdynamic https://cmake.org/cmake/help/latest/policy/CMP0065.html
# This is necessary for https://stackoverflow.com/a/4674531/2770572
set_target_properties(stack_trace PROPERTIES ENABLE_EXPORTS True)
target_include_directories(stack_trace PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_options(stack_trace PUBLIC -fno-pie)
target_link_options(stack_trace PUBLIC -fno-pie)

add_library(test_suite STATIC "${PROJECT_SOURCE_DIR}/src/test_suite.c" test_suite.h)
target_link_libraries(test_suite PUBLIC stack_trace)
target_include_directories(test_suite PUBLIC "${PROJECT_SOURCE_DIR}/include")

install(TARGETS test_suite test_runner stack_trace DESTINATION lib/testc)
install(FILES test_suite.h test_runner.h stack_trace.h DESTINATION include/testc/testc)