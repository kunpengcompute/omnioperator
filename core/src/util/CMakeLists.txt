file(GLOB_RECURSE UTIL_LIST ${CMAKE_CURRENT_LIST_DIR}/*cpp)
file(GLOB_RECURSE UTIL_HEADERS ${CMAKE_CURRENT_LIST_DIR}/*.h)
add_library(util ${UTIL_LIST} ${UTIL_HEADERS})
# dependent include
target_include_directories(util PUBLIC $ENV{JAVA_HOME}/include)
target_include_directories(util PUBLIC $ENV{JAVA_HOME}/include/linux)
target_link_libraries(util PRIVATE boundscheck)
