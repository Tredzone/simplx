# util
cmake_minimum_required(VERSION 3.7.2)
set(TARGET_NAME timer)

include_directories(${SIMPLX_DIR}/include)

list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/timeractor.cpp
    )
    
add_library(${TARGET_NAME} STATIC ${SOURCE_FILES})

target_include_directories(${TARGET_NAME} INTERFACE ${SIMPLX_DIR}/include)

# re-export to parent
set(SOURCE_FILES ${SOURCE_FILES} PARENT_SCOPE)
