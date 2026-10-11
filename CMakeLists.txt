cmake_minimum_required(VERSION 3.10)
project(RISCV_Simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(code text_simulator.cpp)

# Set compiler flags
target_compile_options(code PRIVATE -O2 -Wall -Wextra)