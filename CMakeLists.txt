cmake_minimum_required(VERSION 3.10)
set(CMAKE_C_STANDARD 99)

project(ctest LANGUAGES C VERSION 1.0.1 DESCRIPTION "A test runner for C")

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(test)

