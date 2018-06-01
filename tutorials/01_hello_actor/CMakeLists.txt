# single tuto
cmake_minimum_required(VERSION 3.0)
set(TARGET_NAME hello_actor)

set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/hello_actor.cpp)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} engine)

