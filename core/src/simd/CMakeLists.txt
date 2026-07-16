include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# OBJECT library to avoid duplicate compilation when linked into operator.so
file(GLOB_RECURSE SIMD_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
add_library(simd_obj OBJECT ${SIMD_SRC})
target_include_directories(simd_obj PUBLIC ${CMAKE_CURRENT_LIST_DIR})