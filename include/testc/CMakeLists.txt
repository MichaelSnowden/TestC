add_library(test_runner "${PROJECT_SOURCE_DIR}/src/test_runner.c" test_runner.h)
target_include_directories(test_runner PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_runner PUBLIC test_suite)
target_link_libraries(test_runner PRIVATE utils)
find_package(Threads)
target_link_libraries(test_runner PRIVATE ${CMAKE_THREAD_LIBS_INIT})

add_library(test_suite STATIC "${PROJECT_SOURCE_DIR}/src/test_suite.c" test_suite.h)
target_include_directories(test_suite PRIVATE "${PROJECT_SOURCE_DIR}/include")

install(TARGETS test_suite test_runner DESTINATION lib/testc)
install(FILES test_suite.h test_runner.h DESTINATION include/testc/testc)