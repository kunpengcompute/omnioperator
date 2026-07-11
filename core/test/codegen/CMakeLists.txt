aux_source_directory(${CMAKE_CURRENT_LIST_DIR} CODEGEN_TESTS_LIST)
if(EXCLUDE_BATCH_FUNCTIONS)
    list(FILTER CODEGEN_TESTS_LIST EXCLUDE REGEX "/batch_")
endif()
set(CODEGEN_TEST_TARGET codegentest)
add_library(${CODEGEN_TEST_TARGET} ${CODEGEN_TESTS_LIST})

# dependent library
target_link_libraries(${CODEGEN_TEST_TARGET} memory util type ${OMNI_CODEGEN_SO} ${OMNI_OPERATOR_SO})
target_include_directories(${CODEGEN_TEST_TARGET} PUBLIC ${SOURCE_ROOT}/test)
target_include_directories(${CODEGEN_TEST_TARGET} PUBLIC ${SOURCE_ROOT}/src/util)