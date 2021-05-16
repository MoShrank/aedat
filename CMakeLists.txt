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

# Add tests if specified
option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    add_subdirectory("tests")
endif()

# stream_dvs data
add_library(stream_dvs SHARED stream_dvs.hpp stream_dvs.cpp convert.cpp)
target_compile_features(stream_dvs PRIVATE cxx_std_14)
target_include_directories(stream_dvs PRIVATE ${TORCH_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${LZ4_INCLUDE_DIR})
target_link_directories(stream_dvs PRIVATE ${LIBCAER_LINK_DIRECTORIES} ${TORCH_LINK_DIRECTORIES} ${CMAKE_BINARY_DIR}/res)
target_link_libraries(stream_dvs PRIVATE ${CAER_LIBRARY} ${TORCH_LIBRARIES} ${SDL2_LIBRARIES} ${Python3_LIBRARIES} ${LZ4_LIBRARY})


add_executable(streamdata streamdata.cpp)
target_include_directories(streamdata PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(streamdata PRIVATE stream_dvs ${TORCH_LIBRARIES} ${CAER_LIBRARY})