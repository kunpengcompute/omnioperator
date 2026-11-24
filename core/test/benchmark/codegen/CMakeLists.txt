aux_source_directory(${CMAKE_CURRENT_LIST_DIR} CODEGEN_TESTS_LIST)
set(CODEGEN_TEST_TARGET bmcodgen)
add_library(${CODEGEN_TEST_TARGET} ${CODEGEN_TESTS_LIST})

find_package(benchmark REQUIRED)

set(BM_LINK benchmark::benchmark pthread gtest stdc++ dl)

# dependent library
target_link_libraries(${CODEGEN_TEST_TARGET} memory util type ${OMNI_CODEGEN_SO} ${OMNI_OPERATOR_SO})


add_executable(codegen_benchmark
        decimal.cpp
        )

target_include_directories(codegen_benchmark PRIVATE

        # core
        ${PROJECT_SOURCE_DIR}

        # src
        ${PROJECT_SOURCE_DIR}/src

        # secure c
        ${PROJECT_SOURCE_DIR}/../open_source
        )

target_link_libraries(codegen_benchmark PRIVATE
        ${BM_LINK}
        memory
        util
        type
        LLVM-15
        benchmark_main
        pthread
        jemalloc
        gtest
        ${OMNI_OPERATOR_SO}
        ${OMNI_CODEGEN_SO}
        ${OMNI_VECTOR_SO}
        )