cmake_minimum_required(VERSION 3.16)
project(macos_gconv VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Avoid Mono's legacy PNG headers on machines that have both Mono and Homebrew.
if(APPLE AND EXISTS "/opt/homebrew/opt/libpng/include/png.h")
  set(PNG_PNG_INCLUDE_DIR "/opt/homebrew/opt/libpng/include" CACHE PATH "PNG include directory")
  set(PNG_LIBRARY_RELEASE "/opt/homebrew/opt/libpng/lib/libpng16.a" CACHE FILEPATH "PNG library")
endif()
find_package(PNG 1.6 REQUIRED)
add_executable(gconv src/main.cpp src/common.cpp src/resources.cpp src/images.cpp src/fonts.cpp src/textures.cpp src/qsc.cpp src/qsc/qsc_lexer.cpp src/qsc/qsc_parser.cpp src/qsc/qvm_compiler.cpp)
target_link_libraries(gconv PRIVATE PNG::PNG)
target_compile_options(gconv PRIVATE -Wall -Wextra -Wpedantic)
install(TARGETS gconv RUNTIME DESTINATION bin)

option(GCONV_ENABLE_SANITIZERS "Enable address and undefined-behavior checks" OFF)
if(GCONV_ENABLE_SANITIZERS)
  target_compile_options(gconv PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(gconv PRIVATE -fsanitize=address,undefined)
endif()
include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME gconv_cli COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/verification/check_cli.py
    --binary $<TARGET_FILE:gconv>
    --report ${CMAKE_CURRENT_BINARY_DIR}/cli-results.json)
endif()
