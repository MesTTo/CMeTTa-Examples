# Purpose: discover installed CMeTTa through CMake's imported pkg-config target.
# Guarantees: CTest runs the asserted consumer [tested: make check-consumers; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
cmake_minimum_required(VERSION 3.18)
project(cmetta_consumer C)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CMETTA REQUIRED IMPORTED_TARGET cmetta)
add_executable(consumer ../integration/installed_consumer.c ../common.c ../lane.c)
target_include_directories(consumer PRIVATE ..)
target_compile_features(consumer PRIVATE c_std_11)
target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(consumer PRIVATE PkgConfig::CMETTA)
enable_testing()
add_test(NAME installed_answer COMMAND consumer)
