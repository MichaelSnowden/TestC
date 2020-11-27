add_library(assert INTERFACE)
target_include_directories(assert INTERFACE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(assert INTERFACE stack_trace)

add_library(stack_trace "${PROJECT_SOURCE_DIR}/src/stack_trace.c" stack_trace.h)
set_target_properties(stack_trace PROPERTIES ENABLE_EXPORTS True)
target_include_directories(stack_trace PUBLIC "${PROJECT_SOURCE_DIR}/include")

add_library(test_runner "${PROJECT_SOURCE_DIR}/src/test_runner.c" test_runner.h)
target_include_directories(test_runner PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_runner PUBLIC test_suite)
find_package(Threads)
target_link_libraries(test_runner PRIVATE ${CMAKE_THREAD_LIBS_INIT})

# See:
# https://web.archive.org/web/20201113222302/https://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
# We need -fno-pie to prevent making a position-independent executable, which would mean that
# addresses don't map properly to source code lines.
target_compile_options(stack_trace PUBLIC -fno-pie)
target_link_options(stack_trace PUBLIC -fno-pie)

add_library(test_suite STATIC "${PROJECT_SOURCE_DIR}/src/test_suite.c" test_suite.h)
target_link_libraries(test_suite PRIVATE stack_trace)
target_include_directories(test_suite PUBLIC "${PROJECT_SOURCE_DIR}/include")

install(TARGETS test_suite test_runner stack_trace DESTINATION lib/testc)
install(FILES test_suite.h test_runner.h stack_trace.h DESTINATION include/testc/testc)