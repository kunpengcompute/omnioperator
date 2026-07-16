include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# OBJECT library to avoid duplicate compilation when linked into operator.so
file(GLOB COMPUTE_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
add_library(compute_obj OBJECT ${COMPUTE_SRC})
target_include_directories(compute_obj PUBLIC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB COMPUTE_FILES ${SOURCE_ROOT}/src/compute/*.h)
install(FILES ${COMPUTE_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/compute)
