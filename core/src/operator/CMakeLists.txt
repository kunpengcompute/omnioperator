file(GLOB_RECURSE OPERATOR_LIST ${CMAKE_CURRENT_LIST_DIR}/*.cpp ../simd/*.cpp util/*.cpp ../compute/*.cpp ../plannode/*.cpp ../connectors/*.cpp)

# SVE TU must not use the default march; exclude from glob and add conditionally below.
list(FILTER OPERATOR_LIST EXCLUDE REGEX "sum_int64_sve_hash_agg_sve\\.cpp$")

set(OP_TARGET ${OMNI_OPERATOR_SO})
find_package(nlohmann_json 3.7.3 REQUIRED)
# compile .a file

add_library(${OP_TARGET} SHARED ${OPERATOR_LIST})

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

#dependent library
target_link_libraries(${OP_TARGET} PUBLIC expression vectorization io_core ${OMNI_CODEGEN_SO} ${OMNI_VECTOR_SO} ${OMNI_READER_SO})
target_include_directories(${OP_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/simd)
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