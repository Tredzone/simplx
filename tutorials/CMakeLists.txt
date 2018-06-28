# common tuto cmake
cmake_minimum_required(VERSION 3.0)
set (TARGET_NAME tuto_common)

MESSAGE( STATUS "current dir " ${CMAKE_CURRENT_SOURCE_DIR} )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -pthread")

# should loop through tuto subdir ?
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/01_hello_actor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/02_hello_world)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/03_printer_actor_starter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/04_printer_actor_service)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/05_multi_callback)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/06_undelivered_event_management)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/07_referenced_unreferenced_actor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/08_pingpong)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/09_sync_exit)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/10_timer)
