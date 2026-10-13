aux_source_directory(${CMAKE_CURRENT_LIST_DIR} MEM_POOL_LIST)
set(MEM_TARGET memory)
add_library(${MEM_TARGET} ${MEM_POOL_LIST})

# dependent include
target_include_directories(${MEM_TARGET} PUBLIC /usr/local/include/jemalloc/)

# ${CMAKE_DL_LIBS}: dladdr() for the OMNI_MEM_DEBUG mem_probe in-process RA resolver.
# Empty on glibc >= 2.34 (dladdr is in libc), "-ldl" otherwise; benign in release builds.
target_link_libraries(${MEM_TARGET} PUBLIC jemalloc ${CMAKE_DL_LIBS})