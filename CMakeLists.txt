# required cmake version
cmake_minimum_required(VERSION 3.14.1)

#project name
project(omni-runtime)

message(STATUS "Now processor is ${CMAKE_HOST_SYSTEM_PROCESSOR}")

# omniruntime version
set(OMNI_RUNTIME_VERSION 2.2.0)
set(OS_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
set(OMNI_CODEGEN_SO boostkit-omniop-codegen-${OMNI_RUNTIME_VERSION}-${OS_ARCHITECTURES})
set(OMNI_OPERATOR_SO boostkit-omniop-operator-${OMNI_RUNTIME_VERSION}-${OS_ARCHITECTURES})
set(OMNI_RUNTIME_SO boostkit-omniop-runtime-${OMNI_RUNTIME_VERSION}-${OS_ARCHITECTURES})
set(OMNI_VECTOR_SO boostkit-omniop-vector-${OMNI_RUNTIME_VERSION}-${OS_ARCHITECTURES})
set(OMNI_READER_SO boostkit-omniop-reader-${OMNI_RUNTIME_VERSION}-${OS_ARCHITECTURES})

# configure cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++")

# this is used to generate LLVM IR for jitting
set(IR_COMPILER "clang++-15")
message(STATUS "IR_COMPILER : ${IR_COMPILER}")

if (DEFINED COVERAGE)
    if (${COVERAGE} STREQUAL "ON")
        set(DEBUG_COMPILE_LEVEL "-O2 -fsanitize=address -fsanitize-recover=address,all -static-libasan")
        if (DEFINED ENABLE_DT AND ENABLE_DT STREQUAL "ON")
            set(DEBUG_COMPILE_LEVEL "-fsanitize=address -fsanitize-coverage=trace-pc")
        endif ()
    else ()
        set(DEBUG_COMPILE_LEVEL "-O0")
    endif ()
elseif(DEFINED TRACE)
    set(DEBUG_COMPILE_LEVEL "-O3 -s")
else ()
    set(DEBUG_COMPILE_LEVEL "-O0")
endif ()

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif (CCACHE_FOUND)

find_package(re2 REQUIRED)
find_package(fmt REQUIRED)

# SVE vs NEON gate for the omni SIMD layer.
# omni hard-coded -march=armv8-a+crc+sve, and core/src/simd/targets.h selects
# OMNI_SVE_256 unless the COVERAGE macro is defined (then OMNI_NEON). Apple
# Silicon (the Mac dev env) implements no SVE: SVE instructions SIGILL at runtime
# (seen in omniruntime::reader::BitNotFlip) and the SVE-only SIMD headers fail to
# compile. The build runs in a container sharing the host kernel, so /proc/cpuinfo
# reflects the real CPU. When the host lacks SVE, drop +sve AND define COVERAGE so
# simd/targets.h picks the NEON path. A 区 Kunpeng has SVE and keeps it.
# (Defining COVERAGE here does NOT add gcov instrumentation — those flags live
# only in CMAKE_CXX_FLAGS_DEBUG; release builds use CMAKE_CXX_FLAGS_RELEASE.)
set(OMNI_USE_SVE FALSE)
set(OMNI_USE_SVE2 FALSE)
if (NOT (DEFINED COVERAGE AND COVERAGE STREQUAL "ON"))
    execute_process(COMMAND grep -qw sve2 /proc/cpuinfo RESULT_VARIABLE OMNI_SVE2_RC)
    if (OMNI_SVE2_RC EQUAL 0)
        set(OMNI_USE_SVE2 TRUE)
        set(OMNI_USE_SVE TRUE)
    else ()
        execute_process(COMMAND grep -qw sve /proc/cpuinfo RESULT_VARIABLE OMNI_SVE_RC)
        if (OMNI_SVE_RC EQUAL 0)
            set(OMNI_USE_SVE TRUE)
        endif ()
    endif ()
endif ()
if (OMNI_USE_SVE2)
    set(SIMD_COMPILE_LEVEL "-march=armv8.3-a+crc+sve2 -fpermissive")
elseif (OMNI_USE_SVE)
    set(SIMD_COMPILE_LEVEL "-march=armv8.3-a+crc+sve -fpermissive")
else ()
    add_definitions(-DCOVERAGE)
    set(SIMD_COMPILE_LEVEL "-march=armv8-a+crc -fpermissive")
endif ()
message(STATUS "OmniOperator SIMD: SVE=${OMNI_USE_SVE} SVE2=${OMNI_USE_SVE2} flags=${SIMD_COMPILE_LEVEL}")

#-ffast-math
#-fopt-info-vec-optimized
set(CMAKE_CXX_FLAGS_DEBUG "${DEBUG_COMPILE_LEVEL} ${SIMD_COMPILE_LEVEL} -fno-var-tracking-assignments -pipe -g -Wall -fPIC -fno-omit-frame-pointer -fno-common -fno-stack-protector -ftest-coverage -fprofile-arcs")
if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${SIMD_COMPILE_LEVEL} -fno-var-tracking-assignments -pipe -Wall -Wtrampolines -D_FORTIFY_SOURCE=2 -fPIC -finline-functions -fstack-protector-strong -s -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-var-tracking-assignments -pipe -Wall -Wtrampolines -D_FORTIFY_SOURCE=2 -fPIC -finline-functions -fstack-protector-strong -s -Wl,-z,noexecstack -Wl,-z,relro,-z,now")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -finline-limit=6000 --param inline-unit-growth=300")

