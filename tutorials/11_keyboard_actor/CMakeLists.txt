# single tuto
cmake_minimum_required(VERSION 3.0)
set(TARGET_NAME keyboard)

set(SOURCE_FILES keyboard.cpp)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} engine)
