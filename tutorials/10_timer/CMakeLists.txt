# single tuto
cmake_minimum_required(VERSION 3.0)
set(TARGET_NAME tuto_timer)

set(SOURCE_FILES timer.cpp)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} timer)