if(ENABLE_COMPILE_TIME_REPORT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-report")
endif()

set(CMAKE_VERBOSE_MAKEFILE OFF)

# StringView is an opt-in native feature. Keep it disabled in the default build so
# downstream users that do not enable it do not compile its dedicated functions and tests.
option(STRINGVIEW_ENABLE "Build native StringView support" OFF)
if (STRINGVIEW_ENABLE)
    add_compile_definitions(STRINGVIEW_ENABLE)
endif ()
message(STATUS "Option STRINGVIEW_ENABLE: ${STRINGVIEW_ENABLE}")

# PDQSort / inplace PDQSort / TimSort are opt-in at compile time. When OFF, their
# headers and call sites are excluded and runtime flags are forced off.
option(OMNI_ENABLE_EXPERIMENTAL_SORT
       "Compile PDQSort / inplace PDQSort / TimSort support" OFF)
if (OMNI_ENABLE_EXPERIMENTAL_SORT)
    add_compile_definitions(OMNI_ENABLE_EXPERIMENTAL_SORT)
endif ()
message(STATUS "Option OMNI_ENABLE_EXPERIMENTAL_SORT: ${OMNI_ENABLE_EXPERIMENTAL_SORT}")

if (EXISTS $ENV{OMNI_HOME})
    set(CMAKE_INSTALL_PREFIX $ENV{OMNI_HOME}/lib)
else ()
    set(CMAKE_INSTALL_PREFIX /opt/lib)
endif ()

message(STATUS "Build by ${CMAKE_BUILD_TYPE}")
message(STATUS "OMNI_HOME is $ENV{OMNI_HOME}")
message(STATUS "JAVA_HOME is $ENV{JAVA_HOME}")

# clean all target
set(target_generated ${CMAKE_INSTALL_PREFIX}/lib${OMNI_VECTOR_SO}.so)
if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(target_generated ${CMAKE_INSTALL_PREFIX}/lib${OMNI_RUNTIME_SO}.so
            ${CMAKE_INSTALL_PREFIX}/lib${OMNI_CODEGEN_SO}.so
            ${CMAKE_INSTALL_PREFIX}/ir
            ${CMAKE_INSTALL_PREFIX}/lib${OMNI_OPERATOR_SO}.so
            ${CMAKE_INSTALL_PREFIX}/lib${OMNI_READER_SO}.so
            )
endif ()
foreach (file ${target_generated})
    if (EXISTS ${file})
        file(REMOVE_RECURSE ${file})
        message(STATUS "Delete file or directory:${file}")
    endif ()
endforeach (file)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} ROOT_SRCS)
# for header searching
include_directories(SYSTEM core/src)
include_directories(SYSTEM core)
include_directories(${CMAKE_INSTALL_PREFIX}/lib/APL/include)

# OMNI_MEM_DEBUG: mem_probe memory-accounting diagnostics (REVERTIBLE; OFF in release).
# Allocator-boundary instrumentation — covered vs pool-less traffic keyed by call-site
# return address (dladdr) + jemalloc ground truth + per-task pool dtor residual.
# Replaces the deleted TLS MemoryTrace/ThreadMemoryTrace leak detector.
# Enable with:  cmake -DOMNI_MEM_DEBUG=ON   (defines -DOMNI_MEM_DEBUG for core + bindings).
option(OMNI_MEM_DEBUG "Enable mem_probe memory-accounting diagnostics (allocator coverage + jemalloc stats + per-task pool residual)" OFF)
if (OMNI_MEM_DEBUG)
    add_definitions(-DOMNI_MEM_DEBUG)
endif ()
message(STATUS "Option OMNI_MEM_DEBUG: ${OMNI_MEM_DEBUG}")

# Join probe/table validation guards (per-Find checks added during spill bring-up).
# OFF for release/performance builds — the hot path must stay check-free. Debug
# builds and -DOMNI_JOIN_PROBE_VALIDATION=ON keep the diagnostics.
option(OMNI_JOIN_PROBE_VALIDATION "Enable join probe/table validation guards (debug diagnostics)" OFF)
if (OMNI_JOIN_PROBE_VALIDATION OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DOMNI_JOIN_PROBE_VALIDATION)
endif ()
message(STATUS "Option OMNI_JOIN_PROBE_VALIDATION: ${OMNI_JOIN_PROBE_VALIDATION}")

# compile library
add_subdirectory(core)

add_subdirectory(bindings/java/src/main/cpp EXCLUDE_FROM_ALL)


# options
option(DEBUG "Debug" OFF)
message(STATUS "Option DEBUG: ${DEBUG}")

option(TRACE "Trace" OFF)
message(STATUS "Option TRACE: ${TRACE}")

option(DEBUG_OPERATOR "Using for debug into native operator code" OFF)
message(STATUS "Option DEBUG_OPERATOR: ${DEBUG_OPERATOR}")

option(DEBUG_VECTOR "Using for debug into native vector code" OFF)
message(STATUS "Option DEBUG_VECTOR: ${DEBUG_VECTOR}")

option(DEBUG_LLVM "Using for debug into JIT and codegen code" OFF)
message(STATUS "Option DEBUG_LLVM: ${DEBUG_LLVM}")

option(DISABLE_JIT "Disable OmniJit to optimize operator" OFF)
message(STATUS "Option DISABLE_JIT: ${DISABLE_JIT}")

option(COVERAGE "Enable coverage to optimize operator" OFF)
message(STATUS "Option Enable COVERAGE: ${COVERAGE}")

option(COVERAGE "Enable DT" OFF)
message(STATUS "Option Enable DT: ${ENABLE_DT}")

option(ENABLE_BENCHMARK "Enable Benchmark" OFF)
message(STATUS "Option Enable Benchmark: ${ENABLE_BENCHMARK}")
