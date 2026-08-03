file(GLOB_RECURSE VECTORIZATION_LIST ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
set(VECTORIZATION_TARGET vectorization)
add_library(${VECTORIZATION_TARGET} ${VECTORIZATION_LIST})

find_package(fmt REQUIRED)
find_package(double-conversion REQUIRED)
find_package(folly REQUIRED)
find_package(gflags REQUIRED)
if(NOT TARGET gflags::gflags)
    # This is a bit convoluted, but we want to be able to use gflags::gflags as a
    # target even when omni is built as a subproject which uses
    # `find_package(gflags)` which does not create a globally imported target that
    # we can ALIAS.
    add_library(gflags_gflags INTERFACE)
    target_link_libraries(gflags_gflags INTERFACE gflags)
    add_library(gflags::gflags ALIAS gflags_gflags)
endif()

target_precompile_headers(${VECTORIZATION_TARGET} PRIVATE
    <vector/vector.h>
    <vector/vector_helper.h>
    <type/data_type.h>
)

target_link_libraries(${VECTORIZATION_TARGET} PRIVATE util expression udf re2::re2 fmt::fmt double-conversion::double-conversion Folly::folly gflags::gflags)
file(GLOB EXPRESSION_HEAD_FILES ${SOURCE_ROOT}/src/vectorization/*.h)
install(FILES ${EXPRESSION_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/vectorization)

file(GLOB REGISTRATION_HEAD_FILES ${SOURCE_ROOT}/src/vectorization/registration/*.h)
install(FILES ${REGISTRATION_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/vectorization/registration)