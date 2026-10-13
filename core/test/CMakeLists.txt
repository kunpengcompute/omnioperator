aux_source_directory(${CMAKE_CURRENT_LIST_DIR} TEST_ROOT_SRCS)
include_directories(/usr/lib/llvm-15/include)

if (DEFINED ENABLE_DT AND ENABLE_DT STREQUAL "ON")
    add_subdirectory(util)
    add_subdirectory(dt)
else ()
    # configure
    add_subdirectory(compute)
    add_subdirectory(operator)
    add_subdirectory(plannode)
    add_subdirectory(util)
    add_subdirectory(vector)
    add_subdirectory(function)
    add_subdirectory(memory)
    add_subdirectory(vectorization)
    add_subdirectory(io)
    add_subdirectory(reader/tablescan)
    add_subdirectory(reader/filesystem)
    add_subdirectory(reader/io/arrowadapter)
    add_subdirectory(reader/io/orcfile)
if (DEFINED ENABLE_BENCHMARK AND ENABLE_BENCHMARK STREQUAL "ON")
    find_package(benchmark REQUIRED)
    add_subdirectory(benchmark)
endif ()
    add_subdirectory(parser)
    add_subdirectory(type)
    add_subdirectory(codegen)

    set(OM_TEST_TARGET omtest)
    set(MY_LINK
            optest
            testutil
            vectest
            memtest
            functest
            parsertest
            typetest
            codegentest
            planNodeTest
            computeTest
            vectorizationTest
            ioTest
            tablescantest
            filesystemtest
            arrowadaptertest
            orcfiletest
            )

    # find gtest package
    find_package(GTest REQUIRED)
    find_package(folly REQUIRED)
    find_package(gflags REQUIRED)

    # Fast, isolated cache ownership check. The regular omtest target also
    # contains this source, but linking it requires every unrelated test suite.
    add_executable(bhjCacheTest EXCLUDE_FROM_ALL
        operator/broadcast_hash_table_cache_test.cpp)
    target_link_libraries(bhjCacheTest PRIVATE
        ${GTEST_BOTH_LIBRARIES}
        memory type ${OMNI_CODEGEN_SO} ${OMNI_OPERATOR_SO} ${OMNI_VECTOR_SO}
        gtest gmock pthread stdc++ dl LLVM-15 re2::re2 Folly::folly z-ng)
    target_include_directories(bhjCacheTest PRIVATE
        ${SOURCE_ROOT}/src/util ${SOURCE_ROOT}/src/operator/algorithm ${SOURCE_ROOT}/test)

    # compile a executable file
    add_executable(${OM_TEST_TARGET} ${ROOT_SRCS} ${TEST_ROOT_SRCS})
    # dependent libraries
    target_link_libraries(${OM_TEST_TARGET}
            ${GTEST_BOTH_LIBRARIES}
            ${SOURCE_LINK}
            -Wl,--whole-archive
            ${MY_LINK}
            -Wl,--no-whole-archive
            gtest
            gmock
            pthread
            stdc++
            dl
            LLVM-15
            re2::re2
            Folly::folly
            gcov
            z-ng
    $<$<BOOL:${ENABLE_BENCHMARK}>:benchmark::benchmark>)
    target_compile_options(${OM_TEST_TARGET} PUBLIC -g -O2 -fPIC)

    # dependent include
    target_include_directories(${OM_TEST_TARGET} PRIVATE {GTEST_INCLUDE_DIRS})

    # discover tests
    gtest_discover_tests(${OM_TEST_TARGET} DISCOVERY_TIMEOUT 600)
endif ()
