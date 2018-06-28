# util
cmake_minimum_required(VERSION 3.0)
set (TARGET_NAME timer)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

add_library(timer STATIC timeractor.cpp)
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/trz/util/timer/)
target_link_libraries(${TARGET_NAME} engine)
