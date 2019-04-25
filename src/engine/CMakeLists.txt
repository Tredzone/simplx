# core engine
cmake_minimum_required(VERSION 3.0)
set (TARGET_NAME engine)

include_directories(${SIMPLX_DIR}/include ${SIMPLX_DIR})

list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/actor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/RefMapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/linux/platform_gcc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/linux/platform_linux.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/parallel/parallel_xplat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/e2e_stub.cpp                    # (body will be noped when e2e enabled)
    )

add_library(${TARGET_NAME} STATIC ${SOURCE_FILES})

target_include_directories(${TARGET_NAME} INTERFACE ${SIMPLX_DIR}/include )

# re-export to parent
set(SOURCE_FILES ${SOURCE_FILES} PARENT_SCOPE)


