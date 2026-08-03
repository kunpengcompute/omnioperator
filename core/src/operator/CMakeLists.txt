# aggregation (aggregation/ + aggregation/aggregator/)
file(GLOB AGG_SRC ${CMAKE_CURRENT_LIST_DIR}/aggregation/*.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/aggregation/aggregator/*.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/aggregation/aggregator/regr/*.cpp)
# SVE TU must not use the default march; exclude from glob and add conditionally below.
list(FILTER AGG_SRC EXCLUDE REGEX "sum_int64_sve_hash_agg_sve\\.cpp$")
add_library(op_agg_obj OBJECT ${AGG_SRC})

# join (join/ + join/sortmergejoin/)
file(GLOB JOIN_SRC ${CMAKE_CURRENT_LIST_DIR}/join/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/join/sortmergejoin/*.cpp)
add_library(op_join_obj OBJECT ${JOIN_SRC})

# window
file(GLOB WINDOW_SRC ${CMAKE_CURRENT_LIST_DIR}/window/*.cpp)
add_library(op_window_obj OBJECT ${WINDOW_SRC})

# sort family (sort/ + topn/ + topnsort/)
file(GLOB SORT_SRC ${CMAKE_CURRENT_LIST_DIR}/sort/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/topn/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/topnsort/*.cpp)
add_library(op_sort_obj OBJECT ${SORT_SRC})

# spill
file(GLOB SPILL_SRC ${CMAKE_CURRENT_LIST_DIR}/spill/*.cpp)
add_library(op_spill_obj OBJECT ${SPILL_SRC})

# hashmap + limit
file(GLOB HASH_LIMIT_SRC ${CMAKE_CURRENT_LIST_DIR}/hashmap/*.cpp
                         ${CMAKE_CURRENT_LIST_DIR}/limit/*.cpp)
add_library(op_hashlimit_obj OBJECT ${HASH_LIMIT_SRC})

# misc: operator root + remaining sub-modules
file(GLOB MISC_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/config/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/expand/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/filter/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/grouping/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/projection/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/tablescan/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/union/*.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/unnest/*.cpp)
add_library(op_misc_obj OBJECT ${MISC_SRC})

set(OP_TARGET ${OMNI_OPERATOR_SO})
find_package(nlohmann_json 3.7.3 REQUIRED)

add_library(${OP_TARGET} SHARED
    $<TARGET_OBJECTS:op_agg_obj>
    $<TARGET_OBJECTS:op_join_obj>
    $<TARGET_OBJECTS:op_window_obj>
    $<TARGET_OBJECTS:op_sort_obj>
    $<TARGET_OBJECTS:op_spill_obj>
    $<TARGET_OBJECTS:op_hashlimit_obj>
    $<TARGET_OBJECTS:op_misc_obj>
    $<TARGET_OBJECTS:simd_obj>
    $<TARGET_OBJECTS:compute_obj>
    $<TARGET_OBJECTS:plannode_obj>
)

# Spark sum(bigint) HashAgg SVE kernel: Linux aarch64 only, compile with +sve.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*)")
  set(SUM_INT64_SVE_SRC
      ${CMAKE_CURRENT_LIST_DIR}/aggregation/aggregator/sum_int64_sve_hash_agg_sve.cpp)
  target_sources(${OP_TARGET} PRIVATE ${SUM_INT64_SVE_SRC})
  set_source_files_properties(
    ${SUM_INT64_SVE_SRC}
    PROPERTIES COMPILE_OPTIONS "-march=armv8-a+sve")
endif()

# precompiled headers: declared on the shared lib target for CMake consumption
# but not applied to OBJECT library sources — avoids GCC 10 ICE with arm_sve.h
target_precompile_headers(${OP_TARGET} PRIVATE
        <vector/vector.h>
        <operator/operator.h>
        <operator/operator_factory.h>
        <type/data_type.h>
)

# dependent libraries
target_link_libraries(${OP_TARGET} PUBLIC
    expression vectorization io_core
    -Wl,--whole-archive connectors -Wl,--no-whole-archive
    ${OMNI_CODEGEN_SO} ${OMNI_VECTOR_SO} ${OMNI_READER_SO})
target_include_directories(${OP_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/simd)

# install
install(TARGETS ${OP_TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${SOURCE_ROOT}/src/operator/operator.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/operator_factory.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/execution_context.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/memory_builder.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/status.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/config/operator_config.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/config)
install(FILES ${SOURCE_ROOT}/src/operator/hash_util.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/util/function_type.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/util/)
install(FILES ${SOURCE_ROOT}/src/operator/util/operator_util.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/util/)
install(FILES ${SOURCE_ROOT}/src/operator/projection/projection.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/projection/)
install(FILES ${SOURCE_ROOT}/src/operator/omni_id_type_vector_traits.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/filter/filter_and_project.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/filter)
install(FILES ${SOURCE_ROOT}/src/operator/window/window_frame.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/window/)
install(FILES ${SOURCE_ROOT}/src/operator/window/window_function_options.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/window/)
file(GLOB METRICS_HEAD_FILES ${SOURCE_ROOT}/src/metrics/*.h)
install(FILES ${METRICS_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/metrics)