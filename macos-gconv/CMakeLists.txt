cmake_minimum_required(VERSION 3.16)
project(igi2conv_compat LANGUAGES CXX)

# Keep the old source-directory entry point, using the complete native target.
include(CTest)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/native")
