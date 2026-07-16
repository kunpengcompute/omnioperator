file(GLOB_RECURSE PLAN_NODE_LIST ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
# OBJECT library: .o files are linked into operator.so to avoid duplicate compilation
add_library(plannode_obj OBJECT ${PLAN_NODE_LIST})
file(GLOB PLAN_HEAD_FILES ${SOURCE_ROOT}/src/plannode/*.h)
install(FILES ${PLAN_HEAD_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/plannode)