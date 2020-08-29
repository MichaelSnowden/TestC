include_directories("${PROJECT_SOURCE_DIR}/include")

add_library(test_runner_test test_runner_test.c)
target_link_libraries(test_runner_test test_runner)

add_executable(test test.c)
set_target_properties(test PROPERTIES EXCLUDE_FROM_ALL True)
target_link_libraries(test test_runner_test)