include_directories(SYSTEM "/user/local/include")
file(GLOB_RECURSE READER_LIST
        ${CMAKE_CURRENT_LIST_DIR}/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/*.cc
)
set(READER_TARGET ${OMNI_READER_SO})

#Find required protobuf package
find_package(Protobuf REQUIRED)
if (PROTOBUF_FOUND)
    message(STATUS "protobuf library found")
else ()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif ()

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(${READER_TARGET} SHARED ${READER_LIST} ${PROTO_SRCS} ${PROTO_HDRS} ${PROTO_SRCS_VB} ${PROTO_HDRS_VB})

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(ArrowDataset REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(JNI REQUIRED)

# dependent include
target_include_directories(${READER_TARGET} PUBLIC ${JAVA_INCLUDE_PATH})
target_include_directories(${READER_TARGET} PUBLIC ${JAVA_INCLUDE_PATH2})
target_include_directories(${READER_TARGET} PUBLIC $ENV{JAVA_HOME}/include)
target_include_directories(${READER_TARGET} PUBLIC $ENV{JAVA_HOME}/include/linux)
target_link_libraries(${READER_TARGET} PUBLIC ${JAVA_JVM_LIBRARY})

# from command 'llvm-config-15 --cxxflags --ldflags --libs'
target_compile_options(${READER_TARGET} PRIVATE -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
target_link_libraries(${READER_TARGET} PRIVATE boundscheck)
target_link_libraries(${READER_TARGET} PUBLIC
        Arrow::arrow_shared
        ArrowDataset::arrow_dataset_shared
        Parquet::parquet_shared
        nlohmann_json::nlohmann_json
        orc
        hdfs
        z
        snappy
        lz4
        vectorization
        z-ng)
# z-ng:reader.cpp 间接 include zlib-ng.h(通过 Arrow/Parquet),源码内 deflate/inflate
# 被 macro 替换为 zng_deflate/zng_inflate,必须显式 link libz-ng 解析 symbols。
# 否则 runtime 撞 UnsatisfiedLinkError: undefined symbol: zng_deflate(known-issues #16)。
install(TARGETS ${READER_TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX})

file(GLOB READER_HEAD_FILES  ${SOURCE_ROOT}/src/reader/*.h)
file(GLOB READER_ORC_HEAD_FILES
        ${SOURCE_ROOT}/src/reader/orc/*.h
        ${SOURCE_ROOT}/src/reader/orc/*.hh)
file(GLOB READER_COMMON_HEAD_FILES
        ${SOURCE_ROOT}/src/reader/common/*.h
        ${SOURCE_ROOT}/src/reader/common/*.hh)
file(GLOB READER_PARQUET_HEAD_FILES
        ${SOURCE_ROOT}/src/reader/parquet/*.h
        ${SOURCE_ROOT}/src/reader/parquet/*.hh)
file(GLOB READER_TEXT_HEAD_FILES
        ${SOURCE_ROOT}/src/reader/text/*.h
        ${SOURCE_ROOT}/src/reader/text/*.hh)

file(GLOB READER_FILE_HEAD_FILES  ${SOURCE_ROOT}/src/filesystem/*.h)
install(FILES ${READER_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader)
install(FILES ${READER_ORC_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader/orc)
install(FILES ${READER_FILE_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader/filesystem)
install(FILES ${READER_COMMON_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader/common)
install(FILES ${READER_PARQUET_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader/parquet)
install(FILES ${READER_TEXT_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/reader/text)
