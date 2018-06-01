# core engine
cmake_minimum_required(VERSION 3.0)
set (TARGET_NAME engine)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)


if ("${CMAKE_CXX_FLAGS}" STREQUAL "--std=c++")
    # user-defined C++ version
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    # default is c++11
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra  ")

FILE(GLOB SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/linux/*.cpp")

add_library(engine STATIC ${SOURCE_FILES})

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../include )


