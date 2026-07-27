file(GLOB_RECURSE OPERATOR_LIST ${CMAKE_CURRENT_LIST_DIR}/*.cpp ../simd/*.cpp util/*.cpp ../compute/*.cpp ../plannode/*.cpp ../connectors/*.cpp)

set(OP_TARGET ${OMNI_OPERATOR_SO})
find_package(nlohmann_json 3.7.3 REQUIRED)
# compile .a file

add_library(${OP_TARGET} SHARED ${OPERATOR_LIST})

enable_language(ASM)
target_sources(${OP_TARGET} PRIVATE hashmap/crc32_X4.S)

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
install(FILES ${SOURCE_ROOT}/src/operator/aggregation/udaf/udaf_registry.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/aggregation/udaf)
install(FILES ${SOURCE_ROOT}/src/operator/util/function_type.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/util/)
install(FILES ${SOURCE_ROOT}/src/operator/util/operator_util.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/util/)
install(FILES ${SOURCE_ROOT}/src/operator/projection/projection.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/projection/)
install(FILES ${SOURCE_ROOT}/src/operator/omni_id_type_vector_traits.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator)
install(FILES ${SOURCE_ROOT}/src/operator/filter/filter_and_project.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/filter)
install(FILES ${SOURCE_ROOT}/src/operator/window/window_frame.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/window/)
install(FILES ${SOURCE_ROOT}/src/operator/window/window_function_options.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/operator/window/)
file(GLOB METRICS_HEAD_FILES ${SOURCE_ROOT}/src/metrics/*.h)
install(FILES ${METRICS_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/metrics)