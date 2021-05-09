cmake_minimum_required(VERSION 3.7)
project(aedat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(Torch REQUIRED)

# for linking against python
find_library(TORCH_PYTHON_LIBRARY torch_python PATHS "${TORCH_INSTALL_PREFIX}/lib")
list(APPEND TORCH_LIBRARIES ${TORCH_PYTHON_LIBRARY})

find_path(LZ4_INCLUDE_DIR NAMES	lz4.h)
find_library(LZ4_LIBRARY NAMES lz4)
find_package (Python3 REQUIRED COMPONENTS Development)

# Add subdirectories
add_subdirectory("src")
