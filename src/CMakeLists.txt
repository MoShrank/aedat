# find libcaer 
find_library(CAER_LIBRARY NAMES caer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(viewer viewer.cpp aedat.hpp )

include_directories(viewer ${SDL2_INCLUDE_DIRS} ${LZ4_INCLUDE_DIR} PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(viewer ${SDL2_LIBRARIES} ${LZ4_LIBRARY} ${Python3_LIBRARIES})

# convert 
add_library(convert SHARED convert.cpp)
target_compile_features(convert PRIVATE cxx_std_14)
target_include_directories(convert PRIVATE ${TORCH_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
target_link_directories(convert PRIVATE ${TORCH_LINK_DIRECTORIES})
target_link_libraries(convert PRIVATE ${TORCH_LIBRARIES} ${Python3_LIBRARIES} ${LZ4_LIBRARY})

add_executable(converter converter.cpp)
target_include_directories(converter PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(converter convert ${TORCH_LIBRARIES})


# davis decoder
add_library(decode_davis SHARED decode_davis.cpp convert.cpp)
target_compile_features(decode_davis PRIVATE cxx_std_14)
target_include_directories(decode_davis PRIVATE ${TORCH_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${LZ4_INCLUDE_DIR})
target_link_directories(decode_davis PRIVATE ${LIBCAER_LINK_DIRECTORIES} ${TORCH_LINK_DIRECTORIES} ${CMAKE_BINARY_DIR}/res)
target_link_libraries(decode_davis PRIVATE ${CAER_LIBRARY} ${TORCH_LIBRARIES} ${SDL2_LIBRARIES} ${Python3_LIBRARIES} ${LZ4_LIBRARY})

add_executable(decoder_dvs decoder_dvs.cpp)
target_include_directories(decoder_dvs PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(decoder_dvs PRIVATE decode_davis ${TORCH_LIBRARIES})
