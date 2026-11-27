aux_source_directory(${CMAKE_CURRENT_LIST_DIR} OP_TESTS_LIST)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/sortings_coverage OP_SORTINGS_COVERAGE_TESTS)

# Keep the regular test build complete while allowing the join-specific gate
# to avoid unrelated aggregator test compilation.
option(ENABLE_AGG_TEST "Build aggregator operator tests" ON)
if(ENABLE_AGG_TEST)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/aggregator OP_AGG_TEST_TARGET)
endif()

set(OP_TEST_TARGET optest)
add_library(${OP_TEST_TARGET} ${OP_TESTS_LIST} ${OP_AGG_TEST_TARGET} ${OP_SORTINGS_COVERAGE_TESTS})

# dependent library
target_link_libraries(${OP_TEST_TARGET} memory type ${OMNI_CODEGEN_SO} ${OMNI_OPERATOR_SO} ${OMNI_VECTOR_SO})
target_include_directories(${OP_TEST_TARGET} PRIVATE ${SOURCE_ROOT}/src/util ${SOURCE_ROOT}/src/operator/algorithm ${SOURCE_ROOT}/test)
